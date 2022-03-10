#include <iostream>
#include <string.h>
#include <assert.h>

#include "value_pool.hpp"


// Ugly code below, many switches...
// If someone knows a better way to "auto-generate" a base class that can be
// used to hold template derived classes and do dynamic dispatching on the
// template arguments, please let me know!


namespace foreactor {


/////////////////////////////////////////
// EpochList base class implementation //
/////////////////////////////////////////

EpochListBase::EpochListBase(unsigned max_dims)
        : max_dims(max_dims) {
    assert(max_dims <= 2);      // may set to higher if necessary
}


std::ostream& operator<<(std::ostream& s, const EpochListBase& e) {
    switch (e.max_dims) {
    case 0:
        s << *static_cast<const EpochList<0> *>(&e);
        break;
    case 1:
        s << *static_cast<const EpochList<1> *>(&e);
        break;
    case 2:
        s << *static_cast<const EpochList<2> *>(&e);
        break;
    default:
        break;      // not reached
    }
    return s;
}


EpochListBase *EpochListBase::New(unsigned max_dims) {
    EpochListBase *ret = nullptr;
    switch (max_dims) {
    case 0:
        ret = new EpochList<0>();
        break;
    case 1:
        ret = new EpochList<1>();
        break;
    case 2:
        ret = new EpochList<2>();
        break;
    default:
        break;      // not reached
    }
    return ret;
}

EpochListBase *EpochListBase::Copy(EpochListBase *epoch) {
    EpochListBase *ret = nullptr;
    switch (epoch->max_dims) {
    case 0:
        ret = new EpochList<0>();
        memcpy(static_cast<void *>(ret), static_cast<void *>(epoch),
               sizeof(EpochList<0>));
        break;
    case 1:
        ret = new EpochList<1>();
        memcpy(static_cast<void *>(ret), static_cast<void *>(epoch),
               sizeof(EpochList<1>));
        break;
    case 2:
        ret = new EpochList<2>();
        memcpy(static_cast<void *>(ret), static_cast<void *>(epoch),
               sizeof(EpochList<2>));
        break;
    default:
        break;      // not reached
    }
    return ret;
}

void EpochListBase::Delete(EpochListBase *epoch) {
    switch (epoch->max_dims) {
    case 0:
        delete static_cast<EpochList<0> *>(epoch);
        break;
    case 1:
        delete static_cast<EpochList<1> *>(epoch);
        break;
    case 2:
        delete static_cast<EpochList<2> *>(epoch);
        break;
    default:
        break;      // not reached
    }
}


bool EpochListBase::IsSame(const EpochListBase *epoch) const {
    if (max_dims != epoch->max_dims)
        return false;
    switch (max_dims) {
    case 0:
        return static_cast<const EpochList<0> *>(this)->IsSame(
               static_cast<const EpochList<0> *>(epoch));
    case 1:
        return static_cast<const EpochList<1> *>(this)->IsSame(
               static_cast<const EpochList<1> *>(epoch));
    case 2:
        return static_cast<const EpochList<2> *>(this)->IsSame(
               static_cast<const EpochList<2> *>(epoch));
    default:
        return false;   // not reached
    }
}

bool EpochListBase::AheadOf(const EpochListBase *epoch) const {
    if (max_dims != epoch->max_dims)
        return false;
    switch (max_dims) {
    case 0:
        return static_cast<const EpochList<0> *>(this)->AheadOf(
               static_cast<const EpochList<0> *>(epoch));
    case 1:
        return static_cast<const EpochList<1> *>(this)->AheadOf(
               static_cast<const EpochList<1> *>(epoch));
    case 2:
        return static_cast<const EpochList<2> *>(this)->AheadOf(
               static_cast<const EpochList<2> *>(epoch));
    default:
        return false;   // not reached
    }
}

void EpochListBase::CopyFrom(const EpochListBase *epoch) {
    assert(max_dims == epoch->max_dims);
    switch (max_dims) {
    case 0:
        static_cast<EpochList<0> *>(this)->CopyFrom(
            static_cast<const EpochList<0> *>(epoch));
        break;
    case 1:
        static_cast<EpochList<1> *>(this)->CopyFrom(
            static_cast<const EpochList<1> *>(epoch));
        break;
    case 2:
        static_cast<EpochList<2> *>(this)->CopyFrom(
            static_cast<const EpochList<2> *>(epoch));
        break;
    default:
        break;      // not reached
    }
}


size_t EpochListBase::GetEpoch(int dim_idx) const {
    switch (max_dims) {
    case 0:
        return static_cast<const EpochList<0> *>(this)->GetEpoch(dim_idx);
    case 1:
        return static_cast<const EpochList<1> *>(this)->GetEpoch(dim_idx);
    case 2:
        return static_cast<const EpochList<2> *>(this)->GetEpoch(dim_idx);
    default:
        return 0;   // not reached
    }
}

void EpochListBase::IncrementEpoch(int dim_idx) {
    switch (max_dims) {
    case 0:
        static_cast<EpochList<0> *>(this)->IncrementEpoch(dim_idx);
        break;
    case 1:
        static_cast<EpochList<1> *>(this)->IncrementEpoch(dim_idx);
        break;
    case 2:
        static_cast<EpochList<2> *>(this)->IncrementEpoch(dim_idx);
        break;
    default:
        break;      // not reached
    }
}


void EpochListBase::ClearEpochs() {
    switch (max_dims) {
    case 0:
        static_cast<EpochList<0> *>(this)->ClearEpochs();
        break;
    case 1:
        static_cast<EpochList<1> *>(this)->ClearEpochs();
        break;
    case 2:
        static_cast<EpochList<2> *>(this)->ClearEpochs();
        break;
    default:
        break;      // not reached
    }
}


}
