#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <type_traits>
#include <assert.h>

#include "debug.hpp"


#ifndef __FOREACTOR_VALUE_POOL_H__
#define __FOREACTOR_VALUE_POOL_H__


namespace foreactor {


// Global array of epoch numbers used at index into ValuePools.
class EpochList {
    private:
        // Total number of back-pointing edges.
        unsigned total_dims = 0;

        // Array of length total_dims, initially all zeros.
        std::vector<int> epochs;

    public:
        EpochList() = delete;
        EpochList(unsigned total_dims);
        ~EpochList() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const EpochList& e);

        void Increment(int dim);
        void CopyFrom(const EpochList& other);

        [[nodiscard]] size_t Size() const;

        [[nodiscard]] int At(int dim) const;
        [[nodiscard]] int Sum(const std::unordered_set<int>& assoc_dims) const;

        [[nodiscard]] bool SameAs(const EpochList& other) const;
        [[nodiscard]] bool AheadOf(const EpochList& other) const;

        [[nodiscard]] const int *RawArray() const;

        void Reset();
};


// Storage space for values that vary across loops in the SCGraph.
// Template argument T specifies element type.
template <typename T>
class ValuePool {
    private:
        // Associated dimensions indices.
        std::unordered_set<int> assoc_dims;

        // Key is the sum of associated dimensions of EpochList.
        std::unordered_map<int, T> data;

    public:
        ValuePool() = delete;
        ValuePool(const std::unordered_set<int>& assoc_dims);
        ~ValuePool() {}

        template <typename U>
        friend std::ostream& operator<<(std::ostream& s,
                                        const ValuePool<U>& p);

        void Set(const EpochList& epoch, T value);
        void Set(int epoch_sum, T value);

        [[nodiscard]] bool Has(const EpochList& epoch) const;
        [[nodiscard]] bool Has(int epoch_sum) const;

        [[nodiscard]] T Get(const EpochList& epoch) const;
        [[nodiscard]] T Get(int epoch_sum) const;

        template <typename U = T>
        std::enable_if_t<!std::is_pointer<U>::value, void> Remove(
            const EpochList& epoch);
        template <typename U = T>
        std::enable_if_t<std::is_pointer<U>::value, void> Remove(
            const EpochList& epoch,
            std::unordered_set<U> *move_into = nullptr);

        template <typename U = T>
        std::enable_if_t<!std::is_pointer<U>::value, void> Reset();
        template <typename U = T>
        std::enable_if_t<std::is_pointer<U>::value, void> Reset(
            std::unordered_set<U> *move_into = nullptr);
};


}


// Include template implementation in-place.
#include "value_pool.tpl.hpp"


#endif
