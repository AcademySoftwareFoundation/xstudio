// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cmath>
#include <stdexcept>
#include <vector>

namespace xstudio::audio {

/**
 *  @brief A simple ring buffer, used by audio output to efficiently buffer samples
 *  for streaming to the soundcard
 */

template <typename T> class AudioSampleDataFIFO {

  public:
    /**
     *  @brief Constructor
     *
     *  @details Set the total capacity of the buffer on construction.
     */
    AudioSampleDataFIFO(const size_t sz = 8192 * 4) { buffer_.resize(sz); }


    /**
     *  @brief Return the number of elements of data in the buffer
     *
     */
    [[nodiscard]] size_t size() const {
        return (start_ > end_) ? (buffer_.size() - start_) + end_ : end_ - start_;
    }


    /**
     *  @brief Return the number of elements available for appending
     *
     */
    [[nodiscard]] size_t spare_capacity() const { return buffer_.size() - size(); }


    /**
     *  @brief Append 'count' elements to the buffer and initialise with zeros
     *
     */
    void push_zeros(const size_t count);


    /**
     *  @brief Append data to the buffer from a pointer
     *
     */
    void push(T *b, size_t count, const bool reversed);


    /**
     *  @brief Append data to the buffer from a pointer including a smooth cross
     *  blend with the tail samples in the buffer
     *
     *  @details The input buffer b must have window_size elements available in
     *  front of the pointer, these are blended with a sigmoid curve into
     *  window_size elements at the tail of the existing data. Then 'count'
     *  elements are appended from b.
     */
    void blended_push(T *b, size_t count, const size_t window_size, const bool reversed);


    /**
     *  @brief Remove data from the front of the buffer and place in destination vector
     *
     *  @details If destination has more elements than the ringbuffer has elements, only
     *  the elements available are copied.
     */
    inline void pop(std::vector<T> &dest);

    /**
     *  @brief Return the total cumulative amount of elements that have passed through
     *  the ring buffer
     */
    [[nodiscard]] double cumulative_elements() const { return (double)position_; }

  private:
    std::vector<T> buffer_;
    size_t start_  = {0};
    size_t end_    = {1024};
    long position_ = {1024};
};


template <typename T> void AudioSampleDataFIFO<T>::push_zeros(const size_t count) {
    if ((buffer_.size() - end_) > count) {
        memset(buffer_.data() + end_, 0, sizeof(T) * count);
        end_ += count;
    } else {
        memset(buffer_.data() + end_, 0, sizeof(T) * (buffer_.size() - end_));
        end_ = count - (buffer_.size() - end_);
        memset(buffer_.data(), 0, sizeof(T) * end_);
    }
    position_ += count;
}

template <typename T>
void AudioSampleDataFIFO<T>::push(T *b, size_t count, const bool forwards) {

    if (count > spare_capacity()) {
        throw std::runtime_error("Ringbuffer overrun.");
    }
    if (forwards) {
        if ((buffer_.size() - end_) > count) {
            memcpy(buffer_.data() + end_, b, count * sizeof(T));
            end_ += count;
        } else {
            memcpy(buffer_.data() + end_, b, (buffer_.size() - end_) * sizeof(T));
            b += buffer_.size() - end_;
            memcpy(buffer_.data(), b, (count - (buffer_.size() - end_)) * sizeof(T));
            end_ = count - (buffer_.size() - end_);
        }
    } else {
        b = b + count - 1;
        if ((buffer_.size() - end_) > count) {
            T *r  = buffer_.data() + end_ - 1;
            int c = count;
            while (c--) {
                *(r++) = *(b--);
            }
            end_ += count;
        } else {
            int c = (count - (buffer_.size() - end_));
            T *r  = buffer_.data();
            while (c--) {
                *(r++) = *(b--);
            }
            c = (buffer_.size() - end_);
            r = buffer_.data() + end_;
            while (c--) {
                *(r++) = *(b--);
            }
            end_ = count - (buffer_.size() - end_);
        }
    }
    position_ += count;
}

template <typename T>
void AudioSampleDataFIFO<T>::blended_push(
    T *b, size_t count, const size_t window_size, const bool /*forwards*/) {

    if (window_size > size()) {
        throw std::runtime_error("Ringbuffer overrun (blended_push).");
    }
    T *p      = buffer_.data() + end_;
    int c     = window_size;
    size_t ee = end_;
    while (c--) {
        const float x   = 6.0f * (c - window_size / 2) / window_size;
        const float sig = 1.0f / (1.0f + powf(M_E, -x));
        const float rr  = std::min(
            std::max(
                float(*p) * (1.0f - sig) + float(*b) * sig,
                static_cast<float>(std::numeric_limits<T>::lowest())),
            static_cast<float>(std::numeric_limits<T>::max()));
        *p = static_cast<T>(rr);
        p--;
        b--;
        if (ee == 0) {
            ee = buffer_.size() - 1;
            p  = buffer_.data() + ee;
        } else {
            ee--;
        }
    }
    b += window_size;
    push(b, count, true);
}

template <typename T> void AudioSampleDataFIFO<T>::pop(std::vector<T> &dest) {

    if (end_ < start_) {

        memcpy(
            dest.data(),
            buffer_.data() + start_,
            std::min(dest.size(), buffer_.size() - start_) * sizeof(T));

        if (dest.size() > (buffer_.size() - start_)) {
            memcpy(
                dest.data() + (buffer_.size() - start_),
                buffer_.data(),
                std::min(dest.size() - (buffer_.size() - start_), end_) * sizeof(T));
            start_ = std::min(dest.size() - (buffer_.size() - start_), end_);
        } else {
            start_ += dest.size();
        }

    } else {

        memcpy(
            dest.data(),
            buffer_.data() + start_,
            std::min(dest.size(), end_ - start_) * sizeof(T));
        start_ += std::min(dest.size(), end_ - start_);
    }
}

} // namespace xstudio::audio
