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
#include "../common/treelets/Treelet.h"
#include "../common/treelets/TreeletStructureSelector.h"
#include "Occurrence.h"
#include "TreeletSampler.h"
#include "DynamicSequencer.h"
#include "SampleTable.h"
#include "SpanningTreeCounter.h"

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
	TreeletStructureSelector *sample_selector = nullptr;
	TreeletStructureSelector *build_selector = nullptr;
	SpanningTreeCounter *spanning_tree_counter = nullptr;

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

				if(!no_rejection)
                {
					if(build_selector)
					{
						if(rng->random_uint<uint64_t>(1, spanning_tree_counter->number_of_spanning_trees(*occurrence)) != 1)
							continue; //Rejection
					}
					else
					{
						const uint64_t sts = spanning_tree_counter->number_of_spanning_trees(*occurrence);
						assert(sts % size == 0);
						if(rng->random_uint<uint64_t>(1,sts) > size)
							continue; //Rejection
					}
                }

			}
			else
				new(occurrence) Occurrence(t, sampled_vertices);

			if(canonicize)
				canonicizer.canonicize(occurrence);

			break;
		}
	}

	SampleTable* sample(uint64_t n_samples, unsigned int number_of_threads, Random *rng, double time_budget = std::numeric_limits<double>::infinity());

	OccurrenceSampler(const UndirectedGraph *graph, const TreeletTableCollection* ttc, unsigned int size, bool vertices, bool graphlets, bool canonicize, bool no_rejection);

    ~OccurrenceSampler();

    ///@param sample_selector contains all the structures that we are interested in sampling. Only the ones of the correct size are considered
    ///@param build_selector contains all the structures used for the build phase
    ///For both selectors, nullptr means that all treelets are included
	void set_selector(const TreeletStructureSelector *new_sample_selector,  const TreeletStructureSelector *new_build_selector, unsigned int number_of_threads);
};

#endif //MOTIVO_OCCURRENCESAMPLER_H
