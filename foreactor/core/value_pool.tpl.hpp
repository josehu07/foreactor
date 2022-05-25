// Template implementation should be included in-place with the ".hpp".


namespace foreactor {


template <typename T>
ValuePool<T>::ValuePool(const std::unordered_set<int>& assoc_dims)
        : assoc_dims(assoc_dims), data{} {
}


template <typename T>
std::ostream& operator<<(std::ostream& s, const ValuePool<T>& p) {
    s << "ValuePool{";
    for (auto it = p.assoc_dims.begin(); it != p.assoc_dims.end(); ++it) {
        s << *it;
        if (it != std::prev(p.assoc_dims.end()))
            s << ",";
    }
    s << "|";
    for (auto it = p.data.begin(); it != p.data.end(); ++it) {
        s << it->first << "-" << it->second << ",";
        if (it != std::prev(p.data.end()))
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
    assert(epoch_sum >= -1);
    data[epoch_sum] = value;
}


template <typename T>
bool ValuePool<T>::Has(const EpochList& epoch) const {
    return data.contains(epoch.Sum(assoc_dims));
}

template <typename T>
bool ValuePool<T>::Has(int epoch_sum) const {
    assert(epoch_sum >= -1);
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
    assert(epoch_sum >= -1);
    assert(data.contains(epoch_sum));
    return data.at(epoch_sum);
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

template <typename T>
template <typename U>
std::enable_if_t<!std::is_pointer<U>::value, void>
ValuePool<T>::Reset([[maybe_unused]] bool do_delete) {
    // non-pointer type
    data.clear();
}


}
