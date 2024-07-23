// SPDX-License-Identifier: Apache-2.0
#include <filesystem>


#include "xstudio/thumbnail/thumbnail.hpp"

using namespace xstudio;
using namespace xstudio::thumbnail;

namespace fs = std::filesystem;


void DiskCacheStat::populate(const std::string &path) {
    size_  = 0;
    count_ = 0;
    cache_.clear();
    for (const auto &entry : fs::recursive_directory_iterator(path)) {
        if (fs::is_regular_file(entry.status())) {
            auto mtime = fs::last_write_time(entry.path());
            add_thumbnail(
                std::stoull(entry.path().stem().string(), nullptr, 16),
                fs::file_size(entry.path()),
                mtime);
        }
    }
}

void DiskCacheStat::add_thumbnail(
    const size_t key, const size_t size, const fs::file_time_type &mtime) {

    // collisions ?
    if (cache_.count(key)) {
        // replace key..
        count_--;
        size_ -= cache_[key].first;
        cache_.erase(key);
    }

    count_++;
    size_ += size;

    cache_.insert(std::make_pair(key, std::make_pair(size, mtime)));
}

std::vector<size_t> DiskCacheStat::evict(const size_t max_size, const size_t max_count) {
    std::vector<size_t> evicted;

    if (max_size < size_ || max_count < count_) {
        // build map from cache
        // maps are order by key...
        // can get time collisions so bucket them
        std::map<fs::file_time_type, std::vector<size_t>> evict_map;
        for (const auto &i : cache_) {
            if (not evict_map.count(i.second.second))
                evict_map[i.second.second] = std::vector<size_t>();
            evict_map[i.second.second].push_back(i.first);
        }


        // oldest is at front of map.
        // keep adding to eviction list until we hit limits.
        while (size_ > max_size or count_ > max_count) {
            auto it = evict_map.begin();

            auto key = it->second.back();
            it->second.pop_back();
            if (it->second.empty())
                evict_map.erase(it);

            count_--;
            size_ -= cache_[key].first;

            evicted.push_back(key);
            cache_.erase(key);
        }
    }

    return evicted;
}

namespace {

void add_weighted_items(
    uint8_t *parOutBuffer, const uint8_t *parInBuffer, uint16_t parContribCoeff, int parCount) {
    using std::min;

    for (int z = 0; z < parCount; ++z) {
        const int new_value = parOutBuffer[z] + ((parInBuffer[z] * parContribCoeff) >> 8);
        parOutBuffer[z]     = static_cast<uint8_t>(min(255, new_value));
    }
}

// These utils should be improved. See notes below
float *xHalfSize(const int inWidth, const int inHeight, float *inBuffer, const int nchans) {

    const int sz = (inWidth * inHeight * nchans) / 2;

    auto tbuf = new float[sz];
    memset(tbuf, 0, sizeof(float) * sz);
    auto t_tbuf = tbuf;

    for (int i = 0; i < sz; i += nchans) {

        for (int c = 0; c < nchans; ++c) {
            *t_tbuf = (*inBuffer + *(inBuffer + nchans)) / 2.0f;
            t_tbuf++;
            inBuffer++;
        }
        inBuffer += nchans;
    }
    return tbuf;
}

float *yHalfSize(const int inWidth, const int inHeight, float *inBuffer, const int nchans) {

    const int sz = (inWidth * inHeight * nchans) / 2;

    auto tbuf = new float[sz];
    memset(tbuf, 0, sizeof(float) * sz);
    auto t_tbuf = tbuf;
    int step    = inWidth * nchans;
    for (int i = 0; i < inHeight / 2; ++i) {

        float *t_tbuf = &tbuf[i * inWidth * nchans];
        for (int j = 0; j < inWidth * nchans; ++j) {
            *t_tbuf = ((*inBuffer) + *(inBuffer + step)) / 2.0f;
            t_tbuf++;
            inBuffer++;
        }
        inBuffer += (inWidth * nchans);
    }

    return tbuf;
}

/* old & ugly utility for resizing an float * n chans image buffer*/
void quickSquash(
    float *outBuffer,
    const int outWidth,
    const int outHeight,
    const int inWidth,
    const int inHeight,
    const float *inBuffer,
    const int nchans) {

    float *tbuf;

    int width_ratio          = 1;
    int is_pow_2_width_ratio = 0;
    while (width_ratio < 100 && !is_pow_2_width_ratio) {

        width_ratio *= 2;
        if (width_ratio * outWidth == inWidth) {
            is_pow_2_width_ratio = 1;
        }
    }

    if (is_pow_2_width_ratio) {

        auto *squashed = new float[inWidth * inHeight * nchans];
        memcpy(squashed, inBuffer, inWidth * inHeight * nchans * sizeof(float));
        int width = inWidth;
        while (width != outWidth) {

            float *old_squashed = squashed;
            squashed            = xHalfSize(width, inHeight, old_squashed, nchans);
            if (old_squashed != inBuffer)
                delete[] old_squashed;
            width /= 2;
        }
        tbuf = squashed;

    } else {
        // temp buffer for the image that has been resized in x direction ..
        tbuf = new float[inHeight * outWidth * nchans];
        memset(tbuf, 0, sizeof(float) * inHeight * outWidth * nchans);

        float x_squeeze = (float)inWidth / (float)outWidth;

        // image is being shrunk in the x-direction...

        float x_pix_ratio = 1.0f / x_squeeze;
        float x_offset    = float(outWidth) / 2.0f - float(inWidth) * x_pix_ratio / 2.0f;

        std::vector<float> contrib_coeff1(inWidth);
        std::vector<float> contrib_coeff2(inWidth);
        std::vector<int> contrib_pos1(inWidth);
        std::vector<int> contrib_double_pos(inWidth);
        float xx = x_offset;

        // loop through input width. Each pixel contributes to one output pixel
        // or possibly two if the input pixel straddles the boundary between pixels
        // in the output. Record this info per pixel in a look-up
        for (int x = 0; x < inWidth; x++) {

            int contrib1 = (int)floor(xx);
            int contrib2 = (int)floor(xx + x_pix_ratio);

            if (contrib1 == contrib2 || x == (inWidth - 1)) {
                contrib_coeff1[x]     = x_pix_ratio;
                contrib_pos1[x]       = contrib1;
                contrib_double_pos[x] = 0;
            } else {
                contrib_coeff1[x]     = static_cast<float>(contrib2) - xx;
                contrib_coeff2[x]     = xx + x_pix_ratio - static_cast<float>(contrib2);
                contrib_pos1[x]       = contrib1;
                contrib_double_pos[x] = 1;
            }

            // check that we're not trying to write past the edge of the outpu image
            if (contrib_pos1[x] < 0) {
                contrib_pos1[x]       = 0;
                contrib_coeff1[x]     = 0;
                contrib_double_pos[x] = 0;
            } else if (contrib_pos1[x] >= outWidth) {
                contrib_pos1[x]       = outWidth - 1;
                contrib_coeff1[x]     = 0;
                contrib_double_pos[x] = 0;
            }

            xx += x_pix_ratio;
        }

        auto *buf_ref = (float *)inBuffer;
        for (int i = 0; i < inHeight; i++) {

            // loop through x pixels in the input and accumulate their
            // contribution to the output
            for (int j = 0; j < inWidth; j++) {
                if (contrib_double_pos[j]) {

                    int x_ref  = (contrib_pos1[j] + i * outWidth) * nchans;
                    int x_refp = x_ref + nchans;

                    for (int c = 0; c < nchans; ++c) {

                        tbuf[x_ref + c] += buf_ref[c] * contrib_coeff1[j];
                        tbuf[x_refp + c] += buf_ref[c] * contrib_coeff2[j];
                    }

                } else {

                    int x_ref = (contrib_pos1[j] + i * outWidth) * nchans;

                    for (int c = 0; c < nchans; ++c) {
                        tbuf[x_ref + c] += buf_ref[c] * contrib_coeff1[j];
                    }
                }

                buf_ref += nchans;
            }


            if (i == 10) {
            }
        }
    }

    int height_ratio          = 1;
    int is_pow_2_height_ratio = 0;
    while (height_ratio < 100 && !is_pow_2_height_ratio) {

        height_ratio *= 2;
        if (height_ratio * outHeight == inHeight) {
            is_pow_2_height_ratio = 1;
        }
    }

    if (is_pow_2_height_ratio) {

        float *squashed = tbuf;
        int height      = inHeight;
        while (height != outHeight) {

            float *old_squashed = squashed;
            squashed            = yHalfSize(outWidth, height, old_squashed, nchans);
            delete[] old_squashed;
            height /= 2;
        }
        memcpy(outBuffer, squashed, outHeight * outWidth * nchans * sizeof(float));
        delete[] squashed;
        return;
    }

    float y_squeeze   = (float)inHeight / (float)outHeight;
    float y_pix_ratio = 1.0f / y_squeeze; //(float)outWidth/(float)(inWidth);
    float y_offset    = float(outHeight) / 2.0f - float(inHeight) * y_pix_ratio / 2.0f;

    std::vector<float> contrib_coeff1(inHeight);
    std::vector<float> contrib_coeff2(inHeight);
    std::vector<int> contrib_pos1(inHeight);
    std::vector<int> contrib_double_pos(inHeight);

    float yy = y_offset;
    for (int y = 0; y < inHeight; y++) {

        int contrib1 = (int)floor(yy);
        int contrib2 = (int)floor(yy + y_pix_ratio);

        if (contrib1 == contrib2 || y == (inHeight - 1)) {
            contrib_coeff1[y]     = y_pix_ratio;
            contrib_pos1[y]       = contrib1;
            contrib_double_pos[y] = 0;
        } else {
            contrib_coeff1[y]     = static_cast<float>(contrib2) - yy;
            contrib_coeff2[y]     = yy + y_pix_ratio - static_cast<float>(contrib2);
            contrib_pos1[y]       = contrib1;
            contrib_double_pos[y] = 1;
        }

        if (contrib_pos1[y] < 0) {
            contrib_pos1[y]       = 0;
            contrib_coeff1[y]     = 0;
            contrib_double_pos[y] = 0;
        } else if (contrib_pos1[y] >= outHeight) {
            contrib_pos1[y]       = outHeight - 1;
            contrib_coeff1[y]     = 0;
            contrib_double_pos[y] = 0;
        } else if ((contrib_pos1[y] == (outHeight - 1)) && contrib_double_pos[y]) {
            contrib_double_pos[y] = 0;
        }

        yy += y_pix_ratio;
    }

    memset(outBuffer, 0, sizeof(float) * outWidth * outHeight * nchans);

    for (int j = 0; j < outWidth; j++) {

        int idx = j * nchans;

        for (int i = 0; i < inHeight; i++) {

            if (contrib_double_pos[i]) {

                int y_ref  = (j + contrib_pos1[i] * outWidth) * nchans;
                int y_refp = (j + (contrib_pos1[i] + 1) * outWidth) * nchans;

                for (int c = 0; c < nchans; ++c) {

                    outBuffer[y_ref + c] += tbuf[idx + c] * contrib_coeff1[i];
                    outBuffer[y_refp + c] += tbuf[idx + c] * contrib_coeff2[i];
                }

            } else {

                int y_ref = (j + contrib_pos1[i] * outWidth) * nchans;

                for (int c = 0; c < nchans; ++c)
                    outBuffer[y_ref + c] += tbuf[idx + c] * contrib_coeff1[i];
            }
            idx += outWidth * nchans;
        }
    }

    delete[] tbuf;
}

uint8_t *xHalfSize(const int inWidth, const int inHeight, uint8_t *inBuffer) {

    const int sz = (inWidth * inHeight * 3) / 2;

    // temp buffer for the image that has been resized in x direction ..
    auto *tbuf = new uint8_t[sz];
    memset(tbuf, 0, sizeof(uint8_t) * (sz));

    uint8_t *t_tbuf = tbuf;
    for (int i = 0; i < sz; i += 3) {

        *t_tbuf = uint8_t((uint16_t(*inBuffer) + uint16_t(*(inBuffer + 3))) >> 1);
        t_tbuf++;
        inBuffer++;
        *t_tbuf = uint8_t((uint16_t(*inBuffer) + uint16_t(*(inBuffer + 3))) >> 1);
        t_tbuf++;
        inBuffer++;
        *t_tbuf = uint8_t((uint16_t(*inBuffer) + uint16_t(*(inBuffer + 3))) >> 1);
        t_tbuf++;
        inBuffer += 4;
    }

    return tbuf;
}

uint8_t *yHalfSize(const int inWidth, const int inHeight, uint8_t *inBuffer) {

    const int sz = (inWidth * inHeight * 3) / 2;

    // temp buffer for the image that has been resized in x direction ..
    auto *tbuf = new uint8_t[sz];
    memset(tbuf, 0, sizeof(uint8_t) * (sz));

    int step = inWidth * 3;
    for (int i = 0; i < inHeight / 2; ++i) {

        uint8_t *t_tbuf = tbuf + i * inWidth * 3;
        for (int j = 0; j < inWidth * 3; ++j) {
            *t_tbuf = uint8_t((uint16_t(*inBuffer) + uint16_t(*(inBuffer + step))) >> 1);
            t_tbuf++;
            inBuffer++;
        }
        inBuffer += (inWidth * 3);
    }

    return tbuf;
}

/* old & ugly utility for resizing an RGB24 image buffer*/
void quickSquash(
    uint8_t *outBuffer,
    const int outWidth,
    const int outHeight,
    const int inWidth,
    const int inHeight,
    const uint8_t *inBuffer,
    const int nchans) {
    uint8_t *tbuf;

    int width_ratio          = 1;
    int is_pow_2_width_ratio = 0;
    while (width_ratio < 100 && !is_pow_2_width_ratio) {

        width_ratio *= 2;
        if (width_ratio * outWidth == inWidth) {
            is_pow_2_width_ratio = 1;
        }
    }

    if (is_pow_2_width_ratio) {

        auto *squashed = new uint8_t[inWidth * inHeight * nchans];
        memcpy(squashed, inBuffer, inWidth * inHeight * nchans * sizeof(uint8_t));
        int width = inWidth;
        while (width != outWidth) {

            uint8_t *old_squashed = squashed;
            squashed              = xHalfSize(width, inHeight, old_squashed);
            if (old_squashed != inBuffer)
                delete[] old_squashed;
            width /= 2;
        }
        tbuf = squashed;

    } else {
        // temp buffer for the image that has been resized in x direction ..
        tbuf = new uint8_t[inHeight * outWidth * 3];
        memset(tbuf, 0, inHeight * outWidth * 3);

        float x_squeeze = (float)inWidth / (float)outWidth;

        // image is being shrunk in the x-direction...

        const float x_pix_ratio = 1.0f / x_squeeze;
        float x_offset          = float(outWidth) / 2.0f - float(inWidth) * x_pix_ratio / 2.0f;

        std::vector<uint16_t> contrib_coeff1(inWidth);
        std::vector<uint16_t> contrib_coeff2(inWidth);
        std::vector<int> contrib_pos1(inWidth);
        std::vector<int> contrib_double_pos(inWidth);
        float xx = x_offset;

        // loop through input width. Each pixel contributes to one output pixel
        // or possibly two if the input pixel straddles the boundary between pixels
        // in the output. Record this info per pixel in a look-up
        for (int x = 0; x < inWidth; x++) {

            int contrib1 = static_cast<int>(std::floor(xx));
            int contrib2 = static_cast<int>(std::floor(xx + x_pix_ratio));

            if (contrib1 == contrib2 || x == (inWidth - 1)) {
                contrib_coeff1[x]     = static_cast<uint16_t>(roundf(x_pix_ratio * 255.0f));
                contrib_pos1[x]       = contrib1;
                contrib_double_pos[x] = 0;
            } else {
                contrib_coeff1[x] =
                    static_cast<uint16_t>(roundf((static_cast<float>(contrib2) - xx) * 255.0f));
                contrib_coeff2[x] = static_cast<uint16_t>(
                    roundf((xx + x_pix_ratio - static_cast<float>(contrib2)) * 255.0f));
                contrib_pos1[x]       = contrib1;
                contrib_double_pos[x] = 1;
            }

            // check that we're not trying to write past the edge of the outpu image
            if (contrib_pos1[x] < 0) {
                contrib_pos1[x]       = 0;
                contrib_coeff1[x]     = 0;
                contrib_double_pos[x] = 0;
            } else if (contrib_pos1[x] >= outWidth) {
                contrib_pos1[x]       = outWidth - 1;
                contrib_coeff1[x]     = 0;
                contrib_double_pos[x] = 0;
            }

            xx += x_pix_ratio;
        }

        auto *buf_ref = (uint8_t *)inBuffer;
        for (int i = 0; i < inHeight; i++) {

            // loop through x pixels in the input and accumulate their
            // contribution to the output
            for (int j = 0; j < inWidth; j++) {

                if (contrib_double_pos[j]) {

                    int x_ref  = (contrib_pos1[j] + i * outWidth) * 3;
                    int x_refp = x_ref + 3;

                    add_weighted_items(&tbuf[x_ref], buf_ref, contrib_coeff1[j], 3);
                    add_weighted_items(&tbuf[x_refp], buf_ref, contrib_coeff2[j], 3);
                } else {
                    int x_ref = (contrib_pos1[j] + i * outWidth) * 3;

                    add_weighted_items(&tbuf[x_ref], buf_ref, contrib_coeff1[j], 3);
                }

                buf_ref += 3;
            }
        }
    }

    int height_ratio          = 1;
    int is_pow_2_height_ratio = 0;
    while (height_ratio < 100 && !is_pow_2_height_ratio) {

        height_ratio *= 2;
        if (height_ratio * outHeight == inHeight) {
            is_pow_2_height_ratio = 1;
        }
    }

    if (is_pow_2_height_ratio) {

        uint8_t *squashed = tbuf;
        int height        = inHeight;
        while (height != outHeight) {

            uint8_t *old_squashed = squashed;
            squashed              = yHalfSize(outWidth, height, old_squashed);
            delete[] old_squashed;
            height /= 2;
        }
        memcpy(outBuffer, squashed, outWidth * outHeight * nchans);
        delete[] squashed;
        return;
    }

    std::vector<uint16_t> contrib_coeff1(inHeight);
    std::vector<uint16_t> contrib_coeff2(inHeight);
    std::vector<int> contrib_pos1(inHeight);
    std::vector<int> contrib_double_pos(inHeight);

    const float y_pix_ratio = static_cast<float>(outHeight) / static_cast<float>(inHeight);
    float yy                = 0.0f;
    for (int y = 0; y < inHeight; y++) {

        const int contrib1 = static_cast<int>(yy);
        const int contrib2 = static_cast<int>(yy + y_pix_ratio);

        if (contrib1 == contrib2 || y == (inHeight - 1)) {
            contrib_coeff1[y]     = uint16_t(roundf(y_pix_ratio * 255.0f));
            contrib_pos1[y]       = contrib1;
            contrib_double_pos[y] = 0;
        } else {
            contrib_coeff1[y] = uint16_t(roundf((static_cast<float>(contrib2) - yy) * 255.0f));
            contrib_coeff2[y] =
                uint16_t(roundf((yy + y_pix_ratio - static_cast<float>(contrib2)) * 255.0f));
            contrib_pos1[y]       = contrib1;
            contrib_double_pos[y] = 1;
        }

        if (contrib_pos1[y] < 0) {
            contrib_pos1[y]       = 0;
            contrib_coeff1[y]     = 0;
            contrib_double_pos[y] = 0;
        } else if (contrib_pos1[y] >= outHeight) {
            contrib_pos1[y]       = outHeight - 1;
            contrib_coeff1[y]     = 0;
            contrib_double_pos[y] = 0;
        } else if ((contrib_pos1[y] == (outHeight - 1)) && contrib_double_pos[y]) {
            contrib_double_pos[y] = 0;
        }

        yy += y_pix_ratio;
    }

    for (int j = 0; j < outWidth; j++) {

        int idx = j * 3;

        for (int i = 0; i < inHeight; i++) {

            if (contrib_double_pos[i]) {

                int y_ref  = (j + contrib_pos1[i] * outWidth) * 3;
                int y_refp = (j + (contrib_pos1[i] + 1) * outWidth) * 3;

                add_weighted_items(&outBuffer[y_ref], &tbuf[idx], contrib_coeff1[i], 3);
                add_weighted_items(&outBuffer[y_refp], &tbuf[idx], contrib_coeff2[i], 3);
            } else {
                int y_ref = (j + contrib_pos1[i] * outWidth) * 3;

                add_weighted_items(&outBuffer[y_ref], &tbuf[idx], contrib_coeff1[i], 3);
            }
            idx += outWidth * 3;
        }
    }
}

} // namespace

/* Here I am using some ooooollllldddd rubbishy code for bilin image squash.
We should update to use a superior library or refactor */
void ThumbnailBuffer::bilin_resize(const size_t new_width, const size_t new_height) {

    if (new_width == width_ && new_height == height_)
        return;

    std::vector<std::byte> new_buffer;
    if (format_ == TF_RGBF96) {
        new_buffer.resize(new_width * new_height * channels_ * channel_size_);
        quickSquash(
            (float *)new_buffer.data(),
            new_width,
            new_height,
            width_,
            height_,
            reinterpret_cast<const float *>(buffer_.data()),
            channels_);
    } else {
        new_buffer.resize(new_width * new_height * channels_ * channel_size_);
        quickSquash(
            (uint8_t *)new_buffer.data(),
            new_width,
            new_height,
            width_,
            height_,
            reinterpret_cast<const uint8_t *>(buffer_.data()),
            channels_);
    }

    width_  = new_width;
    height_ = new_height;
    std::swap(buffer_, new_buffer);
}

void ThumbnailBuffer::convert_to(const THUMBNAIL_FORMAT format) {
    if (format == format_)
        return;
    std::vector<std::byte> new_buffer;
    if (format_ == TF_RGBF96 && format == TF_RGB24) {
        new_buffer.resize(width_ * height_ * channels_);
        const auto *in = (const float *)buffer_.data();
        auto *out      = (uint8_t *)new_buffer.data();
        size_t sz      = width_ * height_ * channels_;
        while (sz--) {
            (*out++) = (uint8_t)(std::max(0, std::min(255, (int)round(*(in++) * 255.0f))));
        }
    } else if (format_ == TF_RGB24 && format == TF_RGBF96) {
        new_buffer.resize(width_ * height_ * channels_ * sizeof(float));
        const auto *in = (const uint8_t *)buffer_.data();
        auto *out      = (float *)new_buffer.data();
        size_t sz      = width_ * height_ * channels_;
        while (sz--) {
            (*out++) = float(*(in++) * (1.0f / 255.0f));
        }
    };
    std::swap(buffer_, new_buffer);
    format_ = format;
}

void ThumbnailBuffer::flip() {
    if (format_ == TF_RGBF96) {
        auto *in      = (float *)buffer_.data();
        float *in_top = ((float *)buffer_.data()) + (height_ - 1) * width_ * channels_;
        std::vector<float> tmp(width_ * channels_);
        size_t sz = (height_ / 2);
        while (sz--) {
            memcpy(tmp.data(), in_top, width_ * channels_ * sizeof(float));
            memcpy(in_top, in, width_ * channels_ * sizeof(float));
            memcpy(in, tmp.data(), width_ * channels_ * sizeof(float));
            in += width_ * channels_;
            in_top -= width_ * channels_;
        }
    } else if (format_ == TF_RGB24) {
        auto *in        = (uint8_t *)buffer_.data();
        uint8_t *in_top = ((uint8_t *)buffer_.data()) + (height_ - 1) * width_ * channels_;
        size_t sz       = (height_ / 2);
        std::vector<float> tmp(width_ * channels_);
        while (sz--) {
            memcpy(tmp.data(), in_top, width_ * channels_ * sizeof(uint8_t));
            memcpy(in_top, in, width_ * channels_ * sizeof(uint8_t));
            memcpy(in, tmp.data(), width_ * channels_ * sizeof(uint8_t));
            in += width_ * channels_;
            in_top -= width_ * channels_;
        }
    };
}
