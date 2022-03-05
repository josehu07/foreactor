#include <vector>
#include <assert.h>

#include "value_pool.hpp"


namespace foreactor {


/////////////////////////////////////////
// EpochList base class implementation //
/////////////////////////////////////////

EpochListBase::EpochListBase(unsigned max_dims)
        : max_dims(max_dims) {
    assert(max_dims <= 2);      // may set to higher if necessary
}


inline size_t EpochListBase::GetEpoch(int dim_idx) const {
    return static_cast<EpochList<max_dims> *>(this)->GetEpoch(dim_idx);
}

inline size_t EpochListBase::operator[](int dim_idx) const {
    return static_cast<EpochList<max_dims> *>(this)->operator[](dim_idx);
}

inline void EpochListBase::IncrementEpoch(int dim_idx) {
    static_cast<EpochList<max_dims> *>(this)->IncrementEpoch(dim_idx);
}


//////////////////////////////
// EpochList implementation //
//////////////////////////////

template <unsigned D>
EpochList<D>::EpochList()
        : EpochListBase(D) {
    for (int i = 0; i < D; ++i)
        epochs[i] = 0;
}


template <unsigned D>
inline size_t EpochList<D>::GetEpoch(int dim_idx) const {
    assert(dim_idx >= 0 && dim_idx < D);
    return epochs[dim_idx];
}

template <unsigned D>
inline size_t EpochList<D>::operator[](int dim_idx) const {
    return GetEpoch(dim_idx);
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
inline bool ValuePoolBase<T>::GetReady(EpochList& epoch) const {
    return static_cast<ValuePool<T, max_dims, num_dims> *>(this)->GetReady(
        static_cast<EpochList<max_dims>&>(epoch));
}

template <typename T>
inline T ValuePoolBase<T>::GetValue(EpochList& epoch) const {
    return static_cast<ValuePool<T, max_dims, num_dims> *>(this)->GetValue(
        static_cast<EpochList<max_dims>&>(epoch));
}

template <typename T>
inline T *ValuePoolBase<T>::PtrValue(EpochList& epoch) const {
    return static_cast<ValuePool<T, max_dims, num_dims> *>(this)->PtrValue(
        static_cast<EpochList<max_dims>&>(epoch));
}


template <typename T>
inline void ValuePoolBase<T>::SetValue(EpochList& epoch, T value) {
    static_cast<ValuePool<T, max_dims, num_dims> *>(this)->SetValue(
        static_cast<EpochList<max_dims>&>(epoch), value);
}

template <typename T>
inline void ValuePoolBase<T>::SetValueBatch(T& values) {
    assert(num_dims == 0);
    static_cast<ValuePool<T, max_dims, 0> *>(this)->SetValuebatch(values);
}

template <typename T>
inline void ValuePoolBase<T>::SetValueBatch(std::vector<T>& values) {
    assert(num_dims == 1);
    static_cast<ValuePool<T, max_dims, 1> *>(this)->SetValuebatch(values);
}

template <typename T>
inline void ValuePoolBase<T>::SetValueBatch(std::vector<std::vector<T>>& values) {
    assert(num_dims == 2);
    static_cast<ValuePool<T, max_dims, 2> *>(this)->SetValuebatch(values);
}


template <typename T>
inline void ValuePoolBase<T>::ClearValues() {
    static_cast<ValuePool<T, max_dims, num_dims> *>(this)->ClearValues();
}


//////////////////////////////
// ValuePool implementation //
//////////////////////////////

template <typename T, unsigned MaxD, unsigned NumD>
ValuePool<T, MaxD, NumD>::ValuePool(std::vector<int> dim_idx_)
        : ValuePoolBase(MaxD, NumD) {
    assert(dim_idx_.size() == NumD);
    for (int i = 0; i < NumD; ++i) {
        dim_idx[i] = dim_idx_[i];
        assert(dim_idx[i] >= 0 && dim_idx[i] < MaxD);
    }
}


template <typename T, unsigned MaxD, unsigned NumD>
inline bool ValuePool<T, MaxD, NumD>::GetReady(EpochList<MaxD>& epoch) const {
    if constexpr (NumD == 0)
        return ready;
    else if constexpr (NumD == 1)
        return ready[epoch[dim_idx[0]]];
    else
        return ready[epoch[dim_idx[0]]][epoch[dim_idx[1]]];
}

template <typename T, unsigned MaxD, unsigned NumD>
inline T ValuePool<T, MaxD, NumD>::GetValue(EpochList<MaxD>& epoch) const {
    if constexpr (NumD == 0)
        return data;
    else if constexpr (NumD == 1)
        return data[epoch[dim_idx[0]]];
    else
        return data[epoch[dim_idx[0]]][epoch[dim_idx[1]]];
}

template <typename T, unsigned MaxD, unsigned NumD>
inline T *ValuePool<T, MaxD, NumD>::PtrValue(EpochList<MaxD>& epoch) const {
    if constexpr (NumD == 0)
        return &data;
    else if constexpr (NumD == 1)
        return &data[epoch[dim_idx[0]]];
    else
        return &data[epoch[dim_idx[0]]][epoch[dim_idx[1]]];
}


template <typename T, unsigned MaxD, unsigned NumD>
inline void ValuePool<T, MaxD, NumD>::SetValue(EpochList<MaxD>& epoch,
                                               T value) {
    if constexpr (NumD == 0)
        data = value;
    else if constexpr (NumD == 1)
        data[epoch[dim_idx[0]]] = value;
    else
        data[epoch[dim_idx[0]]][epoch[dim_idx[1]]] = value;
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
inline void ValuePool<T, MaxD, NumD>::ClearValues() {
    if constexpr (NumD == 0)
        ready = false;
    else {
        ready.clear();
        data.clear();
    }   
}


}
