#ifndef NEIGHBOR_COMPUTE_FUNCTIONAL_H
#define NEIGHBOR_COMPUTE_FUNCTIONAL_H

#include <memory>
#include <tbb/tbb.h>
#include <iostream>

#include "AABBQuery.h"
#include "Index1D.h"
#include "NeighborList.h"
#include "NeighborQuery.h"

namespace freud { namespace locality {

template<typename T> T* makeNewEmptyArray(unsigned int size)
{
    T* tmp = new T[size];
    memset((void*) tmp, 0, sizeof(T) * size);
    return tmp;
}

template <typename T>
class ETSArrayWrapper 
{
public:
    tbb::enumerable_thread_specific<T *> array;

    ETSArrayWrapper(): 
        m_size(0), array(tbb::enumerable_thread_specific<T*>([](){return nullptr;})) 
        {}

    void update(unsigned int size) 
    {
        m_size = size;
        deleteArray();
        array = tbb::enumerable_thread_specific<T*>([this] ()
            {
                return makeNewEmptyArray<T>(m_size);
            });
    }

    ETSArrayWrapper(unsigned int size):
        m_size(size), array(tbb::enumerable_thread_specific<T*>([this] ()
            {
                return makeNewEmptyArray<T>(m_size);
            }))
        {}

    ~ETSArrayWrapper()
    {
        deleteArray();
    }

private:
    void deleteArray()
    {
        for (auto i = array.begin(); i != array.end(); ++i)
        {
            delete[](*i);
        }
    }

    unsigned int m_size;
};

//! Wrapper for for-loop
/*! \param parallel If true, run body in parallel.
    \param begin Beginning index.
    \param end Ending index.
    \param body Body should be an object taking in 
           with operator(size_t begin, size_t end).
*/
template<typename Body> void for_loop_wrapper(bool parallel, size_t begin, size_t end, const Body& body)
{
    if (parallel)
    {
        tbb::parallel_for(tbb::blocked_range<size_t>(begin, end),
                          [&body](const tbb::blocked_range<size_t>& r) { body(r.begin(), r.end()); });
    }
    else
    {
        body(begin, end);
    }
}

//! Wrapper iterating looping over NeighborQuery
/*! \param ref_points NeighborQuery object to iterate over
    \param points Points
    \param Np Number of points
    \param qargs Query arguments
    \param nlist Neighbor List. If not NULL, loop over it. Otherwise, use ref_points
           appropriately with given qargs.
    \param cf A void function that takes 
           (ref_point_index, point_index, distance, weight) as input.
*/
template<typename ComputePairType>
void loop_over_NeighborList(const NeighborQuery* ref_points, const vec3<float>* points, unsigned int Np,
                            QueryArgs qargs, const NeighborList* nlist, const ComputePairType& cf)
{
    // check if nlist exists
    if (nlist != NULL)
    {
        // if nlist exists, loop over it in parallel.
        loop_over_NeighborList_parallel(nlist, cf);
    }
    else
    {
        // if nlist does not exist, check if ref_points is an actual NeighborQuery
        std::shared_ptr<NeighborQueryIterator> iter;
        std::shared_ptr<AABBQuery> abq;
        // check if ref_points is a pointer to a RawPoints object
        // dynamic_cast will fail if ref_points is not actually pointing to RawPoints
        // and return a null pointer. Then, the assignment operator will return
        // a null pointer, making the condition in the if statement to be false.
        // This is a typical C++ way of checking the type of a polymorphic class
        // using pointers and casting.
        if (const RawPoints* rp = dynamic_cast<const RawPoints*>(ref_points))
        {
            // if ref_points is RawPoints, build a NeighborQuery
            abq = std::make_shared<AABBQuery>(ref_points->getBox(), ref_points->getRefPoints(),
                                              ref_points->getNRef());
            iter = abq.get()->queryWithArgs(points, Np, qargs);
        }
        else
        {
            iter = ref_points->queryWithArgs(points, Np, qargs);
        }

        // iterate over the query object in parallel
        for_loop_wrapper(true, 0, Np, [&iter, &qargs, &cf](size_t begin, size_t end) {
            NeighborPoint np;
            for (size_t i = begin; i != end; ++i)
            {
                std::shared_ptr<NeighborQueryIterator> it = iter->query(i);
                np = it->next();
                while (!it->end())
                {
                    if (!qargs.exclude_ii || i != np.ref_id)
                    {
                        // TODO when Voronoi gets incorporated in NeighborQuery infrastructure
                        // weight set to 1 for now
                        cf(np.ref_id, i, np.distance, 1);
                    }
                    np = it->next();
                }
            }
        });
    }
}

//! Wrapper iterating looping over NeighborList in parallel.
/*! \param nlist Neighbor List to loop over.
    \param cf A void function that takes 
           (ref_point_index, point_index, distance, weight) as input.
*/
template<typename ComputePairType>
void loop_over_NeighborList_parallel(const NeighborList* nlist, const ComputePairType& cf)
{
    const size_t* neighbor_list(nlist->getNeighbors());
    size_t n_bonds = nlist->getNumBonds();
    const float* neighbor_distances = nlist->getDistances();
    const float* neighbor_weights = nlist->getWeights();
    parallel_for(tbb::blocked_range<size_t>(0, n_bonds), [=](const tbb::blocked_range<size_t>& r) {
        for (size_t bond = r.begin(); bond != r.end(); ++bond)
        {
            size_t i(neighbor_list[2 * bond]);
            size_t j(neighbor_list[2 * bond + 1]);
            cf(i, j, neighbor_distances[bond], neighbor_weights[bond]);
        }
    });
}

}; }; // end namespace freud::locality

#endif
