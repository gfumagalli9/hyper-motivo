// MIT License
#include "HyperMultithreadedBuilder.h"
#include <cassert>
#include <vector>

HyperMultithreadedBuilder::HyperMultithreadedBuilder(
    const Hypergraph*                 H,
    V                                 from_vertex,
    V                                 to_vertex,
    const unsigned int                size,
    const TreeletTableCollection*     ttc,
    const TreeletTableCollection*     tIEc,
    const bool                        store_only_0,
    const TreeletStructureSelector*   selector,
    std::ostream*                     output,
    const unsigned int                nthreads,
    const bool                        normalize
)
  : H(H)
  , from_vertex(from_vertex)
  , to_vertex(to_vertex)
  , size(size)
  , ttc(ttc)
  , tIEc(tIEc)
  , store_only_0(store_only_0)
  , output(output)
  , builder(size, ttc, tIEc, selector) // same ctor used in HyperSequentialBuilder
  , normalize(normalize)
  , nthreads(nthreads)
{
    assert(H != nullptr);
    assert(output != nullptr);
    assert(size >= 2);
    assert(ttc != nullptr && tIEc != nullptr);
    assert(from_vertex <= to_vertex);
    assert(nthreads >= 1);
}

void HyperMultithreadedBuilder::build()
{
    // 1) Header: number of vertices
    const V n = H->number_of_vertices();
    output->write(reinterpret_cast<const char*>(&n), sizeof(V));

    // 2) Concurrent writer (bounded buffer)
    auto* writer = new ConcurrentWriter(output, 100 * nthreads);

    // 3) Start worker threads
    next_vertex = from_vertex;

    std::vector<std::thread> pool;
    pool.reserve(nthreads);
    for (unsigned t = 0; t < nthreads; ++t) {
        pool.emplace_back([this, t, writer]() {
            this->thread_loop(t, writer);
        });
    }

    // 4) Join and cleanup
    for (auto& th : pool) th.join();
    delete writer;
}

void HyperMultithreadedBuilder::thread_loop(const unsigned int /*thread_no*/,
                                            ConcurrentWriter* writer)
{
    ColorCodingHashmap table; // local accumulator per thread

    while (true) {
        // Fetch next vertex atomically
        V u = next_vertex.fetch_add(1, std::memory_order_relaxed);
        if (u > to_vertex) break;

        // Respect "store only color-0 roots" policy (color 0 == bitmask 1)
        if (!store_only_0 ||
            ttc->get_table(1)->begin(u).treelet().get_colors() == 1)
        {
            builder.combine(u, table);
        }

        // Serialize and issue to the concurrent writer
        auto payload = builder.to_normalized_sorted_byte_array(u, table, normalize);
        table.clear();

        writer->write(payload.first, payload.second);
        //delete[] payload.first;
    }
}