#include <iostream>
#include <string.h>
#include <assert.h>

#include "debug.hpp"


#ifndef __FOREACTOR_RING_BUFFER_H__
#define __FOREACTOR_RING_BUFFER_H__


namespace foreactor {


// Basic ring buffer. Template argument T specifies element type.
template <typename T>
class RingBuffer {
    private:
        T *data = nullptr;
        size_t capacity = 0;
        size_t nvalid = 0;
        size_t head = 0;
        size_t tail = 0;

    public:
        RingBuffer() = delete;
        RingBuffer(size_t capacity);
        ~RingBuffer();

        template <typename U>
        friend std::ostream& operator<<(std::ostream& s,
                                        const RingBuffer<U>& rb);

        void Push(T elem);
        T Get(size_t idx) const;

        void Reset();
};


}


// Include template implementation in-place.
#include "ring_buffer.tpl.hpp"


#endif
