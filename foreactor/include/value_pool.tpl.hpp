// Template implementation should be included in-place with the ".hpp".


// Ugly code below, many switches...
// If someone knows a better way to "auto-generate" a base class that can be
// used to hold template derived classes and do dynamic dispatching on the
// template arguments, please let me know!


namespace foreactor {


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
inline bool EpochList<D>::IsSame(const EpochList<D> *epoch) const {
    for (unsigned i = 0; i < D; ++i) {
        if (epochs[i] != epoch->epochs[i])
            return false;
    }
    return true;
}

template <unsigned D>
inline bool EpochList<D>::AheadOf(const EpochList<D> *epoch) const {
    for (unsigned i = 0; i < D; ++i) {
        if (epochs[i] < epoch->epochs[i])
            return false;
    }
    return true;
}


template <unsigned D>
inline void EpochList<D>::CopyFrom(const EpochList<D> *epoch) {
    for (unsigned i = 0; i < D; ++i)
        epochs[i] = epoch->epochs[i];
}


template <unsigned D>
inline size_t EpochList<D>::GetEpoch(int dim_idx) const {
    assert(dim_idx >= 0 && dim_idx < static_cast<int>(D));
    return epochs[dim_idx];
}

template <unsigned D>
inline void EpochList<D>::IncrementEpoch(int dim_idx) {
    assert(dim_idx >= 0 && dim_idx < static_cast<int>(D));
    epochs[dim_idx]++;
}


template <unsigned D>
inline void EpochList<D>::ClearEpochs() {
    for (unsigned i = 0; i < D; ++i)
        epochs[i] = 0;
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
ValuePoolBase<T> *ValuePoolBase<T>::New(unsigned max_dims, unsigned num_dims,
                                        const std::vector<int>& dim_idx) {
    ValuePoolBase<T> *ret = nullptr;
    switch (max_dims) {
    case 0:
        switch (num_dims) {
        case 0:
            ret = new ValuePool<T, 0, 0>(dim_idx);
            break;
        default:
            break;      // not reached
        }
        break;
    case 1:
        switch (num_dims) {
        case 0:
            ret = new ValuePool<T, 1, 0>(dim_idx);
            break;
        case 1:
            ret = new ValuePool<T, 1, 1>(dim_idx);
            break;
        default:
            break;      // not reached
        }
        break;
    case 2:
        switch (num_dims) {
        case 0:
            ret = new ValuePool<T, 2, 0>(dim_idx);
            break;
        case 1:
            ret = new ValuePool<T, 2, 1>(dim_idx);
            break;
        case 2:
            ret = new ValuePool<T, 2, 2>(dim_idx);
            break;
        default:
            break;      // not reached
        }
        break;
    default:
        break;      // not reached
    }
    return ret;
}

template <typename T>
void ValuePoolBase<T>::Delete(ValuePoolBase<T> *pool) {
    switch (pool->max_dims) {
    case 0:
        switch (pool->num_dims) {
        case 0:
            delete static_cast<ValuePool<T, 0, 0> *>(pool);
            break;
        default:
            break;      // not reached
        }
        break;
    case 1:
        switch (pool->num_dims) {
        case 0:
            delete static_cast<ValuePool<T, 1, 0> *>(pool);
            break;
        case 1:
            delete static_cast<ValuePool<T, 1, 1> *>(pool);
            break;
        default:
            break;      // not reached
        }
        break;
    case 2:
        switch (pool->num_dims) {
        case 0:
            delete static_cast<ValuePool<T, 2, 0> *>(pool);
            break;
        case 1:
            delete static_cast<ValuePool<T, 2, 1> *>(pool);
            break;
        case 2:
            delete static_cast<ValuePool<T, 2, 2> *>(pool);
            break;
        default:
            break;      // not reached
        }
        break;
    default:
        break;      // not reached
    }
}


template <typename T>
inline bool ValuePoolBase<T>::GetReady(EpochListBase *epoch) const {
    switch (max_dims) {
    case 0:
        switch (num_dims) {
        case 0:
            return static_cast<const ValuePool<T, 0, 0> *>(this)->GetReady(
                   static_cast<EpochList<0> *>(epoch));
        default:
            return false;   // not reached
        }
        break;
    case 1:
        switch (num_dims) {
        case 0:
            return static_cast<const ValuePool<T, 1, 0> *>(this)->GetReady(
                   static_cast<EpochList<1> *>(epoch));
        case 1:
            return static_cast<const ValuePool<T, 1, 1> *>(this)->GetReady(
                   static_cast<EpochList<1> *>(epoch));
        default:
            return false;   // not reached
        }
        break;
    case 2:
        switch (num_dims) {
        case 0:
            return static_cast<const ValuePool<T, 2, 0> *>(this)->GetReady(
                   static_cast<EpochList<2> *>(epoch));
        case 1:
            return static_cast<const ValuePool<T, 2, 1> *>(this)->GetReady(
                   static_cast<EpochList<2> *>(epoch));
        case 2:
            return static_cast<const ValuePool<T, 2, 2> *>(this)->GetReady(
                   static_cast<EpochList<2> *>(epoch));
        default:
            return false;   // not reached
        }
    default:
        return false;   // not reached
    }
}

template <typename T>
inline T& ValuePoolBase<T>::GetValue(EpochListBase *epoch) {
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
inline void ValuePoolBase<T>::SetValue(EpochListBase *epoch, const T& value) {
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
            static_cast<EpochList<1> *>(epoch), value);
            break;
        case 1:
            static_cast<ValuePool<T, 1, 1> *>(this)->SetValue(
            static_cast<EpochList<1> *>(epoch), value);
            break;
        default:
            break;      // not reached
        }
        break;
    case 2:
        switch (num_dims) {
        case 0:
            static_cast<ValuePool<T, 2, 0> *>(this)->SetValue(
            static_cast<EpochList<2> *>(epoch), value);
            break;
        case 1:
            static_cast<ValuePool<T, 2, 1> *>(this)->SetValue(
            static_cast<EpochList<2> *>(epoch), value);
            break;
        case 2:
            static_cast<ValuePool<T, 2, 2> *>(this)->SetValue(
            static_cast<EpochList<2> *>(epoch), value);
            break;
        default:
            break;      // not reached
        }
    default:
        break;      // not reached
    }
}

template <typename T>
inline void ValuePoolBase<T>::SetValueBatch(
        const T& data_) {
    assert(num_dims == 0);
    switch (max_dims) {
    case 0:
        static_cast<ValuePool<T, 0, 0> *>(this)->SetValueBatch(data_);
        break;
    case 1:
        static_cast<ValuePool<T, 1, 0> *>(this)->SetValueBatch(data_);
        break;
    case 2:
        static_cast<ValuePool<T, 2, 0> *>(this)->SetValueBatch(data_);
        break;
    default:
        break;      // not reached
    }
    
}

template <typename T>
inline void ValuePoolBase<T>::SetValueBatch(
        const T& data_, const bool& ready_) {
    assert(num_dims == 0);
    switch (max_dims) {
    case 0:
        static_cast<ValuePool<T, 0, 0> *>(this)->SetValueBatch(data_, ready_);
        break;
    case 1:
        static_cast<ValuePool<T, 1, 0> *>(this)->SetValueBatch(data_, ready_);
        break;
    case 2:
        static_cast<ValuePool<T, 2, 0> *>(this)->SetValueBatch(data_, ready_);
        break;
    default:
        break;      // not reached
    }
    
}

template <typename T>
inline void ValuePoolBase<T>::SetValueBatch(
        const std::vector<T>& data_) {
    assert(num_dims == 1);
    switch (max_dims) {
    case 0:
        static_cast<ValuePool<T, 0, 1> *>(this)->SetValueBatch(data_);
        break;
    case 1:
        static_cast<ValuePool<T, 1, 1> *>(this)->SetValueBatch(data_);
        break;
    case 2:
        static_cast<ValuePool<T, 2, 1> *>(this)->SetValueBatch(data_);
        break;
    default:
        break;      // not reached
    }
}

template <typename T>
inline void ValuePoolBase<T>::SetValueBatch(
        const std::vector<T>& data_, const std::vector<bool>& ready_) {
    assert(num_dims == 1);
    switch (max_dims) {
    case 0:
        static_cast<ValuePool<T, 0, 1> *>(this)->SetValueBatch(data_, ready_);
        break;
    case 1:
        static_cast<ValuePool<T, 1, 1> *>(this)->SetValueBatch(data_, ready_);
        break;
    case 2:
        static_cast<ValuePool<T, 2, 1> *>(this)->SetValueBatch(data_, ready_);
        break;
    default:
        break;      // not reached
    }
}

template <typename T>
inline void ValuePoolBase<T>::SetValueBatch(
        const std::vector<std::vector<T>>& data_) {
    assert(num_dims == 2);
    switch (max_dims) {
    case 0:
        static_cast<ValuePool<T, 0, 2> *>(this)->SetValueBatch(data_);
        break;
    case 1:
        static_cast<ValuePool<T, 1, 2> *>(this)->SetValueBatch(data_);
        break;
    case 2:
        static_cast<ValuePool<T, 2, 2> *>(this)->SetValueBatch(data_);
        break;
    default:
        break;      // not reached
    }
}

template <typename T>
inline void ValuePoolBase<T>::SetValueBatch(
        const std::vector<std::vector<T>>& data_,
        const std::vector<std::vector<bool>>& ready_) {
    assert(num_dims == 2);
    switch (max_dims) {
    case 0:
        static_cast<ValuePool<T, 0, 2> *>(this)->SetValueBatch(data_, ready_);
        break;
    case 1:
        static_cast<ValuePool<T, 1, 2> *>(this)->SetValueBatch(data_, ready_);
        break;
    case 2:
        static_cast<ValuePool<T, 2, 2> *>(this)->SetValueBatch(data_, ready_);
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
ValuePool<T, MaxD, NumD>::ValuePool(const std::vector<int>& dim_idx_)
        : ValuePoolBase<T>(MaxD, NumD) {
    assert(dim_idx_.size() == NumD);
    for (unsigned i = 0; i < NumD; ++i) {
        dim_idx[i] = dim_idx_[i];
        assert(dim_idx[i] >= 0 && dim_idx[i] < MaxD);
    }
}


template <typename T, unsigned MaxD, unsigned NumD>
std::ostream& operator<<(std::ostream& s,
                         const ValuePool<T, MaxD, NumD>& p) {
    s << "pool[" << NumD << "/" << MaxD << ":";
    // print dim_idx
    for (unsigned i = 0; i < NumD; ++i) {
        s << p.dim_idx[i];
        if (NumD > 0 && i < NumD - 1)
            s << ",";
    }
    s << "|";
    // print ready
    if constexpr (NumD == 0)
        s << (p.ready ? "T" : "F");
    else if constexpr (NumD == 1) {
        for (const bool& r : p.ready)
            s << (r ? "T," : "F,");
    } else {
        for (const std::vector<bool>& rr : p.ready) {
            for (const bool& r : rr)
                s << (r ? "T," : "F,");
            s << ";";
        }
    }
    s << "|";
    // print data (the values)
    if constexpr (NumD == 0)
        s << p.data;
    else if constexpr (NumD == 1) {
        for (const T& v : p.data)
            s << v << ",";
    } else {
        for (const std::vector<T>& vv : p.data) {
            for (const T& v : vv)
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
    // 0-d
    if constexpr (NumD == 0)
        return ready;
    // 1-d
    else if constexpr (NumD == 1)
        return ready[epoch->GetEpoch(dim_idx[0])];
    // 2-d
    else
        return ready[epoch->GetEpoch(dim_idx[0])][epoch->GetEpoch(dim_idx[1])];
}

template <typename T, unsigned MaxD, unsigned NumD>
inline T& ValuePool<T, MaxD, NumD>::GetValue(
        EpochList<MaxD> *epoch) {
    assert(GetReady(epoch));

    // 0-d
    if constexpr (NumD == 0)
        return data;
    // 1-d
    else if constexpr (NumD == 1)
        return data[epoch->GetEpoch(dim_idx[0])];
    // 2-d
    else
        return data[epoch->GetEpoch(dim_idx[0])][epoch->GetEpoch(dim_idx[1])];
}


template <typename T, unsigned MaxD, unsigned NumD>
inline void ValuePool<T, MaxD, NumD>::SetValue(
        EpochList<MaxD> *epoch, const T& value) {
    // 0-d
    if constexpr (NumD == 0) {
        data = value;
        ready = true;
    // 1-d
    } else if constexpr (NumD == 1) {
        data[epoch->GetEpoch(dim_idx[0])] = value;
        ready[epoch->GetEpoch(dim_idx[0])] = true;
    // 2-d
    } else {
        data[epoch->GetEpoch(dim_idx[0])][epoch->GetEpoch(dim_idx[1])] = value;
        ready[epoch->GetEpoch(dim_idx[0])][epoch->GetEpoch(dim_idx[1])] = true;
    }
}

template <typename T, unsigned MaxD, unsigned NumD>
inline void ValuePool<T, MaxD, NumD>::SetValueBatch(
        const DataT& data_) {
    // 0-d
    if constexpr (NumD == 0) {
        data = data_;
        ready = true;

    // 1-d
    } else if constexpr (NumD == 1) {
        data.reserve(data_.size());
        ready.reserve(data_.size());
        for (const T& v : data_) {
            data.push_back(v);
            ready.push_back(true);
        }

    // 2-d
    } else {
        data.reserve(data_.size());
        ready.reserve(data_.size());
        for (const std::vector<T>& vv : data_) {
            data.push_back(std::vector<T>());
            data.back().reserve(vv.size());
            ready.push_back(std::vector<bool>());
            ready.back().reserve(vv.size());
            for (const T& v : vv) {
                data.back().push_back(v);
                ready.back().push_back(true);
            }
        }
    }
}

template <typename T, unsigned MaxD, unsigned NumD>
inline void ValuePool<T, MaxD, NumD>::SetValueBatch(
        const DataT& data_, const ReadyT& ready_) {
    // 0-d
    if constexpr (NumD == 0) {
        data = data_;
        ready = true;

    // 1-d
    } else if constexpr (NumD == 1) {
        assert(data_.size() == ready_.size());
        data.reserve(data_.size());
        ready.reserve(ready_.size());
        for (const T& v : data_)
            data.push_back(v);
        for (const bool& r : ready_)
            ready.push_back(r);

    // 2-d
    } else {
        assert(data_.size() == ready_.size());
        data.reserve(data_.size());
        ready.reserve(ready_.size());
        for (const std::vector<T>& vv : data_) {
            data.push_back(std::vector<T>());
            data.back().reserve(vv.size());
            for (const T& v : vv)
                data.back().push_back(v);
        }
        for (const std::vector<bool>& rr : ready_) {
            ready.push_back(std::vector<bool>());
            ready.back().reserve(rr.size());
            for (const bool& r : rr)
                ready.back().push_back(r);
        }
    }
}


template <typename T, unsigned MaxD, unsigned NumD>
inline void ValuePool<T, MaxD, NumD>::ClearValues(bool do_delete) {
    if (do_delete) {
        assert(std::is_pointer<T>::value);
    }

    // 0-d
    if constexpr (NumD == 0) {
        ready = false;
        if constexpr (std::is_pointer<T>::value) {
            if (do_delete) {
                if (data != nullptr)
                    delete data;
            }
        }

    // 1-d
    } else if constexpr (NumD == 1) {
        ready.clear();
        if constexpr (std::is_pointer<T>::value) {
            if (do_delete) {
                for (T& v : data) {
                    if (v != nullptr)
                        delete v;
                }
            }
        }
        data.clear();

    // 2-d
    } else {
        ready.clear();
        if constexpr (std::is_pointer<T>::value) {
            if (do_delete) {
                for (std::vector<T>& vv : data) {
                    for (T& v : vv) {
                        if (v != nullptr)
                            delete v;
                    }
                }
            }
        }
        data.clear();
    }
}


}
