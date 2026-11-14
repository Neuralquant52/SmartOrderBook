#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

template <typename T, std::size_t capacity = 256>
class Buffer {
    static_assert(std::has_single_bit(capacity), "Capacity must be a power of 2");
    static constexpr std::size_t MASK = capacity - 1;

public:
    void push(T item) {
        std::scoped_lock lock(mtx_);
        data_[head_] = std::move(item);
        head_ = (head_ + 1) & MASK;
        if (count_ < capacity) ++count_;
        ++total_;
    }

    std::size_t size() const {
        std::scoped_lock lock(mtx_);
        return count_;
    }

    std::size_t totalPushed() const {
        std::scoped_lock lock(mtx_);
        return total_;
    }

    T back() const {
        std::scoped_lock lock(mtx_);

        return data_[(head_ - 1) & MASK];
    }

    std::vector<T> snapshot() const {
        std::scoped_lock lock(mtx_);
        std::vector<T> out;
        out.reserve(count_);
        const std::size_t start = (head_ - count_) & MASK;
        for (std::size_t i = 0; i < count_; ++i)
            out.push_back(data_[(start + i) & MASK]);
        return out;
    }

    void clear() {
        std::scoped_lock lock(mtx_);
        head_ = count_ = total_ = 0;
    }
	
    bool empty(){
	return total_ == 0;	
    }
private:
    mutable std::mutex mtx_;
    std::size_t head_{}, count_{}, total_{};
    std::array<T, capacity> data_{};
};
