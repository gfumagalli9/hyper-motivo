#include "HyperTreeletSampler.h"
#include <vector>
#include <algorithm>

// --------- TLS workspace for common_incident_count (O(deg) mark array) ----------
namespace {
    thread_local std::vector<uint32_t> tls_last_seen_edge;
    thread_local uint32_t              tls_visit_id = 1;

    // TLS mark array for HIGH neighbors deduplication (per choose_high_ call).
    // We reuse an epoch counter to avoid clearing.
    thread_local std::vector<uint32_t> tls_seen_vertex_high;
    thread_local uint32_t              tls_seen_vertex_epoch = 1;
}

// -------------------- Ctors ----------------------------------------------------

HyperTreeletSampler::HyperTreeletSampler(const UndirectedGraph *G_low,
                                         const Hypergraph *H_large,
                                         const TreeletTableCollection *ttc_root,
                                         const TreeletTableCollection *nws_high,
                                         unsigned int size)
: G_low_(G_low), H_large_(H_large),
  ttc_root_(ttc_root), nws_high_(nws_high), k_(size)
{}

// -------------------- Internal helpers ----------------------------------------

bool HyperTreeletSampler::get_tables_(uint32_t kT, uint32_t kS, uint32_t kP, SamplerTables& out) const {
    out.TtabG = ttc_root_ ? ttc_root_->get_table(kT) : nullptr;
    out.CtabG = ttc_root_ ? ttc_root_->get_table(kP) : nullptr;
    out.StabG = ttc_root_ ? ttc_root_->get_table(kS) : nullptr; // LOW view of child
    out.NWSh  = nws_high_ ? nws_high_->get_table(kS)  : nullptr; // HIGH-only child table
    return (out.TtabG && out.CtabG && out.StabG);
}

uint64_t HyperTreeletSampler::compute_low_mass_(vertex_t u,
                                                const Treelet& t,
                                                const Treelet& split,
                                                const SamplerTables& tb) const
{
    if (!G_low_ || !tb.StabG || !tb.CtabG) return 0;

    uint64_t W = 0;
    const uint32_t deg = G_low_->degree(u);
    if (deg == 0) return 0;

    // --- Micro-optimizations -------------------------------------------------
    // 1) Read split structure & T colors once.
    // 2) Use a tiny flat cache keyed by child color mask to avoid recomputing
    //    CtabG(u, parent) across multiple neighbors v. For a fixed (u,T,split)
    //    the parent Treelet depends only on the child's colors, not on 'v'.
    //    A flat vector with linear probe is typically faster than unordered_map
    //    for the small cardinalities we see here.
    const auto split_structure = split.get_structure();
    using color_t = decltype(t.get_colors());
    const color_t T_colors = t.get_colors();

    std::vector<std::pair<color_t, uint64_t>> cG_cache;
    cG_cache.reserve(32);

    // Sum over LOW neighbors v of u:
    //   mass += CtabG(u, parent) * StabG(v, split_child)
    for (uint32_t i = 0; i < deg; ++i) {
        const auto v = G_low_->neighbor(u, i);

        for (auto it = tb.StabG->begin(v, split); !it.is_over(); ++it) {
            const Treelet& t2 = it.treelet();

            // Iterator is grouped by structure; once we leave 'split' structure we can stop.
            if (t2.get_structure() != split_structure) break;

            const color_t C2 = t2.get_colors();
            // Skip if t2 colors are incompatible with T.
            if (C2 & ~T_colors) continue;

            // NEW: read count first; if zero, skip expensive parent/cG work.
            const uint64_t cnt = static_cast<uint64_t>(it.count());
            if (!cnt) continue;

            // Lookup or compute cG = CtabG(u, parent(T \ t2))
            uint64_t cG = 0;
            bool found = false;
            for (const auto& kv : cG_cache) {
                if (kv.first == C2) { cG = kv.second; found = true; break; }
            }
            if (!found) {
                const Treelet parent = t.complement(t2);               // depends on colors only
                cG = static_cast<uint64_t>(tb.CtabG->get_count(u, parent));
                cG_cache.emplace_back(C2, cG);
            }
            if (!cG) continue; // early skip if no parent mass at u

            W += cG * cnt;
        }
    }
    return W;
}

uint64_t HyperTreeletSampler::compute_high_mass_(vertex_t u,
                                                 const Treelet& t,
                                                 const Treelet& split,
                                                 const SamplerTables& tb) const
{
    if (!tb.NWSh || !tb.CtabG) return 0;

    uint64_t W = 0;

    // NEW: mirror LOW micro-opts (cache cG by child color mask; read cnt first).
    const auto split_structure = split.get_structure();
    using color_t = decltype(t.get_colors());
    const color_t T_colors = t.get_colors();

    // Small per-call cache: (child color mask) -> cG(u, parent)
    std::vector<std::pair<color_t, uint64_t>> cG_cache;
    cG_cache.reserve(32);

    // Sum over HIGH "NWS" entries t2 at root u:
    //   mass += CtabG(u, parent) * NWSh(u, split_child)
    for (auto it = tb.NWSh->begin(u, split); !it.is_over(); ++it) {
        const Treelet& t2 = it.treelet();
        if (t2.get_structure() != split.get_structure()) break; // structure range exhausted

        const color_t C2 = t2.get_colors();
        if (C2 & ~T_colors) continue;                           // color mismatch

        // NEW: read count first; skip if zero.
        const uint64_t cnt = static_cast<uint64_t>(it.count());
        if (!cnt) continue;

        // Lookup/compute cG once per color mask.
        uint64_t cG = 0;
        bool found = false;
        for (const auto& kv : cG_cache) {
            if (kv.first == C2) { cG = kv.second; found = true; break; }
        }
        if (!found) {
            const Treelet parent = t.complement(t2);
            cG = static_cast<uint64_t>(tb.CtabG->get_count(u, parent));
            cG_cache.emplace_back(C2, cG);
        }
        if (!cG) continue;

        W += cG * cnt;
    }
    return W;
}

bool HyperTreeletSampler::choose_low_(vertex_t u, const Treelet& t, const Treelet& split,
                                      uint64_t W_low, const SamplerTables& tb,
                                      DecompChoice& out, Random* rng) const
{
    if (!G_low_ || !tb.StabG) return false;

    uint64_t r = rng->random_uint<uint64_t>(0, W_low - 1);
    const uint32_t deg = G_low_->degree(u);

    for (uint32_t i = 0; i < deg; ++i) {
        const auto v = G_low_->neighbor(u, i);

        for (auto it = tb.StabG->begin(v, split); !it.is_over(); ++it) {
            const Treelet& t2 = it.treelet();
            if (t2.get_structure() != split.get_structure()) break;
            if (t2.get_colors() & ~t.get_colors()) continue;

            const Treelet parent = t.complement(t2);
            const uint64_t cG    = (uint64_t) tb.CtabG->get_count(u, parent);
            const uint64_t cnt   = (uint64_t) it.count();
            const uint64_t w     = cG * cnt;
            if (!w) continue;

            if (r >= w) { r -= w; continue; }

            out.parent  = parent;
            out.child   = t2;
            out.child_v = v;        // child root in LOW is a Gaifman neighbor
            return true;
        }
    }
    return false;
}

bool HyperTreeletSampler::choose_high_(vertex_t u, const Treelet& t, const Treelet& split,
                                       uint64_t W_high, const SamplerTables& tb,
                                       DecompChoice& out, Random* rng) const
{
    if (!H_large_ || !tb.NWSh || !tb.StabG) return false;

    // Step 1: draw the split child t2 ~ weight proportional to CtabG(u,parent) * NWSh(u, split).
    {
        uint64_t r = rng->random_uint<uint64_t>(0, W_high - 1);

        for (auto it = tb.NWSh->begin(u, split); !it.is_over(); ++it) {
            const Treelet& t2 = it.treelet();
            if (t2.get_structure() != split.get_structure()) break;
            if (t2.get_colors() & ~t.get_colors()) continue;

            const Treelet parent = t.complement(t2);
            const uint64_t cG    = (uint64_t) tb.CtabG->get_count(u, parent);
            const uint64_t cnt   = (uint64_t) it.count();
            const uint64_t w     = cG * cnt;
            if (!w) continue;

            if (r >= w) { r -= w; continue; }

            out.parent = parent;
            out.child  = t2;
            break;
        }
        if (!out.child.is_valid()) return false;
    }

    // Step 2 (optimized): directly sample a HIGH neighbor x of u with weight ~ StabG(x, out.child).
    // ------------------------------------------------------------------------------
    // Rationale: the original acceptance-corrected scheme (pick edge e, then x in e,
    // accept with prob 1/m(u,x)) yields a marginal over x proportional to cnt(x, out.child).
    // Proof sketch:
    //   P(select x before accept) ∝ Σ_{e∋u,x} [ cnt(x)/Σ_{y∈e\{u}} cnt(y) ].
    // After acceptance with 1/m(u,x), the multiplicity cancels and we get P(x) ∝ cnt(x).
    // Therefore we can sample *directly* over distinct HIGH neighbors x with weight cnt(x).
    // We deduplicate x across all hyperedges incident to u and query cnt via get_count().
    // ------------------------------------------------------------------------------

    const uint32_t degH = H_large_->vertex_degree(u);
    if (degH == 0) return false;

    // TLS mark array for dedup of candidate vertices x
    const uint32_t NV = H_large_->number_of_vertices();
    if (tls_seen_vertex_high.size() != NV) tls_seen_vertex_high.assign(NV, 0);
    const uint32_t vmark = ++tls_seen_vertex_epoch;
    if (tls_seen_vertex_epoch == 0) { // wrap-around protection
        std::fill(tls_seen_vertex_high.begin(), tls_seen_vertex_high.end(), 0);
        tls_seen_vertex_epoch = 1;
    }

    std::vector<std::pair<vertex_t, uint64_t>> cand;
    cand.reserve(64);
    uint64_t Wdir = 0;

    for (uint32_t i = 0; i < degH; ++i) {
        const uint32_t e  = H_large_->incident_hyperedge(u, i);
        const uint32_t sz = H_large_->hyperedge_size(e);
        for (uint32_t j = 0; j < sz; ++j) {
            const auto x = H_large_->hyperedge_vertex(e, j);
            if (x == u) continue;

            // Deduplicate x across all incident hyperedges of u
            if (tls_seen_vertex_high[x] == vmark) continue;
            tls_seen_vertex_high[x] = vmark;

            // Direct lookup: how many split-child occurrences rooted at x?
            const uint64_t wx = (uint64_t) tb.StabG->get_count(x, out.child);
            if (!wx) continue;

            cand.emplace_back(x, wx);
            Wdir += wx;
        }
    }

    if (Wdir == 0 || cand.empty()) return false;

    // Draw x ∼ weight wx
    uint64_t rdir = rng->random_uint<uint64_t>(0, Wdir - 1);
    for (auto &p : cand) {
        if (rdir >= p.second) rdir -= p.second;
        else {
            out.child_v = p.first;
            break;
        }
    }
    if (out.child_v == (vertex_t)-1) return false;

    return true;
}

bool HyperTreeletSampler::overlap_reject_half_(vertex_t u, vertex_t v, Random* rng) const {
    const bool in_low  = (G_low_   && G_low_->has_edge(u, v));
    const bool in_high = (H_large_ && common_incident_count(u, v) > 0);
    if (in_low && in_high) {
        // Prevent double counting when (u,v) belongs to both LOW and HIGH.
        return (rng->random_uint<uint32_t>(0,1) == 1);
    }
    return true;
}

// -------------------- Core: sample_rooted_occurrence ---------------------------

bool HyperTreeletSampler::sample_rooted_occurrence(const Treelet &t,
                                                   vertex_t u,
                                                   vertex_t *occ,
                                                   Random *rng)
{
    // ─────────────────────────────────────────────────────────────────────────
    // HOW THE LOW/HIGH SPLIT WORKS (high-level sketch)
    //
    // We want to sample an occurrence of a colored treelet T (size kT) rooted at u.
    // 1) Split T into two parts using the standard DP split:
    //      T = parent ⊕ child,   with kP = |parent| and kS = |child|,
    //    where 'split_child()' returns an UNCOLORED structure that selects the
    //    child shape; the DP complement 'parent = T.complement(child_colored)'.
    //
    // 2) Compute the total "mass" from LOW and HIGH:
    //    LOW  mass W_low  = Σ_v∈N_low(u)  CtabG(u,parent) * StabG(v, split_child)
    //    HIGH mass W_high = Σ_t2 in NWS(u) CtabG(u,parent) * NWSh(u, split_child)
    //
    //    - StabG / NWSh: counts of split-child occurrences (LOW view vs HIGH view).
    //    - CtabG(u,parent): how many ways the parent can be rooted at u.
    //
    // 3) Draw LOW/HIGH with probability proportional to W_low and W_high.
    // 4) If LOW: choose a Gaifman neighbor v and a child t2 at v with the proper
    //    weight, set child_v = v.
    //    If HIGH: choose child t2 from NWSh(u), then choose a vertex x that shares
    //    a hyperedge with u, weighted by StabG(x, t2), and accept with prob 1/m(u,x)
    //    where m is the number of common incident hyperedges (to correct bias).
    // 5) If (u, child_v) is both in LOW and HIGH, do a 1/2 rejection to avoid
    //    double counting.
    // 6) Recurse:
    //      - sample the parent at u, writing its vertices near the end of occ[]
    //      - sample the child at child_v, writing after occ[0]
    //    so that the final layout matches the canonical (preorder-like) split.
    // ─────────────────────────────────────────────────────────────────────────

    occ[0] = u;
    if (t.number_of_vertices() == 1) return true; // base case

    const uint32_t kT = t.number_of_vertices();

    // Split into child (kS) and parent (kP = kT - kS)
    Treelet split = t.split_child(); // uncolored structure
    assert(!split.is_colored());
    const uint32_t kS = split.number_of_vertices();
    const uint32_t kP = kT - kS;

    // Resolve tables for this split
    SamplerTables tb;
    if (!get_tables_(kT, kS, kP, tb)) return false;

    // Root guard: T must exist rooted at u
    const uint64_t root_cntG = (uint64_t)tb.TtabG->get_count(u, t);
    if (root_cntG == 0) return false;

    // Compute partition masses
    const uint64_t W_low  = compute_low_mass_(u, t, split, tb);
    const uint64_t W_high = compute_high_mass_(u, t, split, tb);
    if (W_low + W_high == 0) return false;

    // Choose LOW or HIGH
    const bool pick_high = (rng->random_uint<uint64_t>(0, W_low + W_high - 1) >= W_low);

    // Pick decomposition (and child vertex if HIGH)
    DecompChoice choice;
    const bool ok_choose = pick_high
        ? choose_high_(u, t, split, W_high, tb, choice, rng)
        : choose_low_(u,  t, split, W_low,  tb, choice, rng);
    if (!ok_choose) return false;

    // 1/2 rejection if the edge belongs to both LOW and HIGH
    if (!overlap_reject_half_(u, choice.child_v, rng)) return false;

    // Recurse: parent first (placed at the end), then child (after occ[0])
    if (!choice.parent.is_singleton()) {
        if (!sample_rooted_occurrence(choice.parent, u,
                                      occ + choice.child.number_of_vertices(), rng))
            return false;
    }
    if (!sample_rooted_occurrence(choice.child, choice.child_v, occ + 1, rng))
        return false;

    return true;
}

// -------------------- Utils ---------------------------------------------------

uint32_t HyperTreeletSampler::common_incident_count(vertex_t v, vertex_t u) const {
    if (!H_large_) return 0;
    const uint32_t M = H_large_->number_of_hyperedges();
    if (tls_last_seen_edge.size() != M) tls_last_seen_edge.assign(M, 0);

    const uint32_t mark = ++tls_visit_id;          // monotonic visit id
    if (tls_visit_id == 0) {                       // wrap-around safety
        std::fill(tls_last_seen_edge.begin(), tls_last_seen_edge.end(), 0);
        tls_visit_id = 1;
    }

    const uint32_t dv = H_large_->vertex_degree(v);
    for (uint32_t i = 0; i < dv; ++i) {
        const uint32_t e = H_large_->incident_hyperedge(v, i);
        tls_last_seen_edge[e] = mark;
    }

    uint32_t m = 0;
    const uint32_t du = H_large_->vertex_degree(u);
    for (uint32_t i = 0; i < du; ++i) {
        const uint32_t e = H_large_->incident_hyperedge(u, i);
        if (tls_last_seen_edge[e] == mark) ++m;
    }
    return m;
}