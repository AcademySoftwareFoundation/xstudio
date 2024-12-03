// SPDX-License-Identifier: Apache-2.0
#include <functional>

#include <cstdio>
#ifdef _WIN32
// required to define INT32 type used by jpeglib
#include <basetsd.h>
#endif
#include <jpeglib.h>
#include <fstream>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/thumbnail/thumbnail_disk_cache_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace caf;
using namespace xstudio::thumbnail;
using namespace xstudio::utility;
using namespace xstudio;


// jpg std::vector

struct stdvector_destination_mgr {
    struct jpeg_destination_mgr pub_;       // public fields
    std::vector<std::byte> *vec_ = nullptr; // destination vector
};

void init_stdvector_destination(j_compress_ptr /*cinfo*/) {
    // Nothing to do
}

boolean empty_stdvector_output_buffer(j_compress_ptr cinfo) {
    auto *dest = reinterpret_cast<stdvector_destination_mgr *>(cinfo->dest);

    // Double vector capacity
    const auto currentSize = dest->vec_->size();
    dest->vec_->resize(currentSize * 2);

    // Point to newly allocated data
    dest->pub_.next_output_byte = reinterpret_cast<JOCTET *>(dest->vec_->data()) + currentSize;
    dest->pub_.free_in_buffer   = currentSize;

    return TRUE;
}

void term_stdvector_destination(j_compress_ptr cinfo) {
    auto *dest = reinterpret_cast<stdvector_destination_mgr *>(cinfo->dest);

    // Resize vector to number of bytes actually used
    const auto used_bytes = dest->vec_->capacity() - dest->pub_.free_in_buffer;
    dest->vec_->resize(used_bytes);
}

void jpeg_stdvector_dest(j_compress_ptr cinfo, std::vector<std::byte> &vec) {
    if (cinfo->dest == nullptr) {
        cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)(
            (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(stdvector_destination_mgr));
    }

    auto *dest                     = reinterpret_cast<stdvector_destination_mgr *>(cinfo->dest);
    dest->pub_.init_destination    = init_stdvector_destination;
    dest->pub_.empty_output_buffer = empty_stdvector_output_buffer;
    dest->pub_.term_destination    = term_stdvector_destination;

    // Set output buffer and initial size
    dest->vec_ = &vec;
    dest->vec_->resize(4096);

    // Initialize public buffer ptr and size
    dest->pub_.next_output_byte = reinterpret_cast<JOCTET *>(dest->vec_->data());
    dest->pub_.free_in_buffer   = dest->vec_->size();
}

// The UI will fetch thumbnails as fast as it can by making many concurrent
// requests for thumbnails. This is fine if we're reading from the thumb RAM/disk
// cache as its very fast and not too IO intensive. However, if we are generating
// new thumbanils this can be expensive for media_reader actors and can slow or
// block playback. To get around this, we pass requests for thumb generation
// through this middleman which uses request().await() on thumb generation, so
// that we are only sending one thumb generation request at a time to the
// global media reader but also not blocking the ThumbnailDiskCacheActor
class ThumbGenMiddleman : public caf::event_based_actor {
  public:
    ThumbGenMiddleman(caf::actor_config &cfg);
    ~ThumbGenMiddleman() override = default;

    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "ThumbGenMiddleman";
    caf::behavior make_behavior() override { return behavior_; }

  private:
    caf::behavior behavior_;
};

ThumbGenMiddleman::ThumbGenMiddleman(caf::actor_config &cfg) : caf::event_based_actor(cfg) {

    auto global_reader = system().registry().template get<caf::actor>(media_reader_registry);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](media_reader::get_thumbnail_atom,
            const media::AVFrameID &mptr,
            const size_t thumb_size) -> result<ThumbnailBufferPtr> {
            auto rp = make_response_promise<ThumbnailBufferPtr>();
            if (not global_reader) {
                rp.deliver(make_error(xstudio_error::error, "No readers available"));
            } else {
                request(
                    global_reader,
                    infinite,
                    media_reader::get_thumbnail_atom_v,
                    mptr,
                    thumb_size)
                    .then(
                        [=](const ThumbnailBufferPtr &buf) mutable { rp.deliver(buf); },
                        [=](const caf::error &err) mutable { rp.deliver(err); });
            }
            return rp;
        });
}


fs::path TDCHelperActor::thumbnail_path(const std::string &path, const size_t thumbkey) {
    auto key = to_hash_string(thumbkey);
    return fs::path(path) / key.substr(0, 1) / (key + ".jpg");
}

ThumbnailBufferPtr TDCHelperActor::read_decode_thumb(const std::string &path) {
    auto fdt = [](FILE *fp) { fclose(fp); };
    std::unique_ptr<FILE, decltype(fdt)> infile(fopen(path.c_str(), "rb"), fdt);
    if (infile.get() == nullptr)
        throw std::runtime_error("Could not open " + path);

    // failed seek.
    if (auto rt = fseek(infile.get(), 0, SEEK_END))
        throw std::runtime_error(fmt::format("{} Failed seek {}", __PRETTY_FUNCTION__, rt));

    auto size = ftell(infile.get());

    if (size < 0)
        throw std::runtime_error(fmt::format("{} Failed ftell {}", __PRETTY_FUNCTION__, size));

    if (auto rt = fseek(infile.get(), 0, SEEK_SET))
        throw std::runtime_error(fmt::format("{} Failed seek {}", __PRETTY_FUNCTION__, rt));

    auto buffer = std::vector<std::byte>(size);

    auto bytes_read = fread(buffer.data(), 1, size, infile.get());
    if (bytes_read != static_cast<size_t>(size))
        throw std::runtime_error(
            fmt::format("{} Failed read {}", __PRETTY_FUNCTION__, bytes_read));

    return decode_thumb(buffer);
}

size_t
TDCHelperActor::encode_save_thumb(const std::string &path, const ThumbnailBufferPtr &buffer) {

    auto fdt = [](FILE *fp) { fclose(fp); };
    std::unique_ptr<FILE, decltype(fdt)> outfile(fopen(path.c_str(), "wb"), fdt);
    if (outfile.get() == nullptr)
        throw std::runtime_error("Could not open " + path);

    auto buf     = encode_thumb(buffer);
    auto written = fwrite(buf.data(), 1, buf.size(), outfile.get());
    if (written != buf.size()) {
        spdlog::warn(
            "{} Error writing thumbnail {} {} != {}",
            __PRETTY_FUNCTION__,
            path,
            written,
            buf.size());
    }

    return buf.size();
}

std::vector<std::byte>
TDCHelperActor::encode_thumb(const ThumbnailBufferPtr &buffer, const int quality) {
    auto result = std::vector<std::byte>();

    // Creating a custom deleter for the compressInfo pointer
    // to ensure ::jpeg_destroy_compress() gets called even if
    // we throw out of this function.
    struct jpeg_error_mgr m_errorMgr;

    auto dt = [](::jpeg_compress_struct *cs) { ::jpeg_destroy_compress(cs); };
    std::unique_ptr<::jpeg_compress_struct, decltype(dt)> compressInfo(
        new ::jpeg_compress_struct, dt);
    ::jpeg_create_compress(compressInfo.get());

    compressInfo->image_width      = buffer->width();
    compressInfo->image_height     = buffer->height();
    compressInfo->input_components = 3;
    compressInfo->in_color_space   = ::JCS_RGB;
    compressInfo->err              = ::jpeg_std_error(&m_errorMgr);
    ::jpeg_set_defaults(compressInfo.get());
    ::jpeg_set_quality(compressInfo.get(), quality, TRUE);

    jpeg_stdvector_dest(compressInfo.get(), result);

    ::jpeg_start_compress(compressInfo.get(), TRUE);
    size_t row_stride = buffer->width() * 3;

    for (size_t i = 0; i < buffer->height(); i++) {
        std::array<::JSAMPROW, 1> rowPtr;
        // Casting const-ness away here because the jpeglib
        // call expects a non-const pointer. It presumably
        // doesn't modify our data.
        rowPtr[0] = reinterpret_cast<::JSAMPROW>(&(buffer->data()[row_stride * i]));
        ::jpeg_write_scanlines(compressInfo.get(), rowPtr.data(), 1);
    }
    ::jpeg_finish_compress(compressInfo.get());

    return result;
}


ThumbnailBufferPtr TDCHelperActor::decode_thumb(const std::vector<std::byte> &buffer) {
    auto dt = [](::jpeg_decompress_struct *ds) {
        ::jpeg_destroy_decompress(ds);
        delete ds;
    };
    std::unique_ptr<::jpeg_decompress_struct, decltype(dt)> decompressInfo(
        new ::jpeg_decompress_struct, dt);

    // Note this is a shared pointer as we can share this
    // between objects which have copy constructed from each other
    struct jpeg_error_mgr m_errorMgr;

    decompressInfo->err = ::jpeg_std_error(&m_errorMgr);
    // Note this usage of a lambda to provide our own error handler
    // to libjpeg. If we do not supply a handler, and libjpeg hits
    // a problem, it just prints the error message and calls exit().
    m_errorMgr.error_exit = [](::j_common_ptr cinfo) {
        std::array<char, JMSG_LENGTH_MAX> jpegLastErrorMsg;
        // Call the function pointer to get the error message
        (*(cinfo->err->format_message))(cinfo, jpegLastErrorMsg.data());
        throw std::runtime_error(jpegLastErrorMsg.data());
    };
    ::jpeg_create_decompress(decompressInfo.get());

    // Read the file:
    ::jpeg_mem_src(
        decompressInfo.get(),
        reinterpret_cast<const unsigned char *>(buffer.data()),
        buffer.size());

    int rc = ::jpeg_read_header(decompressInfo.get(), TRUE);
    if (rc != 1) {
        throw std::runtime_error("File does not seem to be a normal JPEG");
    }
    ::jpeg_start_decompress(decompressInfo.get());

    // buffer allocated here..
    auto result = std::make_shared<ThumbnailBuffer>(
        static_cast<size_t>(decompressInfo->output_width),
        static_cast<size_t>(decompressInfo->output_height));

    size_t pixel_size = decompressInfo->output_components;

    if (pixel_size == 3) {
        size_t row_stride = result->width() * 3;

        // should match ..
        std::byte *p = &(result->data()[0]);
        while (decompressInfo->output_scanline < result->height()) {
            ::jpeg_read_scanlines(decompressInfo.get(), reinterpret_cast<JSAMPARRAY>(&p), 1);
            p += row_stride;
        }
        ::jpeg_finish_decompress(decompressInfo.get());
    } else if (pixel_size == 1) {
        size_t row_stride = result->width() * 3;

        char *mono = new char[result->width()];

        // should match ..
        std::byte *p = &(result->data()[0]);
        while (decompressInfo->output_scanline < result->height()) {
            ::jpeg_read_scanlines(decompressInfo.get(), reinterpret_cast<JSAMPARRAY>(&mono), 1);

            for (auto i = 0; i < result->width(); i++) {
                p[i * 3] = p[(i * 3) + 1] = p[(i * 3) + 2] =
                    static_cast<std::byte>(*(mono + i));
            }

            p += row_stride;
        }
        ::jpeg_finish_decompress(decompressInfo.get());

        delete[] mono;

    } else {
        // ::jpeg_finish_decompress(decompressInfo.get());
        throw std::runtime_error(
            "Invalid pixel size " + std::to_string(pixel_size) + " != 3 or 1");
    }

    return result;
}

// if we hit this actor then there'll be IO
// so don't get too carried away with optimising, as it'll be pointless
TDCHelperActor::TDCHelperActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {
    print_on_exit(this, "TDCHelperActor");

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](media_reader::get_thumbnail_atom,
            const std::string &path,
            const size_t thumb) -> result<ThumbnailBufferPtr> {
            try {
                fs::last_write_time(
                    thumbnail_path(path, thumb), std::filesystem::file_time_type::clock::now());
                return read_decode_thumb(thumbnail_path(path, thumb).string());
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }

            return ThumbnailBufferPtr();
        },

        [=](media_reader::get_thumbnail_atom,
            const ThumbnailBufferPtr &buffer,
            const int quality) -> result<std::vector<std::byte>> {
            try {
                return encode_thumb(buffer, quality);
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }
        },

        [=](media_reader::get_thumbnail_atom,
            const ThumbnailBufferPtr &buffer) -> result<std::vector<std::byte>> {
            try {
                return encode_thumb(buffer);
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }
        },

        [=](media_reader::get_thumbnail_atom,
            const std::vector<std::byte> &buffer) -> result<ThumbnailBufferPtr> {
            try {
                return decode_thumb(buffer);
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }
        },

        [=](media_cache::store_atom,
            const std::string &path,
            const size_t thumb,
            const ThumbnailBufferPtr &buffer) -> result<size_t> {
            try {
                auto thumb_path = thumbnail_path(path, thumb);
                // make sure dest exists..
                if (not fs::exists(thumb_path.parent_path())) {
                    if (not fs::create_directories(thumb_path.parent_path())) {
                        return make_error(
                            xstudio_error::error,
                            "Failed to create cache directory. " +
                                thumb_path.parent_path().string());
                    }
                }
                return encode_save_thumb(thumbnail_path(path, thumb).string(), buffer);
                
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }
            return false;
        },

        [=](cache_stats_atom, const std::string &path) -> result<DiskCacheStat> {
            // recursive scan of dir
            // build count and size totals.
            auto dcs = DiskCacheStat();
            try {
                dcs.populate(path);
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }

            return dcs;
        },
        [=](media_cache::erase_atom atom, const std::string &path, const size_t &thumb) {
            delegate(caf::actor_cast<caf::actor>(this), atom, path, std::vector<size_t>{thumb});
        },
        [=](media_cache::erase_atom,
            const std::string &path,
            const std::vector<size_t> &thumbs) -> result<bool> {
            // build path to thumb.
            for (const auto &i : thumbs) {
                // pick first char of thumb...
                try {
                    fs::remove(thumbnail_path(path, i));
                } catch (const std::exception &err) {
                    return make_error(xstudio_error::error, err.what());
                }
            }
            return true;
        });
}

ThumbnailDiskCacheActor::ThumbnailDiskCacheActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg) {
    print_on_exit(this, "ThumbnailDiskCacheActor");

    auto event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);


    pool_ = caf::actor_pool::make(
        system().dummy_execution_unit(),
        5,
        [&] { return system().spawn<TDCHelperActor>(); },
        caf::actor_pool::round_robin());
    link_to(pool_);

    thumb_gen_middleman_ = system().registry().template get<caf::actor>(media_reader_registry);


    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](utility::clear_atom) -> bool {
            evict_thumbnails(cache_.evict(0, 0));
            return true;
        },

        // convert to jpg
        [=](media_reader::get_thumbnail_atom,
            const ThumbnailBufferPtr &buffer,
            const int quality) {
            delegate(pool_, media_reader::get_thumbnail_atom_v, buffer, quality);
        },

        // convert to jpg
        [=](media_reader::get_thumbnail_atom, const ThumbnailBufferPtr &buffer) {
            delegate(pool_, media_reader::get_thumbnail_atom_v, buffer);
        },

        // convert to ThumbnailBufferPtr
        [=](media_reader::get_thumbnail_atom, const std::vector<std::byte> &buffer) {
            delegate(pool_, media_reader::get_thumbnail_atom_v, buffer);
        },

        [=](media_reader::get_thumbnail_atom,
            const media::AVFrameID &mptr,
            const size_t thumb_size,
            const size_t hash,
            const bool cache_to_disk) -> result<ThumbnailBufferPtr> {
            auto rp       = make_response_promise<ThumbnailBufferPtr>();
            auto thumbkey = ThumbnailKey(mptr, hash, thumb_size);
            // check for file in cache
            // spdlog::warn("{} {} {} {} {}", to_string(mptr.uri()), mptr.frame(), hash,
            // thumbkey.hash(), cache_.cache_.count(thumbkey.hash()));
            if (cache_.cache_.count(thumbkey.hash()))
                request_read_of_thumbnail(rp, thumbkey.hash());
            else
                request_generation_of_thumbnail(rp, mptr, thumb_size, hash, cache_to_disk);

            return rp;
        },

        [=](media_cache::count_atom, const size_t max_count) {
            max_cache_count_ = max_count;
            evict_thumbnails(
                cache_.evict(std::numeric_limits<size_t>::max(), max_cache_count_));
        },
        [=](media_cache::count_atom) -> size_t { return cache_.count_; },

        [=](media_cache::size_atom, const size_t max_size) {
            max_cache_size_ = max_size;
            evict_thumbnails(cache_.evict(max_cache_size_, std::numeric_limits<size_t>::max()));
        },
        [=](media_cache::size_atom) -> size_t { return cache_.size_; },

        [=](thumbnail::cache_path_atom) -> caf::uri { return cache_path_pref_; },

        [=](thumbnail::cache_path_atom, const caf::uri &uri) -> result<bool> {
            // if create fails then don't change return fault.
            try {
                fs::path fspath;
                try {
                    fspath = fs::path(expand_envvars(uri_to_posix_path(uri)));
                    if (not fs::exists(fspath)) {
                        if (not fs::create_directories(fspath)) {
                            return make_error(
                                xstudio_error::error,
                                "Failed to create cache directory. " + fspath.string());
                        }
                    }
                } catch (const std::exception &err) {
                    return make_error(
                        xstudio_error::error,
                        std::string("Failed to create cache directory. ") + err.what());
                }
                cache_path_pref_ = uri;
                cache_path_      = fspath;
                cache_           = DiskCacheStat();
                request(pool_, infinite, cache_stats_atom_v, cache_path_.string())
                    .then(
                        [=](const DiskCacheStat &dcs) { cache_ = dcs; },
                        [=](const caf::error &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            } catch (const std::exception &err) {
                return make_error(xstudio_error::error, err.what());
            }

            return true;
        },

        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; });
}


void ThumbnailDiskCacheActor::on_exit() {}

void ThumbnailDiskCacheActor::request_read_of_thumbnail(
    caf::typed_response_promise<ThumbnailBufferPtr> rp, const size_t hash) {
    request(pool_, infinite, media_reader::get_thumbnail_atom_v, cache_path_.string(), hash)
        .then(
            [=](const ThumbnailBufferPtr &buf) mutable {
                if (buf) {
                    // bump mtime (filesystem and cache)
                    cache_.cache_[hash].second = fs::file_time_type::clock::now();
                    rp.deliver(buf);
                } else {
                    // deliver empty buffer ?
                    spdlog::warn("{} got invalid buffer", __PRETTY_FUNCTION__);
                    rp.deliver(buf);
                }
            },
            [=](const caf::error &err) mutable { rp.deliver(err); });
}

void ThumbnailDiskCacheActor::request_generation_of_thumbnail(
    caf::typed_response_promise<ThumbnailBufferPtr> rp,
    const media::AVFrameID &mptr,
    const size_t thumb_size,
    const size_t hash,
    const bool cache_to_disk) {

    auto thumbkey = ThumbnailKey(mptr, hash, thumb_size);
    request(
        thumb_gen_middleman_, infinite, media_reader::get_thumbnail_atom_v, mptr, thumb_size)
        .then(
            [=](const ThumbnailBufferPtr &buf) mutable {
                rp.deliver(buf);

                if (cache_to_disk and max_cache_count_ and max_cache_size_) {
                    // add to disk cache, check limits
                    request(
                        pool_,
                        infinite,
                        media_cache::store_atom_v,
                        cache_path_.string(),
                        thumbkey.hash(),
                        buf)
                        .then(
                            [=](const size_t size) {
                                // make room ? update stats..
                                cache_.add_thumbnail(
                                    thumbkey, size, fs::file_time_type::clock::now());
                                // run eviction..
                                evict_thumbnails(
                                    cache_.evict(max_cache_size_, max_cache_count_));
                            },
                            [=](const caf::error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                }
            },
            [=](const caf::error &err) mutable { rp.deliver(err); });
}

void ThumbnailDiskCacheActor::evict_thumbnails(const std::vector<size_t> &hashes) {
    if (not hashes.empty()) {
        request(pool_, infinite, media_cache::erase_atom_v, cache_path_.string(), hashes)
            .then(
                [=](const bool) {},
                [=](const caf::error &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
}
