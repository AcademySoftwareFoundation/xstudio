// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <fmt/format.h>
#include <limits>
#include <list>
#include <tuple>
#include <utility>
#include <vector>

#include "xstudio/enums.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/frame_list.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace media {

    typedef std::vector<std::pair<int, int>> LogicalFrameRanges;

    MediaType media_type_from_string(const std::string &str);
    inline std::string to_readable_string(const MediaType mt) {
        std::string str;
        switch (mt) {
        case MT_IMAGE:
            str = "Image";
            break;
        case MT_AUDIO:
            str = "Audio";
            break;
        }
        return str;
    }

    inline std::string to_string(const MediaStatus ms) {
        std::string str;
        switch (ms) {
        case MS_ONLINE:
            str = "Online";
            break;
        case MS_MISSING:
            str = "Not available on FileSystem";
            break;
        case MS_CORRUPT:
            str = "Corrupt";
            break;
        case MS_UNSUPPORTED:
            str = "Unsupported";
            break;
        case MS_UNREADABLE:
            str = "Unreadable";
            break;
        }
        return str;
    }


    class StreamDetail {
      public:
        StreamDetail(
            utility::FrameRateDuration duration = utility::FrameRateDuration(),
            std::string name                    = "Main",
            const MediaType media_type          = MT_IMAGE,
            std::string key_format              = "{0}@{1}/{2}",
            Imath::V2i resolution               = Imath::V2i(0, 0),
            float pixel_aspect                  = 1.0f,
            int index                           = -1)
            : duration_(std::move(duration)),
              name_(std::move(name)),
              media_type_(media_type),
              key_format_(std::move(key_format)),
              resolution_(resolution),
              pixel_aspect_(pixel_aspect),
              index_(index) {}
        virtual ~StreamDetail() = default;
        bool operator==(const StreamDetail &other) const {
            return (
                duration_ == other.duration_ and name_ == other.name_ and
                media_type_ == other.media_type_ and key_format_ == other.key_format_ and
                resolution_ == other.resolution_ and pixel_aspect_ == other.pixel_aspect_ and
                index_ == other.index_);
        }

        template <class Inspector> friend bool inspect(Inspector &f, StreamDetail &x) {
            return f.object(x).fields(
                f.field("dur", x.duration_),
                f.field("name", x.name_),
                f.field("mt", x.media_type_),
                f.field("kf", x.key_format_),
                f.field("res", x.resolution_),
                f.field("pa", x.pixel_aspect_),
                f.field("idx", x.index_));
        }
        friend std::string to_string(const StreamDetail &value);

        utility::FrameRateDuration duration_;
        std::string name_;
        MediaType media_type_;
        std::string key_format_;
        Imath::V2i resolution_;
        float pixel_aspect_;
        int index_;
    };

    inline std::string to_string(const StreamDetail &v) {
        return v.name_ + " " + to_readable_string(v.media_type_) + " " + v.key_format_ + " " +
               to_string(v.duration_);
    }

    class MediaDetail {
      public:
        MediaDetail(
            std::string reader                = "",
            std::vector<StreamDetail> streams = std::vector<StreamDetail>(),
            const utility::Timecode &timecode = utility::Timecode("00:00:00:00"))
            : reader_(std::move(reader)), streams_(std::move(streams)), timecode_(timecode) {}
        virtual ~MediaDetail() = default;


        bool operator==(const MediaDetail &other) const {
            return (
                reader_ == other.reader_ and timecode_ == other.timecode_ and
                streams_ == other.streams_);
        }

        template <class Inspector> friend bool inspect(Inspector &f, MediaDetail &x) {
            return f.object(x).fields(
                f.field("reader", x.reader_),
                f.field("strms", x.streams_),
                f.field("tc", x.timecode_));
        }

        std::string reader_;
        std::vector<StreamDetail> streams_;
        utility::Timecode timecode_;
    };

    class MediaKey : private std::string {

      public:
        MediaKey()                  = default;
        MediaKey(const MediaKey &o) = default;
        MediaKey(const std::string &o) : std::string(o) {}
        MediaKey(
            const std::string &key_format,
            const caf::uri &uri,
            const int frame,
            const std::string &stream_id)
            : std::string(fmt::format(
                  key_format,
                  to_string(uri),
                  (frame == std::numeric_limits<int>::min() ? 0 : frame),
                  stream_id)) {}

        bool operator==(const MediaKey &o) const {
            return static_cast<const std::string &>(o) ==
                   static_cast<const std::string &>(*this);
        }

        bool operator!=(const MediaKey &o) const {
            return static_cast<const std::string &>(o) !=
                   static_cast<const std::string &>(*this);
        }

        bool operator<(const MediaKey &o) const {
            return static_cast<const std::string &>(o) <
                   static_cast<const std::string &>(*this);
        }

        friend std::string to_string(const MediaKey &value);
        friend fmt::string_view to_string_view(const MediaKey &value);

        template <class Inspector> friend bool inspect(Inspector &f, MediaKey &x) {
            return f.object(x).fields(f.field("data", static_cast<std::string &>(x)));
        }
    };

    inline std::string to_string(const MediaKey &v) {
        return static_cast<const std::string>(v);
    }

    inline fmt::string_view to_string_view(const MediaKey &s) { return {s.data(), s.length()}; }

    typedef std::vector<MediaKey> MediaKeyVector;

    inline MediaKey media_key(
        const std::string &key_format,
        const caf::uri &uri,
        const int frame,
        const std::string &stream_id) {
        return MediaKey(fmt::format(
            key_format,
            to_string(uri),
            (frame == std::numeric_limits<int>::min() ? 0 : frame),
            stream_id));
    }


    class AVFrameID {
      public:
        AVFrameID(
            const caf::uri &uri               = caf::uri(),
            const int frame                   = std::numeric_limits<int>::min(),
            const int first_frame             = std::numeric_limits<int>::min(),
            const utility::FrameRate rate     = utility::FrameRate(timebase::k_flicks_24fps),
            const std::string &stream_id      = "",
            const std::string &key_format     = "{0}@{1}/{2}",
            std::string reader                = "",
            const caf::actor_addr addr        = caf::actor_addr(),
            const utility::JsonStore &params  = utility::JsonStore(),
            const utility::Uuid &source_uuid  = utility::Uuid(),
            const utility::Uuid &media_uuid   = utility::Uuid(),
            const MediaType media_type        = MT_IMAGE,
            const int playhead_logical_frame  = 0,
            const utility::Timecode time_code = utility::Timecode())
            : uri_(uri),
              frame_(frame),
              first_frame_(first_frame),
              rate_(std::move(rate)),
              stream_id_(stream_id),
              key_(key_format, uri, frame, stream_id),
              reader_(std::move(reader)),
              actor_addr_(addr),
              params_(params),
              source_uuid_(source_uuid),
              media_uuid_(media_uuid),
              media_type_(media_type),
              playhead_logical_frame_(playhead_logical_frame),
              timecode_(time_code) {}

        virtual ~AVFrameID() = default;

        AVFrameID(const AVFrameID &o) = default;

        [[nodiscard]] bool is_nil() const { return uri_.empty(); }

        bool operator==(const AVFrameID &other) const {
            return (
                uri_ == other.uri_ and frame_ == other.frame_ and
                first_frame_ == other.first_frame_ and rate_ == other.rate_ and
                stream_id_ == other.stream_id_ and key_ == other.key_ and
                reader_ == other.reader_ and actor_addr_ == other.actor_addr_ and
                params_ == other.params_ and source_uuid_ == other.source_uuid_ and
                media_uuid_ == other.media_uuid_ and media_type_ == other.media_type_ and
                playhead_logical_frame_ == other.playhead_logical_frame_ and
                timecode_ == other.timecode_ and error_ == other.error_);
        }
        template <class Inspector> friend bool inspect(Inspector &f, AVFrameID &x) {
            return f.object(x).fields(
                f.field("uri", x.uri_),
                f.field("frm", x.frame_),
                f.field("ffrm", x.first_frame_),
                f.field("rat", x.rate_),
                f.field("strid", x.stream_id_),
                f.field("key", x.key_),
                f.field("rdr", x.reader_),
                f.field("actad", x.actor_addr_),
                f.field("prms", x.params_),
                f.field("suuid", x.source_uuid_),
                f.field("skpky", x.media_uuid_),
                f.field("mt", x.media_type_),
                f.field("plc", x.playhead_logical_frame_),
                f.field("tc", x.timecode_),
                f.field("err", x.error_));
        }

        caf::uri uri_;
        int frame_;
        int first_frame_;
        utility::FrameRate rate_;
        std::string stream_id_;
        MediaKey key_;
        std::string reader_;
        caf::actor_addr actor_addr_;
        utility::JsonStore params_;
        utility::Uuid source_uuid_;
        utility::Uuid media_uuid_;
        MediaType media_type_;
        int playhead_logical_frame_;
        utility::Timecode timecode_;
        std::string error_;
    };

    typedef std::pair<utility::time_point, std::shared_ptr<const AVFrameID>>
        MediaPointerAndTimePoint;
    typedef std::vector<MediaPointerAndTimePoint> AVFrameIDsAndTimePoints;

    class Media : public utility::Container {
      public:
        Media(const utility::JsonStore &jsn);
        Media(
            const std::string &name                   = "",
            const utility::Uuid &current_image_source = utility::Uuid(),
            const utility::Uuid &current_audio_source = utility::Uuid());
        ~Media() override = default;

        void add_media_source(
            const utility::Uuid &uuid, const utility::Uuid &before_uuid = utility::Uuid());
        void remove_media_source(const utility::Uuid &uuid);

        /*Sets the media source for the specified media type (video or audio) - if
        video is specified and there is no current audio, audio source is also set*/
        bool set_current(const utility::Uuid &uuid, const MediaType mt = MediaType::MT_IMAGE);
        [[nodiscard]] bool empty() const { return media_sources_.empty(); }
        [[nodiscard]] utility::Uuid current(const MediaType mt = MediaType::MT_IMAGE) const {
            return mt == MediaType::MT_IMAGE ? current_image_source_ : current_audio_source_;
        }
        [[nodiscard]] const std::list<utility::Uuid> &media_sources() const {
            return media_sources_;
        }
        [[nodiscard]] utility::JsonStore serialise() const override;
        void deserialise(const utility::JsonStore &jsn) override;
        [[nodiscard]] std::string flag() const { return flag_; }
        void set_flag(const std::string &flag) { flag_ = flag; }
        [[nodiscard]] std::string flag_text() const { return flag_text_; }
        void set_flag_text(const std::string &flag_text) { flag_text_ = flag_text; }


      private:
        // will need extending.., tagging ?
        utility::Uuid current_image_source_;
        utility::Uuid current_audio_source_;
        std::string flag_{"#00000000"};
        std::string flag_text_{""};
        std::list<utility::Uuid> media_sources_;
    };

    class MediaSource : public utility::Container {
      public:
        MediaSource(const utility::JsonStore &jsn);
        MediaSource(
            const std::string &name,
            const caf::uri &_uri,
            const utility::FrameList &frame_list);
        MediaSource(const std::string &name, const caf::uri &_uri);
        MediaSource(const std::string &name, const utility::MediaReference &media_reference);
        ~MediaSource() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;

        // Note - although this function takes a stream uuid, we only have one
        // media reference shared for all streams. Thus we have to assume that
        // the frame rate and duration of each stream in a source are the same.
        // This assumption may not be correct, so leaving the argument here for
        // future if needed.
        const utility::MediaReference &
        media_reference(const utility::Uuid &stream_uuid = utility::Uuid()) const {
            return ref_;
        }

        void set_media_reference(const utility::MediaReference &ref);

        [[nodiscard]] std::string reader() const { return reader_; }
        void set_reader(const std::string &reader) { reader_ = reader; }

        [[nodiscard]] utility::Uuid current(const MediaType media_type) const;
        bool set_current(const MediaType media_type, const utility::Uuid &uuid);

        void add_media_stream(
            const MediaType media_type,
            const utility::Uuid &uuid,
            const utility::Uuid &before_uuid = utility::Uuid());
        void remove_media_stream(const MediaType media_type, const utility::Uuid &uuid);

        [[nodiscard]] bool empty() const {
            return audio_streams_.empty() and image_streams_.empty();
        }
        void clear();

        void set_media_status(const MediaStatus status) { media_status_ = status; }

        void set_error_detail(const std::string error_detail) { error_detail_ = error_detail; }

        [[nodiscard]] bool has_type(const MediaType media_type) const;
        [[nodiscard]] MediaStatus media_status() const { return media_status_; }
        [[nodiscard]] bool online() const { return media_status_ == MediaStatus::MS_ONLINE; }
        [[nodiscard]] const std::string &error_detail() const { return error_detail_; }


        [[nodiscard]] const std::list<utility::Uuid> &streams(const MediaType media_type) const;

        [[nodiscard]] std::pair<std::string, uintmax_t> checksum() const {
            return std::make_pair(checksum_, size_);
        }
        [[nodiscard]] bool checksum(const std::pair<std::string, uintmax_t> &checksum) {
            auto changed = false;
            if (checksum_ != checksum.first or size_ != checksum.second) {
                checksum_ = checksum.first;
                size_     = checksum.second;
                changed   = true;
            }
            return changed;
        }

      private:
        utility::MediaReference ref_;
        utility::Uuid current_audio_;
        utility::Uuid current_image_;
        std::list<utility::Uuid> image_streams_;
        std::list<utility::Uuid> audio_streams_;
        std::string reader_;
        std::string error_detail_;
        MediaStatus media_status_{MS_ONLINE};
        uintmax_t size_{0};
        std::string checksum_;
    };

    class MediaStream : public utility::Container {
      public:
        MediaStream(const utility::JsonStore &jsn);
        MediaStream(const StreamDetail &detail);

        ~MediaStream() override = default;
        [[nodiscard]] utility::JsonStore serialise() const override;

        [[nodiscard]] std::string key_format() const { return detail_.key_format_; }
        void set_key_format(const std::string &key_format) { detail_.key_format_ = key_format; }
        void set_detail(const StreamDetail &detail) {
            detail_       = detail;
            detail_.name_ = name();
        }

        [[nodiscard]] MediaType media_type() const { return detail_.media_type_; }
        [[nodiscard]] utility::FrameRateDuration duration() const { return detail_.duration_; }
        const StreamDetail &detail() const { return detail_; }

      private:
        StreamDetail detail_;
    };

    inline std::shared_ptr<const AVFrameID> make_blank_frame(const MediaType media_type) {
        utility::JsonStore js;
        js["BLANK_FRAME"] = true;

        return std::shared_ptr<const AVFrameID>(new AVFrameID(
            *caf::make_uri("xstudio://blank/?colour=gray"),
            0,
            0,
            timebase::k_flicks_24fps,
            "",
            "{0}@{1}/{2}",
            "Blank",
            actor_addr(),
            js,
            utility::Uuid(),
            utility::Uuid(),
            media_type));
    }
} // namespace media
} // namespace xstudio

namespace std {
template <> struct hash<xstudio::media::MediaKey> {
    size_t operator()(const xstudio::media::MediaKey &k) const {
        return hash<std::string>()(to_string(k));
    }
};
} // namespace std