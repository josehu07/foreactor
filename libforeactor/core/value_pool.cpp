#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <type_traits>
#include <assert.h>

#include "debug.hpp"
#include "value_pool.hpp"


namespace foreactor {


EpochList::EpochList(unsigned total_dims)
        : total_dims(total_dims), epochs(total_dims, 0) {
    Reset();
}


std::ostream& operator<<(std::ostream& s, const EpochList& e) {
    s << "EL{" << e.total_dims << ":";
    for (auto it = e.epochs.cbegin(); it != e.epochs.cend(); ++it) {
        s << *it;
        if (it != std::prev(e.epochs.cend()))
            s << ",";
    }
    s << "}";
    return s;
}


void EpochList::Increment(int dim) {
    assert(dim >= 0 && dim < static_cast<int>(total_dims));
    epochs[dim]++;
}

void EpochList::CopyFrom(const EpochList& other) {
    assert(total_dims == other.Size());
    for (size_t i = 0; i < total_dims; ++i)
        epochs[i] = other.At(i);
}


size_t EpochList::Size() const {
    return total_dims;
}


int EpochList::At(int dim) const {
    assert(dim >= 0 && dim < static_cast<int>(total_dims));
    return epochs.at(dim);
}

int EpochList::Sum(const std::unordered_set<int>& assoc_dims) const {
    if (assoc_dims.size() == 0)
        return 0;

    int epoch_sum = 0;
    for (int dim : assoc_dims) {
        assert(dim >= 0 && dim < static_cast<int>(total_dims));
        epoch_sum += epochs.at(dim);
    }
    return epoch_sum;
}


bool EpochList::SameAs(const EpochList& other) const {
    assert(total_dims == other.Size());
    for (size_t i = 0; i < total_dims; ++i) {
        if (epochs[i] != other.At(i))
            return false;
    }
    return true;
}

bool EpochList::AheadOf(const EpochList& other) const {
    assert(total_dims == other.Size());
    for (size_t i = 0; i < total_dims; ++i) {
        if (epochs[i] < other.At(i))
            return false;
    }
    return true;
}


const int *EpochList::RawArray() const {
    return epochs.data();
}


void EpochList::Reset() {
    for (size_t i = 0; i < total_dims; ++i)
        epochs[i] = 0;
}


}
