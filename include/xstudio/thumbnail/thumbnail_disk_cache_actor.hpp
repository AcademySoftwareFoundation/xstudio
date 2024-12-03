// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <filesystem>

#include <memory>
#include <set>
#include <string>

namespace xstudio {
namespace thumbnail {

    namespace fs = std::filesystem;


    class TDCHelperActor : public caf::event_based_actor {
      public:
        TDCHelperActor(caf::actor_config &cfg);

        ~TDCHelperActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }

        const char *name() const override { return NAME.c_str(); }

      private:
        fs::path thumbnail_path(const std::string &path, const size_t thumbkey);
        ThumbnailBufferPtr read_decode_thumb(const std::string &path);
        size_t encode_save_thumb(const std::string &path, const ThumbnailBufferPtr &buffer);

        // std::vector<std::byte> read_thumb(const std::string &path);

        std::vector<std::byte>
        encode_thumb(const ThumbnailBufferPtr &buffer, const int quality = 75);
        ThumbnailBufferPtr decode_thumb(const std::vector<std::byte> &buffer);

        inline static const std::string NAME = "TDCHelperActor";
        caf::behavior behavior_;
    };

    class ThumbnailDiskCacheActor : public caf::event_based_actor {
      public:
        ThumbnailDiskCacheActor(caf::actor_config &cfg);

        ~ThumbnailDiskCacheActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }

        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

      private:
        void request_read_of_thumbnail(
            caf::typed_response_promise<ThumbnailBufferPtr> rp, const size_t hash);
        void request_generation_of_thumbnail(
            caf::typed_response_promise<ThumbnailBufferPtr> rp,
            const media::AVFrameID &mptr,
            const size_t thumb_size,
            const size_t hash,
            const bool cache_to_disk);
        void evict_thumbnails(const std::vector<size_t> &hashes);


        inline static const std::string NAME = "ThumbnailDiskCacheActor";
        caf::behavior behavior_;
        caf::uri cache_path_pref_;
        fs::path cache_path_;
        size_t max_cache_size_{std::numeric_limits<size_t>::max()};
        size_t max_cache_count_{std::numeric_limits<size_t>::max()};

        struct DiskCacheStat cache_;
        caf::actor pool_;
        caf::actor thumb_gen_middleman_;
    };

} // namespace thumbnail
} // namespace xstudio
