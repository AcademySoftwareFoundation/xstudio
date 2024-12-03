// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <chrono>
#include <unordered_map>

#include <flicks.hpp>

#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/timecode.hpp"

namespace xstudio {
namespace bookmark {

    class AnnotationBase {
      public:
        AnnotationBase()                        = default;
        AnnotationBase(const AnnotationBase &o) = default;
        AnnotationBase(const utility::JsonStore &s, const utility::Uuid &uuid)
            : store_(s), bookmark_uuid_(uuid) {}
        virtual ~AnnotationBase() = default;

        /* AnnotationBase sub-class must fill in the plugin_uuid with its respective
        plugin uuid */
        virtual utility::JsonStore serialise(utility::Uuid &plugin_uuid) const {
            return store_;
        }
        utility::Uuid bookmark_uuid_;

      private:
        utility::JsonStore store_;
    };

    typedef std::shared_ptr<AnnotationBase> AnnotationBasePtr;

    class Note {
      public:
        Note() = default;
        Note(const utility::JsonStore &);
        virtual ~Note() = default;
        [[nodiscard]] utility::JsonStore serialise() const;

        auto subject() const { return subject_; }
        auto note() const { return note_; }
        auto author() const { return author_; }
        auto created() const { return created_; }
        auto category() const { return category_; }
        auto colour() const { return colour_; }

        void set_subject(const std::string &subject) {
            subject_ = subject;
            set_created();
        }
        void set_note(const std::string &note) {
            note_ = note;
            set_created();
        }
        void set_author(const std::string &author = "Anonymous") { author_ = author; }
        void set_created(const utility::sys_time_point &created = utility::sysclock::now()) {
            created_ = created;
        }
        void set_category(const std::string &category = "") { category_ = category; }
        void set_colour(const std::string &colour = "") { colour_ = colour; }

      private:
        std::string subject_;
        std::string note_;
        std::string author_{"Anonymous"};
        utility::sys_time_point created_{utility::sysclock::now()};
        std::string category_;
        std::string colour_;
        // thumb
        // audio
    };

    class Bookmark;

    struct BookmarkDetail {
        BookmarkDetail() = default;
        BookmarkDetail(const Bookmark &bm) { *this = bm; }

        utility::Uuid uuid_;
        caf::actor_addr actor_addr_;

        std::optional<utility::UuidActor> owner_;
        std::optional<bool> enabled_;
        std::optional<bool> has_focus_;
        std::optional<bool> visible_;
        std::optional<timebase::flicks> start_;
        std::optional<timebase::flicks> duration_;
        std::optional<std::string> user_type_;
        std::optional<utility::JsonStore> user_data_;

        std::optional<std::string> author_;
        std::optional<std::string> subject_;
        std::optional<std::string> note_;
        std::optional<utility::sys_time_point> created_;
        std::optional<std::string> category_;
        std::optional<std::string> colour_;

        std::optional<bool> has_note_;
        std::optional<bool> has_annotation_;

        std::optional<utility::MediaReference> media_reference_;
        std::optional<std::string> media_flag_;

        std::optional<int> logical_start_frame_;

        BookmarkDetail &operator=(const Bookmark &bookmark);
        template <class Inspector> friend bool inspect(Inspector &f, BookmarkDetail &x) {
            return f.object(x).fields(
                f.field("uui", x.uuid_),
                f.field("act", x.actor_addr_),
                f.field("own", x.owner_),
                f.field("ena", x.enabled_),
                f.field("foc", x.has_focus_),
                f.field("vis", x.visible_),
                f.field("sta", x.start_),
                f.field("dur", x.duration_),
                f.field("utp", x.user_type_),
                f.field("udt", x.user_data_),
                f.field("hasa", x.has_annotation_),
                f.field("hasn", x.has_note_),
                f.field("aut", x.author_),
                f.field("cat", x.category_),
                f.field("col", x.colour_),
                f.field("sub", x.subject_),
                f.field("not", x.note_),
                f.field("mr", x.media_reference_),
                f.field("mf", x.media_flag_),
                f.field("lsf", x.logical_start_frame_),
                f.field("cre", x.created_));
        }
        friend std::string to_string(const BookmarkDetail &value);

        timebase::flicks start() const {
            if (not start_ or *start_ == timebase::k_flicks_low)
                return timebase::k_flicks_zero_seconds;
            return *start_;
        }

        std::string start_timecode() const {
            if (media_reference_)
                return to_string(start_timecode_tc());

            return "--:--:--:--";
        }

        utility::Timecode start_timecode_tc() const {
            if (media_reference_) {
                auto fc = (*media_reference_)
                              .duration()
                              .frame(max(*start_, timebase::k_flicks_zero_seconds));
                // turn into timecode..
                return (*media_reference_).timecode() +
                       utility::Timecode(fc, (*media_reference_).rate().to_fps());
            }
            return utility::Timecode();
        }

        std::string end_timecode() const {
            if (media_reference_) {
                auto fs = (*media_reference_)
                              .duration()
                              .frame(max(*start_, timebase::k_flicks_zero_seconds));


                auto fd = (*media_reference_).duration().frame(*duration_);
                if (*duration_ == timebase::k_flicks_max) {
                    fd = (*media_reference_)
                             .duration()
                             .frame(
                                 (*media_reference_).duration().duration() -
                                 max(*start_, timebase::k_flicks_zero_seconds)) -
                         1;
                }

                // turn into timecode..
                return to_string(
                    (*media_reference_).timecode() +
                    utility::Timecode(fs + fd, (*media_reference_).rate().to_fps()));
            }

            return "--:--:--:--";
        }

        std::string duration_timecode() const {
            if (media_reference_) {
                auto fd =
                    (*media_reference_)
                        .duration()
                        .frame(min(*duration_, (*media_reference_).duration().duration()));
                // unbounded. limit to clip duration - start.
                // also check start isn't unbounded..
                if (*duration_ == timebase::k_flicks_max) {
                    fd -= (*media_reference_)
                              .duration()
                              .frame(max(*start_, timebase::k_flicks_zero_seconds)) +
                          1;
                }

                // turn into timecode..
                return to_string(
                    utility::Timecode(fd + 1, (*media_reference_).rate().to_fps()));
            }
            return "--:--:--:--";
        }

        double get_second(const int frame) const {
            if (media_reference_) {
                return timebase::to_seconds((*(media_reference_)).rate().to_flicks() * frame);
            }
            return 0.0;
        }

        int get_frame(const double sec) const {
            if (media_reference_) {
                return (*(media_reference_)).duration().frame(timebase::to_flicks(sec));
            }
            return 0;
        }

        int get_frame(const timebase::flicks sec) const {
            if (media_reference_) {
                return (*(media_reference_)).duration().frame(sec);
            }
            return 0;
        }

        timebase::flicks duration() const {
            if (not duration_ or *duration_ == timebase::k_flicks_max) {
                if (media_reference_) {
                    return timebase::to_flicks((*(media_reference_)).seconds()) - start();
                } else
                    return timebase::k_flicks_zero_seconds;
            }
            return *duration_;
        }


        std::string created() const {
#ifdef _WIN32
            auto dt = (created_ ? *created_ : std::chrono::high_resolution_clock::now());
#else
            auto dt = (created_ ? *created_ : std::chrono::system_clock::now());
#endif
            return utility::to_string(dt);
        }

        std::string colour() const {
            if (colour_)
                return *colour_;
            return "";
        }

        int start_frame() const {
            if (media_reference_) {
                return (*(media_reference_)).duration().frame(start());
            }
            return 0;
        }

        int end_frame() const {
            if (media_reference_) {
                return start_frame() + (*(media_reference_)).duration().frame(duration());
            }
            return 0;
        }
    };

    inline std::string to_string(const BookmarkDetail &) { return "Bookmark"; }

    class Bookmark : public utility::Container {
      public:
        Bookmark(const utility::Uuid &uuid = utility::Uuid::generate());
        Bookmark(const utility::JsonStore &jsn);
        Bookmark(const Bookmark &src);
        ~Bookmark() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;

        auto note() const { return note_; }
        auto annotation() const { return annotation_; }

        auto has_note() const { return static_cast<bool>(note_); }
        auto has_annotation() const { return static_cast<bool>(annotation_); }

        void create_note();
        void create_annotation();

        auto enabled() const { return enabled_; }
        auto has_focus() const { return has_focus_; }
        auto visible() const { return visible_; }
        auto start() const { return start_; }
        auto duration() const { return duration_; }
        auto user_type() const { return user_type_; }
        auto user_data() const { return user_data_; }
        auto created() const { return created_; };

        auto owner() const { return owner_; }

        void set_owner(const utility::Uuid owner) { owner_ = owner; }
        void set_enabled(const bool enabled = true) { enabled_ = enabled; }
        void set_visible(const bool visible = true) { visible_ = visible; }
        void set_has_focus(const bool has_focus = true) { has_focus_ = has_focus; }
        void set_start(const timebase::flicks start = timebase::k_flicks_low) {
            start_ = start;
        }
        void set_duration(const timebase::flicks duration = timebase::k_flicks_max) {
            duration_ = duration;
        }
        void set_user_type(const std::string &user_type) { user_type_ = user_type; }
        void set_user_data(const utility::JsonStore &user_data) { user_data_ = user_data; }
        void set_created(const utility::sys_time_point &created) { created_ = created; }

        friend BookmarkDetail &BookmarkDetail::operator=(const Bookmark &bookmark);

        bool update(const BookmarkDetail &detail);

      private:
        friend class BookmarkActor;
        utility::Uuid owner_;
        bool enabled_{true};
        bool has_focus_{false};
        bool visible_{true};
        timebase::flicks start_{timebase::k_flicks_low};
        timebase::flicks duration_{timebase::k_flicks_max};
        std::string user_type_;
        utility::JsonStore user_data_;
        utility::sys_time_point created_{utility::sysclock::now()};

        std::shared_ptr<Note> note_{nullptr};
        std::shared_ptr<AnnotationBase> annotation_{nullptr};
    };

    /* This struct is used by Playhead classes as a convenient way to maintain
    a record of bookmarks, attached annotation data and a logical frame range*/
    struct BookmarkAndAnnotation {

        BookmarkDetail detail_;
        std::shared_ptr<AnnotationBase> annotation_;
        int start_frame_ = -1;
        int end_frame_   = -1;
    };

    // BookmarkAndAnnotationPtrs can be shared across different parts of the
    // application (for example the SubPlayhead, ImageBufPtr, AnnotationsTool)
    // and therefore it must be const data
    typedef std::shared_ptr<const BookmarkAndAnnotation> BookmarkAndAnnotationPtr;
    typedef std::vector<BookmarkAndAnnotationPtr> BookmarkAndAnnotations;

    // not sure if we want this...
    class Bookmarks : public utility::Container {
      public:
        Bookmarks(const std::string &name = "Bookmarks");
        Bookmarks(const utility::JsonStore &);
        ~Bookmarks() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;

        //    bool has_association(const utility::Uuid &src) const;
        //    void associate(const utility::Uuid &bookmark, const utility::Uuid &src);
        //    void disassociate(const utility::Uuid &bookmark, const utility::Uuid &src);
        //    void disassociate_from_src(const utility::Uuid &src);
        //    void disassociate_from_bookmark(const utility::Uuid &bookmark);
        //    bool can_retire_bookmark(const utility::Uuid &bookmark) const;
        //    bool can_retire_src(const utility::Uuid &src) const;
        // utility::UuidVector can_retire_srcs() const;
        // utility::UuidVector can_retire_bookmarks() const;

        // utility::UuidVector get_bookmark_associations(const utility::Uuid &bookmark)
        // const;

        //    [[nodiscard]] bool empty() const {return bookmark_associations_.empty();}
        //    [[nodiscard]] size_t size() const {return bookmark_associations_.size();}

        //  private:
        //  	std::map<utility::Uuid, std::set<utility::Uuid>> bookmark_associations_;
        //  	std::map<utility::Uuid, std::set<utility::Uuid>> src_associations_;
    };

} // namespace bookmark
} // namespace xstudio
