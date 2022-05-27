#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <type_traits>
#include <assert.h>

#include "debug.hpp"
#include "value_pool.hpp"


namespace foreactor {


//////////////////////////////
// EpochList implementation //
//////////////////////////////

EpochList::EpochList(unsigned total_dims)
        : total_dims(total_dims), epochs(total_dims) {
    Reset();
}


std::ostream& operator<<(std::ostream& s, const EpochList& e) {
    s << "EpochList{" << e.total_dims << ":";
    for (auto it = e.epochs.begin(); it != e.epochs.end(); ++it) {
        s << *it;
        if (it != e.epochs.end() - 1)
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
    // used key -1 as the special sum for singular pools, meaning not indexed
    // by anything, just a single value
    if (assoc_dims.size() == 0)
        return -1;

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
