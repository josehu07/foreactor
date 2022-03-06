#include <iostream>
#include <vector>
#include <type_traits>
#include <string.h>
#include <assert.h>


#ifndef __FOREACTOR_VALUE_POOL_H__
#define __FOREACTOR_VALUE_POOL_H__


namespace foreactor {


class NodeAndEpoch;     // forward declarations
class SCGraph;
class SyscallNode;
class BranchNode;


// An EpochList is a n-dimensional "tuple" of loop indices. If an SCGraph
// has n back-pointing edges, then it is associated with n-dimensional Epochs
// to index any ValuePool created for that SCGraph. Each back-pointing edge
// corresponds to one index value in the tuple, and anytime it is traversed
// that index value increments.
// 
// This is the base class for auto-resolving of template argument.
class EpochListBase {
    friend class NodeAndEpoch;
    friend class SCGraph;
    friend class SyscallNode;
    friend class BranchNode;

    private:
        const unsigned max_dims = 0;

        static EpochListBase *New(unsigned max_dims);
        static EpochListBase *Copy(EpochListBase *epoch);
        static void Delete(EpochListBase *epoch);
        
    public:
        EpochListBase() = delete;
        EpochListBase(unsigned max_dims);
        virtual ~EpochListBase() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const EpochListBase& e);

        // Compare epoch numbers with another instance.
        virtual bool IsSame(const EpochListBase *epoch) const;

        // Every child class must implement these interfaces.
        // The base class implementation of these methods provide
        // auto-resolving of D.
        virtual size_t GetEpoch(int dim_idx) const;
        virtual void IncrementEpoch(int dim_idx);

        // Epoch numbers are cleared at the exit of hijacked function.
        virtual void ClearEpochs();
};

// Template argument D specifies the max #dims, i.e., number of all loops
// in SCGraph.
template <unsigned D>
class EpochList final : public EpochListBase {
    private:
        size_t epochs[D];

    public:
        EpochList();
        ~EpochList() {}

        template <unsigned E>
        friend std::ostream& operator<<(std::ostream& s,
                                        const EpochList<E>& e);

        bool IsSame(const EpochList<D> *epoch) const;

        size_t GetEpoch(int dim_idx) const;
        void IncrementEpoch(int dim_idx);

        void ClearEpochs();
};


// Value pool is an n-dimensional vector used to hold argument values across
// loops in SCGraph. 0-dim pool is a single scalar, 1-dim pool is a vector
// holding values that change only across one loop, 2-dim pool is a vector
// of vectors holding values that change across two nested loops, etc.
//
// This is the base class for auto-resolving of template arguments.
template <typename T>
class ValuePoolBase {
    friend class SyscallNode;

    private:
        const unsigned max_dims = 0;
        const unsigned num_dims = 0;

        static ValuePoolBase<T> *New(unsigned max_dims, unsigned num_dims,
                                     const std::vector<int>& dim_idx);
        static void Delete(ValuePoolBase<T> *pool);

    public:
        ValuePoolBase() = delete;
        ValuePoolBase(unsigned max_dims, unsigned num_dims);
        virtual ~ValuePoolBase() {}

        template <typename U>
        friend std::ostream& operator<<(std::ostream& s,
                                        const ValuePoolBase<U>& p);

        // Every child class must implement these interfaces.
        // The base class implementations of these methods provide
        // auto-resolving of MaxD and NumD.
        virtual bool GetReady(EpochListBase *epoch) const;
        virtual T& GetValue(EpochListBase *epoch);

        // Value might be set at the entrance of hijacked function, or at
        // an argument installation action, or by CheckArgs() of a SyscallNode
        // when it is actually invoked. If ready_ not given, assumes all data
        // points are valid and ready.
        virtual void SetValue(EpochListBase *epoch, const T& value);
        virtual void SetValueBatch(const T& data_);
        virtual void SetValueBatch(const T& data_,
                                   const bool& ready_);
        virtual void SetValueBatch(const std::vector<T>& data_);
        virtual void SetValueBatch(const std::vector<T>& data_,
                                   const std::vector<bool>& ready_);
        virtual void SetValueBatch(const std::vector<std::vector<T>>& data_);
        virtual void SetValueBatch(const std::vector<std::vector<T>>& data_,
                                   const std::vector<std::vector<bool>>& ready_);

        // Values are cleared at the exit of hijacked function.
        virtual void ClearValues(bool do_delete = false);
};

// Template argument MaxD specifies the max #dims, i.e., number of all
// loops in SCGraph. All EpochLists used to index values in this pool must
// have #dims == MaxD.
// 
// Template argument NumD specifies #dims that indexes me, <= max_dims.
template <typename T, unsigned MaxD, unsigned NumD>
class ValuePool final : public ValuePoolBase<T> {
    // NumD decides what should the data field storage be like.
    typedef std::conditional_t<NumD == 0, T,
            std::conditional_t<NumD == 1, std::vector<T>,
                                          std::vector<std::vector<T>>>> DataT;
            // higher dims may be added if necessary
    typedef std::conditional_t<NumD == 0, bool,
            std::conditional_t<NumD == 1, std::vector<bool>,
                                          std::vector<std::vector<bool>>>> ReadyT;
            // higher dims may be added if necessary

    private:
        int dim_idx[NumD];      // which of the dims in EpochList index me
        DataT data;
        ReadyT ready;

    public:
        ValuePool(const std::vector<int>& dim_idx_);
        ~ValuePool() {}

        template <typename U, unsigned MaxE, unsigned NumE>
        friend std::ostream& operator<<(std::ostream& s,
                                        const ValuePool<U, MaxE, NumE>& p);

        bool GetReady(EpochList<MaxD> *epoch) const;
        T& GetValue(EpochList<MaxD> *epoch);

        void SetValue(EpochList<MaxD> *epoch, const T& value);
        void SetValueBatch(const DataT& data_);
        void SetValueBatch(const DataT& data_, const ReadyT& ready_);

        void ClearValues(bool do_delete = false);
};


}


// Include template implementation in-place.
#include "value_pool.tpl.hpp"


#endif
