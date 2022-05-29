// Template implementation should be included in-place with the ".hpp".


namespace foreactor {


template <typename T>
ValuePool<T>::ValuePool(const std::unordered_set<int>& assoc_dims)
        : assoc_dims(assoc_dims), data{} {
}


template <typename T>
std::ostream& operator<<(std::ostream& s, const ValuePool<T>& p) {
    s << "ValuePool{";
    for (auto it = p.assoc_dims.cbegin(); it != p.assoc_dims.cend(); ++it) {
        s << *it;
        if (std::next(it) != p.assoc_dims.cend())
            s << ",";
    }
    s << "|";
    for (auto it = p.data.cbegin(); it != p.data.cend(); ++it) {
        s << it->first;
        if (std::next(it) != p.data.cend())
            s << ",";
    }
    s << "}";
    return s;
}


template <typename T>
void ValuePool<T>::Set(const EpochList& epoch, T value) {
    data[epoch.Sum(assoc_dims)] = value;
}

template <typename T>
void ValuePool<T>::Set(int epoch_sum, T value) {
    // variant used in IOUring class where epoch_sum has already been calculated
    assert(epoch_sum >= 0);
    data[epoch_sum] = value;
}


template <typename T>
bool ValuePool<T>::Has(const EpochList& epoch) const {
    return data.contains(epoch.Sum(assoc_dims));
}

template <typename T>
bool ValuePool<T>::Has(int epoch_sum) const {
    assert(epoch_sum >= 0);
    return data.contains(epoch_sum);
}


template <typename T>
T ValuePool<T>::Get(const EpochList& epoch) const {
    int epoch_sum = epoch.Sum(assoc_dims);
    assert(data.contains(epoch_sum));
    return data.at(epoch_sum);
}

template <typename T>
T ValuePool<T>::Get(int epoch_sum) const {
    assert(epoch_sum >= 0);
    assert(data.contains(epoch_sum));
    return data.at(epoch_sum);
}


template <typename T>
template <typename U>
std::enable_if_t<!std::is_pointer<U>::value, void>
ValuePool<T>::Remove(const EpochList& epoch) {
    assert(Has(epoch));
    data.erase(epoch.Sum(assoc_dims));
}

template <typename T>
template <typename U>
std::enable_if_t<std::is_pointer<U>::value, void>
ValuePool<T>::Remove(const EpochList& epoch, bool do_delete) {
    assert(Has(epoch));
    auto&& ptr = data.extract(epoch.Sum(assoc_dims)).mapped();
    if (do_delete) {
        if (ptr != nullptr)
            delete[] ptr;
    }
}


template <typename T>
template <typename U>
std::enable_if_t<!std::is_pointer<U>::value, void>
ValuePool<T>::Reset() {
    // non-pointer type
    data.clear();
}

template <typename T>
template <typename U>
std::enable_if_t<std::is_pointer<U>::value, void>
ValuePool<T>::Reset(bool do_delete) {
    // pointer type specialization
    if (do_delete) {
        for (auto&& [_, ptr] : data) {
            if (ptr != nullptr)
                delete[] ptr;
        }
    }
    data.clear();
}


}
