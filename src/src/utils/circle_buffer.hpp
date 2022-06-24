#pragma once

template <typename T>
class CircleBuffer {
    size_t head_;
    size_t tail_;
    size_t size_;
    std::vector<T> buffer_;

    void _inc(size_t & _idx) {
        _idx = (_idx + 1) % buffer_.size();
    }

public:
    CircleBuffer(size_t _size)
        : head_(0), tail_(0)  // [head_, tail_)
        , size_(_size), buffer_(_size + 1)  // ! one extra item for judging full queue.
                                            // ! so that, full is (tail_ + 1 == head_), empty is (tail_ == head_)
    {}

    size_t n_elements() const {
        if (tail_ >= head_) {
            return tail_ - head_;
        } else {
            return tail_ + buffer_.size() - head_;
        }
    }

    bool is_empty() const {
        return tail_ == head_;
    }

    bool is_full() const {
        return (tail_ + 1 == head_) || (head_ == 0 && tail_ == size_);
    }

    void push_back(T && _new_val) {
        buffer_[tail_] = std::move(_new_val);
        this->_inc(tail_);
        if (tail_ == head_) {
            this->_inc(head_);
        }
    }

    T && pop_front() {
        // ! must check not empty before pop;
        assert(!this->is_empty());

        size_t idx = head_;
        this->_inc(head_);
        return std::move(buffer_[idx]);
    }

    T & offset_front(size_t _off) {
        return buffer_[(head_ + _off) % buffer_.size()];
    }

    T & offset_back(size_t _off) {
        _off += 1; // since 'tail_-1' point to last element
        size_t t = tail_;
        if (t < _off) {
            t += buffer_.size();
        }
        return buffer_[t - _off];
    }
};


// #include <spdlog/spdlog.h>
// inline void TestCircleBuffer() {
//     CircleBuffer<std::unique_ptr<int32_t>> buffer(5);
//     log::info("n_elements: {}, full: {}, empty: {}", buffer.n_elements(), buffer.is_full(), buffer.is_empty());
//     buffer.push_back(std::make_unique<int32_t>(100));
//     buffer.push_back(std::make_unique<int32_t>(200));
//     buffer.push_back(std::make_unique<int32_t>(300));
//     buffer.push_back(std::make_unique<int32_t>(400));
//     log::info("n_elements: {}, full: {}, empty: {}", buffer.n_elements(), buffer.is_full(), buffer.is_empty());
//     buffer.push_back(std::make_unique<int32_t>(500));
//     log::info("n_elements: {}, full: {}, empty: {}", buffer.n_elements(), buffer.is_full(), buffer.is_empty());
//     buffer.push_back(std::make_unique<int32_t>(600));
//     log::info("n_elements: {}, full: {}, empty: {}", buffer.n_elements(), buffer.is_full(), buffer.is_empty());
//     buffer.push_back(std::make_unique<int32_t>(700));
//     log::info("n_elements: {}, full: {}, empty: {}", buffer.n_elements(), buffer.is_full(), buffer.is_empty());
//     std::unique_ptr<int32_t> v;
//     v = buffer.pop_front(); log::info("value: {}", *v);
//     v = buffer.pop_front(); log::info("value: {}", *v);
//     buffer.push_back(std::make_unique<int32_t>(800));
//     log::info("n_elements: {}, full: {}, empty: {}", buffer.n_elements(), buffer.is_full(), buffer.is_empty());
//     buffer.push_back(std::make_unique<int32_t>(900));
//     log::info("n_elements: {}, full: {}, empty: {}", buffer.n_elements(), buffer.is_full(), buffer.is_empty());
//     v = buffer.pop_front(); log::info("value: {}", *v);
//     v = buffer.pop_front(); log::info("value: {}", *v);
//     v = buffer.pop_front(); log::info("value: {}", *v);
//     v = buffer.pop_front(); log::info("value: {}", *v);

//     buffer.push_back(std::make_unique<int32_t>(100));
//     buffer.push_back(std::make_unique<int32_t>(200));
//     buffer.push_back(std::make_unique<int32_t>(300));
//     buffer.push_back(std::make_unique<int32_t>(400));
//     buffer.push_back(std::make_unique<int32_t>(500));
//     log::info("offset front 0: {}", *buffer.offset_front(0));
//     log::info("offset front 1: {}", *buffer.offset_front(1));
//     log::info("offset front 2: {}", *buffer.offset_front(2));
//     log::info("offset front 3: {}", *buffer.offset_front(3));
//     log::info("offset front 4: {}", *buffer.offset_front(4));
//     log::info("offset back 0: {}", *buffer.offset_back(0));
//     log::info("offset back 1: {}", *buffer.offset_back(1));
//     log::info("offset back 2: {}", *buffer.offset_back(2));
//     log::info("offset back 3: {}", *buffer.offset_back(3));
//     log::info("offset back 4: {}", *buffer.offset_back(4));
// }
