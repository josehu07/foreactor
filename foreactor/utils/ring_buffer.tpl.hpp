// Template implementation should be included in-place with the ".hpp".


namespace foreactor {


template <typename T>
RingBuffer<T>::RingBuffer(size_t capacity)
        : capacity(capacity), nvalid(0), head(0), tail(0) {
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
    // May discard the head if ring is currently full.
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
T RingBuffer<T>::Get(size_t idx) const {
    PANIC_IF((idx >= capacity) || (idx >= head + nvalid) ||
             (idx < head && idx + capacity >= head + nvalid),
             "ring buffer idx %lu out of range\n", idx);
    return data[idx];
}


template <typename T>
void RingBuffer<T>::Reset() {
    nvalid = 0;
    head = 0;
    tail = 0;
}


}
