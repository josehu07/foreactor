// Template implementation should be included in-place with the ".hpp".


namespace foreactor {


template <typename T>
RingBuffer<T>::RingBuffer(size_t capacity)
        : capacity(capacity), nvalid(0), head(0), tail(0) {
    assert(capacity > 0);
    data = new T[capacity];
    assert(data != nullptr);
}

template <typename T>
RingBuffer<T>::~RingBuffer() {
    delete[] data;
}


template <typename T>
std::ostream& operator<<(std::ostream& s, const RingBuffer<T>& rb) {
    s << "rb[" << nvalid << "/" << capacity << "," << head << "," << tail
      << "]";
    return s;
}


template <typename T>
void RingBuffer<T>::Push(T elem) {
    // may discard the head if ring is currently full
    if (nvalid == capacity) {
        assert(head == tail);
        head = (head + 1) % capacity;
    }
    data[tail] = elem;
    tail = (tail + 1) % capacity;
    if (nvalid < capacity)
        nvalid++;
}


template <typename T>
size_t RingBuffer<T>::RoundedIdx(int idx) const {
    PANIC_IF(idx < 0, "ring buffer idx %d less than 0\n", idx);
    size_t idx_ = (size_t) idx % capacity;   // automatic rounding
    PANIC_IF((idx_ >= head + nvalid) ||
             (idx_ < head && idx_ + capacity >= head + nvalid),
             "ring buffer idx %lu out of range\n", idx_);
    return idx_;
}

template <typename T>
void RingBuffer<T>::Set(int idx, T elem) {
    data[RoundedIdx(idx)] = elem;
}

template <typename T>
T RingBuffer<T>::Get(int idx) const {
    return data[RoundedIdx(idx)];
}


template <typename T>
void RingBuffer<T>::Reset() {
    nvalid = 0;
    head = 0;
    tail = 0;
}


}
