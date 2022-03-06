#include <iostream>
#include <vector>
#include <type_traits>


#ifndef __FOREACTOR_VALUE_POOL_H__
#define __FOREACTOR_VALUE_POOL_H__


namespace foreactor {


class NodeAndEpoch;     // forward declarations
class SyscallNode;


// An EpochList is a n-dimensional "tuple" of loop indices. If an SCGraph
// has n back-pointing edges, then it is associated with n-dimensional Epochs
// to index any ValuePool created for that SCGraph. Each back-pointing edge
// corresponds to one index value in the tuple, and anytime it is traversed
// that index value increments.
// 
// This is the base class for auto-resolving of template arguments if called
// upon a base class pointer.
class EpochListBase {
    friend class NodeAndEpoch;
    friend class SyscallNode;

    private:
        const unsigned max_dims = 0;

        static EpochListBase *Copy(EpochListBase *epoch);
        static void Delete(EpochListBase *epoch);
        
    public:
        EpochListBase() = delete;
        EpochListBase(unsigned max_dims);
        virtual ~EpochListBase();

        friend std::ostream& operator<<(std::ostream& s,
                                        const EpochListBase& e);

        // Every child class must implement these interfaces.
        // The base class implementation of these methods provide
        // auto-resolving of D.
        virtual size_t GetEpoch(int dim_idx) const;
        virtual void IncrementEpoch(int dim_idx);
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

        size_t GetEpoch(int dim_idx) const;
        void IncrementEpoch(int dim_idx);
};


// Value pool is an n-dimensional vector used to hold argument values across
// loops in SCGraph. 0-dim pool is a single scalar, 1-dim pool is a vector
// holding values that change only across one loop, 2-dim pool is a vector
// of vectors holding values that change across two nested loops, etc.
//
// This is the base class for auto-resolving of template arguments if called
// upon a base class pointer.
template <typename T>
class ValuePoolBase {
    private:
        const unsigned max_dims = 0;
        const unsigned num_dims = 0;

    public:
        ValuePoolBase() = delete;
        ValuePoolBase(unsigned max_dims, unsigned num_dims);
        ~ValuePoolBase() {}

        template <typename U>
        friend std::ostream& operator<<(std::ostream& s,
                                        const ValuePoolBase<U>& p);

        // Every child class must implement these interfaces.
        // The base class implementations of these methods provide
        // auto-resolving of MaxD and NumD.
        virtual bool GetReady(EpochListBase *epoch) const;
        virtual T& GetValue(EpochListBase *epoch) const;

        // Value might be set at the entrance of hijacked function, or at
        // an argument installation action, or by CheckArgs() of a SyscallNode
        // when it is actually invoked.
        virtual void SetValue(EpochListBase *epoch, T value);
        virtual void SetValueBatch(T& values);
        virtual void SetValueBatch(std::vector<T>& values);
        virtual void SetValueBatch(std::vector<std::vector<T>>& values);

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
        ValuePool(std::vector<int> dim_idx_);
        ~ValuePool() {}

        template <typename U, unsigned MaxE, unsigned NumE>
        friend std::ostream& operator<<(std::ostream& s,
                                        const ValuePool<U, MaxE, NumE>& p);

        bool GetReady(EpochList<MaxD> *epoch) const;
        T& GetValue(EpochList<MaxD> *epoch) const;

        void SetValue(EpochList<MaxD> *epoch, T value);
        void SetValueBatch(DataT& values);

        void ClearValues(bool do_delete = false);
};


}


#endif
