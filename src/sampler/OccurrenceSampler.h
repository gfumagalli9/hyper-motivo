//
// Created by steven on 9/11/17.
//

#ifndef MOTIVO_OCCURRENCESAMPLER_H
#define MOTIVO_OCCURRENCESAMPLER_H

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <limits>
#include <sparsehash/dense_hash_map>
#include "../common/random/Random.h"
#include "../common/graph/UndirectedGraph.h"
#include "Occurrence.h"
#include "SpanningTreeCounter.h"
#include "TreeletSampler.h"
#include "DynamicSequencer.h"
#include "SampleTable.h"

class OccurrenceSampler
{
private:
	typedef google::dense_hash_map<Occurrence, uint64_t, Occurrence::OccurrenceFootprintHash, Occurrence::OccurrenceFootprintEquality> occ_count_table_t;
    typedef DynamicSequencer<uint64_t> sequencer_t;

    class ThreadSync
	{
	private:
    	const unsigned int number_of_threads;
		unsigned int terminated_threads = 0;
		std::mutex mutex;
		std::condition_variable all_threads_terminated;

    public:
	    explicit ThreadSync(unsigned int nthreads) : number_of_threads(nthreads)
        {}

        ThreadSync(ThreadSync&) = delete; //Deleted copy constructor

        void terminate()
        {
	        std::unique_lock lock(mutex);
	        if(++terminated_threads==number_of_threads)
	            all_threads_terminated.notify_one();
        }

        void wait_timeout(double timeout_seconds)
        {
            std::unique_lock lock(mutex);
            all_threads_terminated.wait_for(lock, std::chrono::duration<double>(timeout_seconds), [this] {return terminated_threads==number_of_threads; } );
        }

        void wait()
        {
            std::unique_lock lock(mutex);
            all_threads_terminated.wait(lock, [this] {return terminated_threads==number_of_threads; } );
        }
	};

    const UndirectedGraph *graph;
	const TreeletTableCollection *ttc;
	const unsigned int size;

	const bool vertices;
	const bool graphlets;
	const bool canonicize;
	const bool no_rejection;

	TreeletSampler sampler;

    SpanningTreeCounter* spanning_tree_counter;
    TreeletSelector *sp_counter_selector = nullptr;
    bool delete_stc = true;

	void sample_thread [[gnu::hot, gnu::flatten]](occ_count_table_t *table, sequencer_t *sequencer, Random *rng, std::atomic<bool> &terminate_flag, ThreadSync &sync);

	SampleTable *build_sample_table(const occ_count_table_t &count_tab) const;


public:
	inline void sample_one [[gnu::hot]] (Occurrence *occurrence, Random *rng)
	{
		UndirectedGraph::vertex_t sampled_vertices[16];
		UndirectedGraph::vertex_t root = sampler.sample_root(rng);
		assert(root < graph->number_of_vertices());
		Treelet t = sampler.sample_treelet(root, rng);

		static thread_local OccurrenceCanonicizer canonicizer(size);


		while (true)
		{
			if (vertices || graphlets) //If we want treelets but not the occurrence vertices we can skip sampling
			{
#ifndef NDEBUG
				bool success =
#endif
						sampler.sample_rooted_occurrence(t, root, sampled_vertices, rng); //FIXME: Handle case in which there are no treelets
				assert(success);
			}

			if (graphlets)
			{
				new(occurrence) Occurrence(size, graph, sampled_vertices);

				if (!no_rejection && rng->random_uint<uint64_t>(0, occurrence->number_of_spanning_trees() - 1) != 0)
					continue; //Rejection
			}
			else
				new(occurrence) Occurrence(t, sampled_vertices);

			if(canonicize)
				canonicizer.canonicize(occurrence);

			break;
		}
	}

	SampleTable* sample(uint64_t n_samples, unsigned int number_of_threads, Random *rng, double time_budget = std::numeric_limits<double>::infinity());

	OccurrenceSampler(const UndirectedGraph *graph, const TreeletTableCollection* ttc, unsigned int size,
                                         bool vertices, bool graphlets, bool canonicize, bool no_rejection) :
            graph(graph), ttc(ttc), size(size), vertices(vertices), graphlets(graphlets), canonicize(canonicize),
            no_rejection(no_rejection), sampler(graph, ttc, size)
    {
    	spanning_tree_counter = new SpanningTreeCounter();
    }

    void setSpanningTreeCounter(SpanningTreeCounter* stc)
    {
    	if (delete_stc)
    	{
    		delete spanning_tree_counter;
    		delete_stc = false;
    	}
    	spanning_tree_counter = stc;
    }

    ~OccurrenceSampler()
    {
    	if (delete_stc)
    		delete spanning_tree_counter;
    }

    /**
     * Set the treelet selector.
     * - the first specifies the treelets to be used for sampling the graphlets.
     * - the second specified the treelets to be used to count the spanning trees of
     *   the graphlets, including all their subtrees. This means that you want this
     *   second selector to contain all the treelets of the first, plus their subtrees,
     *   if the first selector is in INCLUDE mode. By default, it uses the first selector
     *   again, which is correct if in EXCLUDE mode.
     */
    void set_selector(const TreeletSelector *selector, unsigned int number_of_threads, const TreeletSelector *sp_counter_selector = nullptr);
};

#endif //MOTIVO_OCCURRENCESAMPLER_H
