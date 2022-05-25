#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <assert.h>

#include "debug.hpp"


namespace foreactor {


//////////////////////////////
// EpochList implementation //
//////////////////////////////

EpochList::EpochList(unsigned total_dims)
        : total_dims(total_dims), epochs(total_dims) {
    Reset();
}


friend std::ostream& operator<<(std::ostream& s, const EpochList& e) {
    s << "EpochList{" << total_dims << ":";
    for (auto it = e.epochs.begin(); it != e.epochs.end(); ++it) {
        s << *it;
        if (it != e.epochs.end() - 1)
            s << ",";
    }
    s << "}";
    return s;
}


void EpochList::Increment(int dim) {
    assert(dim >= 0 && dim < total_dims);
    epochs[dim]++;
}


int EpochList::At(int dim) const {
    assert(dim >= 0 && dim < total_dims);
    return epochs.at(dim);
}


int EpochList::Sum(const std::unordered_set<int>& assoc_dims) const {
    // used key -1 as the special sum for singular pools, meaning not indexed
    // by anything, just a single value
    if (assoc_dims.size() == 0)
        return -1;

    int epoch_sum = 0;
    for (int dim : assoc_dims) {
        assert(dim >= 0 && dim < total_dims);
        epoch_sum += epochs.at(dim);
    }
    return epoch_sum;
}


void EpochList::Reset() {
    for (size_t i = 0; i < total_dims; ++i)
        epochs[i] = 0;
}


}
