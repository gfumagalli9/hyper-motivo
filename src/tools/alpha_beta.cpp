// MIT License
//
// Alpha–beta decomposition scorer for hypergraphs (Motivo).
// Given a Hypergraph H, compute the "best alpha" (hyperedge-size threshold)
// following the C reference algorithm: we enumerate all incidences (v,e),
// build tuples (t = |e|, p = rank of e in v’s incident list ordered by |e| desc),
// then scan tuples sorted by (t desc, p desc) to accumulate:
//   - beta(t) = max_{processed} (p+1)
//   - ie_cost(t) += (2^(p+1) - 2^p) for p >= 2   [saturating arithmetic]
//
// Finally, total_cost(alpha) = naive_cost(alpha) + ie_cost(alpha),
// where naive_cost(alpha) = floor(0.01 * sum_{|e|<=alpha} |e|^2).
//
// Returns the alpha (a hyperedge size) minimizing total_cost.

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>
#include <stdexcept>

#include "../common/graph/Hypergraph.h"

namespace motivo {

// -------------------- helpers: saturating arithmetic --------------------
static inline std::size_t sat_add(std::size_t a, std::size_t b) {
    const std::size_t M = std::numeric_limits<std::size_t>::max();
    if (a > M - b) return M;
    return a + b;
}

static inline std::size_t pow2_sat(unsigned k) {
    const unsigned bits = static_cast<unsigned>(sizeof(std::size_t) * 8U);
    if (k >= bits) return std::numeric_limits<std::size_t>::max();
    return (std::size_t{1} << k);
}

// -------------------- main routine --------------------
std::size_t compute_best_alpha(const Hypergraph& H, double naive_decay = 0.01)
{
    using vertex_t = Hypergraph::vertex_t;
    using edge_t   = Hypergraph::edge_t;

    const edge_t m = H.number_of_hyperedges();
    const vertex_t n = H.number_of_vertices();
    if (m == 0 || n == 0) return 0; // degenerate

    // Precompute edge sizes
    std::vector<std::uint32_t> esize(m);
    std::size_t total_inc = 0;
    for (edge_t e = 0; e < m; ++e) {
        esize[e] = H.hyperedge_size(e);
        total_inc += esize[e];
    }
    if (total_inc == 0) return 0;

    // Build per-vertex incidence lists, sorted by edge size DESC (tie-break by e id DESC)
    std::vector<std::vector<edge_t>> inc(n);
    inc.shrink_to_fit();
    for (vertex_t v = 0; v < n; ++v) {
        inc[v].reserve(H.vertex_degree(v));
    }
    for (edge_t e = 0; e < m; ++e) {
        const std::uint32_t sz = esize[e];
        for (std::uint32_t i = 0; i < sz; ++i) {
            vertex_t v = H.hyperedge_vertex(e, i);
            inc[v].push_back(e);
        }
    }
    for (vertex_t v = 0; v < n; ++v) {
        auto& L = inc[v];
        std::sort(L.begin(), L.end(), [&](edge_t a, edge_t b){
            if (esize[a] != esize[b]) return esize[a] > esize[b]; // larger first
            return a > b;                                         // tie-break
        });
    }

    // Build the (t,p) list for all incidences
    struct TP { std::size_t t; std::size_t p; };
    std::vector<TP> tp;
    tp.reserve(total_inc);
    for (vertex_t v = 0; v < n; ++v) {
        const auto& L = inc[v];
        for (std::size_t p = 0; p < L.size(); ++p) {
            const edge_t e = L[p];
            tp.push_back(TP{ static_cast<std::size_t>(esize[e]),
                             static_cast<std::size_t>(p) });
        }
    }

    // Sort (t,p) by t DESC then p DESC (exactly like the C code)
    std::sort(tp.begin(), tp.end(), [](const TP& a, const TP& b){
        if (a.t != b.t) return a.t > b.t;
        return a.p > b.p;
    });
    if (tp.empty()) return 0;

    // Scan blocks of equal t to compute alpha candidates, beta and ie_cost
    std::vector<std::size_t> alpha;   alpha.reserve(64);
    std::vector<std::size_t> beta;    beta.reserve(64);
    std::vector<std::size_t> ie_cost; ie_cost.reserve(64);

    std::size_t maxB = 1;  // beta so far
    std::size_t ie    = 0; // inclusion–exclusion cost so far

    for (std::size_t i = 0; i < tp.size();) {
        const std::size_t t = tp[i].t;

        // Update maxB with p+1 over the upcoming block
        std::size_t j = i;
        while (j < tp.size() && tp[j].t == t) {
            const std::size_t p1 = tp[j].p + 1;
            if (p1 > maxB) maxB = p1;
            ++j;
        }

        // Now accumulate IE delta for this block:
        // for each oldp >= 2: add (2^(oldp+1) - 2^oldp) with saturation
        for (std::size_t k = i; k < j; ++k) {
            const std::size_t oldp = tp[k].p;
            if (oldp > 1) {
                const std::size_t a = pow2_sat(static_cast<unsigned>(oldp + 1));
                const std::size_t b = pow2_sat(static_cast<unsigned>(oldp));
                const std::size_t delta = (a >= b ? a - b : 0); // safe even if both are SIZE_MAX
                ie = sat_add(ie, delta);
            }
        }

        alpha.push_back(t);
        beta .push_back(maxB);
        ie_cost.push_back(ie);

        i = j; // next block
    }

    // Build histogram of edge sizes to compute naive prefix cost fast
    std::size_t max_t = 0;
    for (edge_t e = 0; e < m; ++e) {
        if (esize[e] > max_t) max_t = esize[e];
    }
    std::vector<std::size_t> count_by_size(max_t + 1, 0);
    for (edge_t e = 0; e < m; ++e) ++count_by_size[esize[e]];

    // Prefix of sum_{s<=t} (s*s * count[s])
    std::vector<std::size_t> naive_prefix(max_t + 1, 0);
    std::size_t run = 0;
    for (std::size_t s = 0; s <= max_t; ++s) {
        // s*s can overflow size_t if s is huge, but hyperedge sizes are <= degree of an edge;
        // in practice safe; still use saturation.
        std::size_t term = s * s;
        if (s != 0 && term / s != s) term = std::numeric_limits<std::size_t>::max(); // fallback
        // multiply by count[s] with saturation
        std::size_t contrib = 0;
        for (std::size_t k = 0; k < count_by_size[s]; ++k) {
            contrib = sat_add(contrib, term);
        }
        run = sat_add(run, contrib);
        naive_prefix[s] = run;
    }

    // Pick best alpha (iterate alpha values in ascending order of t)
    // Our 'alpha' vector is in DESC order; traverse from the end.
    const std::size_t nA = alpha.size();
    if (nA == 0) return 0;

    std::size_t best_alpha = alpha.back();
    std::size_t best_cost  = std::numeric_limits<std::size_t>::max();

    for (std::size_t idx = nA; idx-- > 0; ) {
        const std::size_t a  = alpha[idx];
        const std::size_t iec = ie_cost[idx];
        const std::size_t naive_sum = (a <= max_t ? naive_prefix[a] : naive_prefix.back());

        // naive_cost = floor(0.01 * naive_sum) — keep the original behavior
        const std::size_t naive_cost =
            static_cast<std::size_t>(naive_decay * static_cast<long double>(naive_sum));

        const std::size_t total = sat_add(iec, naive_cost);
        if (total < best_cost) {
            best_cost  = total;
            best_alpha = a;
        }
    }

    return best_alpha;
}

} // namespace motivo