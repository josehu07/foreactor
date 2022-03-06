#include <vector>
#include <assert.h>
#include <string.h>

#include "value_pool.hpp"


// Ugly code...
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


inline size_t EpochListBase::GetEpoch(int dim_idx) const {
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

inline void EpochListBase::IncrementEpoch(int dim_idx) {
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


//////////////////////////////
// EpochList implementation //
//////////////////////////////

template <unsigned D>
EpochList<D>::EpochList()
        : EpochListBase(D) {
    for (unsigned i = 0; i < D; ++i)
        epochs[i] = 0;
}


template <unsigned D>
std::ostream& operator<<(std::ostream& s, const EpochList<D>& e) {
    s << "epoch[";
    for (unsigned i = 0; i < D; ++i) {
        s << e.epochs[i];
        if (D > 0 && i < D - 1)
            s << ",";
    }
    s << "]";
    return s;
}


template <unsigned D>
inline size_t EpochList<D>::GetEpoch(int dim_idx) const {
    assert(dim_idx >= 0 && dim_idx < D);
    return epochs[dim_idx];
}

template <unsigned D>
inline void EpochList<D>::IncrementEpoch(int dim_idx) {
    assert(dim_idx >= 0 && dim_idx < D);
    epochs[dim_idx]++;
}


/////////////////////////////////////////
// ValuePool base class implementation //
/////////////////////////////////////////

template <typename T>
ValuePoolBase<T>::ValuePoolBase(unsigned max_dims, unsigned num_dims)
        : max_dims(max_dims), num_dims(num_dims) {
    assert(max_dims <= 2);      // may set to higher if necessary
    assert(num_dims <= max_dims);
}


template <typename T>
std::ostream& operator<<(std::ostream& s, const ValuePoolBase<T>& p) {
    switch (p.max_dims) {
    case 0:
        switch (p.num_dims) {
        case 0:
            s << *static_cast<const ValuePool<T, 0, 0> *>(&p);
            break;
        default:
            break;      // not reached
        }
        break;
    case 1:
        switch (p.num_dims) {
        case 0:
            s << *static_cast<const ValuePool<T, 1, 0> *>(&p);
            break;
        case 1:
            s << *static_cast<const ValuePool<T, 1, 1> *>(&p);
            break;
        default:
            break;      // not reached
        }
        break;
    case 2:
        switch (p.num_dims) {
        case 0:
            s << *static_cast<const ValuePool<T, 2, 0> *>(&p);
            break;
        case 1:
            s << *static_cast<const ValuePool<T, 2, 1> *>(&p);
            break;
        case 2:
            s << *static_cast<const ValuePool<T, 2, 2> *>(&p);
            break;
        default:
            break;      // not reached
        }
        break;
    default:
        break;      // not reached
    }
    return s;
}


template <typename T>
inline bool ValuePoolBase<T>::GetReady(EpochListBase *epoch) const {
    switch (max_dims) {
    case 0:
        switch (num_dims) {
        case 0:
            return static_cast<ValuePool<T, 0, 0> *>(this)->GetReady(
                   static_cast<EpochList<0> *>(epoch));
        default:
            return false;   // not reached
        }
        break;
    case 1:
        switch (num_dims) {
        case 0:
            return static_cast<ValuePool<T, 1, 0> *>(this)->GetReady(
                   static_cast<EpochList<0> *>(epoch));
        case 1:
            return static_cast<ValuePool<T, 1, 1> *>(this)->GetReady(
                   static_cast<EpochList<0> *>(epoch));
        default:
            return false;   // not reached
        }
        break;
    case 2:
        switch (num_dims) {
        case 0:
            return static_cast<ValuePool<T, 2, 0> *>(this)->GetReady(
                   static_cast<EpochList<0> *>(epoch));
        case 1:
            return static_cast<ValuePool<T, 2, 1> *>(this)->GetReady(
                   static_cast<EpochList<0> *>(epoch));
        case 2:
            return static_cast<ValuePool<T, 2, 2> *>(this)->GetReady(
                   static_cast<EpochList<0> *>(epoch));
        default:
            return false;   // not reached
        }
    default:
        return false;   // not reached
    }
}

template <typename T>
inline T& ValuePoolBase<T>::GetValue(EpochListBase *epoch) const {
    switch (max_dims) {
    case 0:
        switch (num_dims) {
        case 0:
            return static_cast<ValuePool<T, 0, 0> *>(this)->GetValue(
                   static_cast<EpochList<0> *>(epoch));
        default:
            return static_cast<ValuePool<T, 0, 0> *>(this)->GetValue(
                   static_cast<EpochList<0> *>(epoch));      // not reached
        }
        break;
    case 1:
        switch (num_dims) {
        case 0:
            return static_cast<ValuePool<T, 1, 0> *>(this)->GetValue(
                   static_cast<EpochList<1> *>(epoch));
        case 1:
            return static_cast<ValuePool<T, 1, 1> *>(this)->GetValue(
                   static_cast<EpochList<1> *>(epoch));
        default:
            return static_cast<ValuePool<T, 0, 0> *>(this)->GetValue(
                   static_cast<EpochList<0> *>(epoch));      // not reached
        }
        break;
    case 2:
        switch (num_dims) {
        case 0:
            return static_cast<ValuePool<T, 2, 0> *>(this)->GetValue(
                   static_cast<EpochList<2> *>(epoch));
        case 1:
            return static_cast<ValuePool<T, 2, 1> *>(this)->GetValue(
                   static_cast<EpochList<2> *>(epoch));
        case 2:
            return static_cast<ValuePool<T, 2, 2> *>(this)->GetValue(
                   static_cast<EpochList<2> *>(epoch));
        default:
            return static_cast<ValuePool<T, 0, 0> *>(this)->GetValue(
                   static_cast<EpochList<0> *>(epoch));      // not reached
        }
    default:
        return static_cast<ValuePool<T, 0, 0> *>(this)->GetValue(
               static_cast<EpochList<0> *>(epoch));      // not reached
    }
}


template <typename T>
inline void ValuePoolBase<T>::SetValue(EpochListBase *epoch, T value) {
    switch (max_dims) {
    case 0:
        switch (num_dims) {
        case 0:
            static_cast<ValuePool<T, 0, 0> *>(this)->SetValue(
            static_cast<EpochList<0> *>(epoch), value);
            break;
        default:
            break;      // not reached
        }
        break;
    case 1:
        switch (num_dims) {
        case 0:
            static_cast<ValuePool<T, 1, 0> *>(this)->SetValue(
            static_cast<EpochList<0> *>(epoch), value);
            break;
        case 1:
            static_cast<ValuePool<T, 1, 1> *>(this)->SetValue(
            static_cast<EpochList<0> *>(epoch), value);
            break;
        default:
            break;      // not reached
        }
        break;
    case 2:
        switch (num_dims) {
        case 0:
            static_cast<ValuePool<T, 2, 0> *>(this)->SetValue(
            static_cast<EpochList<0> *>(epoch), value);
            break;
        case 1:
            static_cast<ValuePool<T, 2, 1> *>(this)->SetValue(
            static_cast<EpochList<0> *>(epoch), value);
            break;
        case 2:
            static_cast<ValuePool<T, 2, 2> *>(this)->SetValue(
            static_cast<EpochList<0> *>(epoch), value);
            break;
        default:
            break;      // not reached
        }
    default:
        break;      // not reached
    }
}

template <typename T>
inline void ValuePoolBase<T>::SetValueBatch(T& values) {
    assert(num_dims == 0);
    switch (max_dims) {
    case 0:
        static_cast<ValuePool<T, 0, 0> *>(this)->SetValuebatch(values);
        break;
    case 1:
        static_cast<ValuePool<T, 1, 0> *>(this)->SetValuebatch(values);
        break;
    case 2:
        static_cast<ValuePool<T, 2, 0> *>(this)->SetValuebatch(values);
        break;
    default:
        break;      // not reached
    }
    
}

template <typename T>
inline void ValuePoolBase<T>::SetValueBatch(std::vector<T>& values) {
    assert(num_dims == 1);
    switch (max_dims) {
    case 0:
        static_cast<ValuePool<T, 0, 1> *>(this)->SetValuebatch(values);
        break;
    case 1:
        static_cast<ValuePool<T, 1, 1> *>(this)->SetValuebatch(values);
        break;
    case 2:
        static_cast<ValuePool<T, 2, 1> *>(this)->SetValuebatch(values);
        break;
    default:
        break;      // not reached
    }
}

template <typename T>
inline void ValuePoolBase<T>::SetValueBatch(std::vector<std::vector<T>>& values) {
    assert(num_dims == 2);
    switch (max_dims) {
    case 0:
        static_cast<ValuePool<T, 0, 2> *>(this)->SetValuebatch(values);
        break;
    case 1:
        static_cast<ValuePool<T, 1, 2> *>(this)->SetValuebatch(values);
        break;
    case 2:
        static_cast<ValuePool<T, 2, 2> *>(this)->SetValuebatch(values);
        break;
    default:
        break;      // not reached
    }
}


template <typename T>
inline void ValuePoolBase<T>::ClearValues(bool do_delete) {
    switch (max_dims) {
    case 0:
        switch (num_dims) {
        case 0:
            static_cast<ValuePool<T, 0, 0> *>(this)->ClearValues(do_delete);
            break;
        default:
            break;      // not reached
        }
        break;
    case 1:
        switch (num_dims) {
        case 0:
            static_cast<ValuePool<T, 1, 0> *>(this)->ClearValues(do_delete);
            break;
        case 1:
            static_cast<ValuePool<T, 1, 1> *>(this)->ClearValues(do_delete);
            break;
        default:
            break;      // not reached
        }
        break;
    case 2:
        switch (num_dims) {
        case 0:
            static_cast<ValuePool<T, 2, 0> *>(this)->ClearValues(do_delete);
            break;
        case 1:
            static_cast<ValuePool<T, 2, 1> *>(this)->ClearValues(do_delete);
            break;
        case 2:
            static_cast<ValuePool<T, 2, 2> *>(this)->ClearValues(do_delete);
            break;
        default:
            break;      // not reached
        }
    default:
        break;      // not reached
    }
}


//////////////////////////////
// ValuePool implementation //
//////////////////////////////

template <typename T, unsigned MaxD, unsigned NumD>
ValuePool<T, MaxD, NumD>::ValuePool(std::vector<int> dim_idx_)
        : ValuePoolBase<T>(MaxD, NumD) {
    assert(dim_idx_.size() == NumD);
    for (int i = 0; i < NumD; ++i) {
        dim_idx[i] = dim_idx_[i];
        assert(dim_idx[i] >= 0 && dim_idx[i] < MaxD);
    }
}


template <typename T, unsigned MaxD, unsigned NumD>
std::ostream& operator<<(std::ostream& s,
                         const ValuePool<T, MaxD, NumD>& p) {
    s << "pool[" << NumD << "/" << MaxD << ":";
    // print dim_idx
    for (int i = 0; i < NumD; ++i) {
        s << p.dim_idx[i];
        if (NumD > 0 && i < NumD - 1)
            s << ",";
    }
    s << "|";
    // print ready
    if constexpr (NumD == 0)
        s << p.ready ? "T" : "F";
    else if constexpr (NumD == 1) {
        for (bool& r : p.ready)
            s << r ? "T," : "F,";
    } else {
        for (std::vector<bool>& rr : p.ready) {
            for (bool& r : rr)
                s << r ? "T," : "F,";
            s << ";";
        }
    }
    s << "|";
    // print data (the values)
    if constexpr (NumD == 0)
        s << p.data;
    else if constexpr (NumD == 1) {
        for (T& v : p.data)
            s << v << ",";
    } else {
        for (std::vector<T>& vv : p.data) {
            for (T& v : vv)
                s << v << ",";
            s << ";";
        }
    }
    s << "]";
    return s;
}


template <typename T, unsigned MaxD, unsigned NumD>
inline bool ValuePool<T, MaxD, NumD>::GetReady(
        EpochList<MaxD> *epoch) const {
    if constexpr (NumD == 0)
        return ready;
    else if constexpr (NumD == 1)
        return ready[epoch->GetEpoch(dim_idx[0])];
    else
        return ready[epoch->GetEpoch(dim_idx[0])][epoch->GetEpoch(dim_idx[1])];
}

template <typename T, unsigned MaxD, unsigned NumD>
inline T& ValuePool<T, MaxD, NumD>::GetValue(
        EpochList<MaxD> *epoch) const {
    if constexpr (NumD == 0)
        return data;
    else if constexpr (NumD == 1)
        return data[epoch->GetEpoch(dim_idx[0])];
    else
        return data[epoch->GetEpoch(dim_idx[0])][epoch->GetEpoch(dim_idx[1])];
}


template <typename T, unsigned MaxD, unsigned NumD>
inline void ValuePool<T, MaxD, NumD>::SetValue(
        EpochList<MaxD> *epoch, T value) {
    if constexpr (NumD == 0)
        data = value;
    else if constexpr (NumD == 1)
        data[epoch->GetEpoch(dim_idx[0])] = value;
    else
        data[epoch->GetEpoch(dim_idx[0])][epoch->GetEpoch(dim_idx[1])] = value;
}

template <typename T, unsigned MaxD, unsigned NumD>
inline void ValuePool<T, MaxD, NumD>::SetValueBatch(DataT& values) {
    if constexpr (NumD == 0)
        data = values;
    else if constexpr (NumD == 1) {
        data.reserve(values.size());
        for (T& v : values)
            data.push_back(v);
    } else {
        data.reserve(values.size());
        for (std::vector<T>& vv : values) {
            data.push_back(std::vector<T>());
            data.back().reserve(vv.size());
            for (T& v : vv)
                data.back().push_back(v);
        }
    }
}


template <typename T, unsigned MaxD, unsigned NumD>
inline void ValuePool<T, MaxD, NumD>::ClearValues(bool delete_ptr) {
    if constexpr (NumD == 0) {
        ready = false;
        if (delete_ptr)
            delete data;
    } else if constexpr (NumD == 1) {
        ready.clear();
        if (delete_ptr)
            for (T& v : data)
                delete v;
        data.clear();
    } else {
        ready.clear();
        if (delete_ptr) {
            for (std::vector<T>& vv : data) {
                for (T& v : vv)
                    delete v;
            }
        }
        data.clear();
    }
}


}
