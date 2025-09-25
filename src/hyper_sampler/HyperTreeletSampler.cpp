#include "HyperTreeletSampler.h"
#include <vector>
#include <algorithm>

// --------- TLS workspace for common_incident_count (O(deg) mark array) ----------
namespace {
    thread_local std::vector<uint32_t> tls_last_seen_edge;
    thread_local uint32_t              tls_visit_id = 1;
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

    // Sum over LOW neighbors v of u:
    //   mass += CtabG(u, parent) * StabG(v, split_child)
    for (uint32_t i = 0; i < deg; ++i) {
        const auto v = G_low_->neighbor(u, i);
        for (auto it = tb.StabG->begin(v, split); !it.is_over(); ++it) {
            const Treelet& t2 = it.treelet();
            if (t2.get_structure() != split.get_structure()) break;    // structure range exhausted
            if (t2.get_colors() & ~t.get_colors()) continue;           // color mismatch

            const Treelet parent = t.complement(t2);
            const uint64_t cG    = (uint64_t) tb.CtabG->get_count(u, parent);
            const uint64_t cnt   = (uint64_t) it.count();
            if (cG && cnt) W += cG * cnt;
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

    // Sum over HIGH "NWS" entries t2 at root u:
    //   mass += CtabG(u, parent) * NWSh(u, split_child)
    for (auto it = tb.NWSh->begin(u, split); !it.is_over(); ++it) {
        const Treelet& t2 = it.treelet();
        if (t2.get_structure() != split.get_structure()) break; // structure range exhausted
        if (t2.get_colors() & ~t.get_colors()) continue;        // color mismatch

        const Treelet parent = t.complement(t2);
        const uint64_t cG    = (uint64_t) tb.CtabG->get_count(u, parent);
        const uint64_t cnt   = (uint64_t) it.count();
        if (cG && cnt) W += cG * cnt;
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

    // Step 2: draw a vertex x connected to u in HIGH consistent with chosen t2.
    // We sample an incident hyperedge of u, then a vertex x in it, with local
    // weight ~ StabG(x, split_child), and accept with probability 1/m(u,x),
    // where m is the number of common incident hyperedges of u and x.
    const uint32_t degH = H_large_->vertex_degree(u);
    if (degH == 0) return false;

    std::vector<uint64_t> edgeW(degH, 0);
    uint64_t Wtot = 0;

    for (uint32_t i = 0; i < degH; ++i) {
        const uint32_t e  = H_large_->incident_hyperedge(u, i);
        uint64_t w = 0;
        const uint32_t sz = H_large_->hyperedge_size(e);
        for (uint32_t j = 0; j < sz; ++j) {
            const auto x = H_large_->hyperedge_vertex(e, j);
            if (x == u) continue;

            // Find counts of chosen split-child t2 rooted at x
            for (auto jt = tb.StabG->begin(x, split); !jt.is_over(); ++jt) {
                const Treelet& t2x = jt.treelet();
                if (t2x.get_structure() != split.get_structure()) break;
                if (t2x != out.child) continue;
                w += (uint64_t) jt.count();
                break;
            }
        }
        edgeW[i] = w; Wtot += w;
    }
    if (Wtot == 0) return false;

    // Try acceptance-corrected selection a few times.
    static constexpr int MAX_HIGH_TRIES = 64;
    for (int tries = 0; tries < MAX_HIGH_TRIES; ++tries) {
        uint64_t rE = rng->random_uint<uint64_t>(0, Wtot - 1);
        uint32_t ie = 0;
        for (; ie < degH; ++ie) {
            if (!edgeW[ie]) continue;
            if (rE >= edgeW[ie]) rE -= edgeW[ie]; else break;
        }
        if (ie == degH) continue;

        const uint32_t e  = H_large_->incident_hyperedge(u, ie);
        uint64_t rl       = rng->random_uint<uint64_t>(0, edgeW[ie] - 1);
        vertex_t x_sel    = u;
        const uint32_t sz = H_large_->hyperedge_size(e);

        for (uint32_t j = 0; j < sz; ++j) {
            const auto x = H_large_->hyperedge_vertex(e, j);
            if (x == u) continue;

            uint64_t wx = 0;
            for (auto jt = tb.StabG->begin(x, split); !jt.is_over(); ++jt) {
                const Treelet& t2x = jt.treelet();
                if (t2x.get_structure() != split.get_structure()) break;
                if (t2x != out.child) continue;
                wx = (uint64_t) jt.count();
                break;
            }
            if (!wx) continue;

            if (rl >= wx) rl -= wx;
            else { x_sel = x; break; }
        }
        if (x_sel == u) continue;

        const uint32_t m = common_incident_count(u, x_sel);
        if (!m) continue;

        const bool accept = (rng->random_uint<uint32_t>(1, m) == 1);
        if (accept) { out.child_v = x_sel; break; }
    }

    if (out.child_v == (vertex_t)-1) {
        // Fallback: proportional to sum_e cnt(x,t2)/m(u,x)
        std::vector<std::pair<vertex_t,uint64_t>> cand;
        uint64_t Wdir = 0;

        for (uint32_t i = 0; i < degH; ++i) {
            const uint32_t e  = H_large_->incident_hyperedge(u, i);
            const uint32_t sz = H_large_->hyperedge_size(e);
            for (uint32_t j = 0; j < sz; ++j) {
                const auto x = H_large_->hyperedge_vertex(e, j);
                if (x == u) continue;

                uint64_t wx = 0;
                for (auto jt = tb.StabG->begin(x, split); !jt.is_over(); ++jt) {
                    const Treelet& t2x = jt.treelet();
                    if (t2x.get_structure() != split.get_structure()) break;
                    if (t2x != out.child) continue;
                    wx = (uint64_t) jt.count(); break;
                }
                if (!wx) continue;

                const uint32_t m = common_incident_count(u, x);
                if (!m) continue;

                const uint64_t w = wx / (uint64_t)m;
                if (!w) continue;

                cand.emplace_back(x, w);
                Wdir += w;
            }
        }
        if (Wdir == 0) return false;

        uint64_t rdir = rng->random_uint<uint64_t>(0, Wdir - 1);
        for (auto &p : cand) {
            if (rdir >= p.second) rdir -= p.second;
            else { out.child_v = p.first; break; }
        }
        if (out.child_v == (vertex_t)-1) return false;
    }

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