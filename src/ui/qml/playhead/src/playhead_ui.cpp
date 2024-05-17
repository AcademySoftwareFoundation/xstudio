// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/playhead/playhead.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/playhead/playhead_selection_actor.hpp"
#include "xstudio/ui/qml/playhead_ui.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/timecode.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

PlayheadUI::PlayheadUI(QObject *parent)
    : QMLActor(parent),
      backend_(),
      backend_events_(),

      timecode_start_("00:00:00:00"),
      timecode_("00:00:00:00"),
      timecode_frames_(1),
      timecode_end_("00:00:00:00"),
      timecode_duration_("00:00:00:00") {}

// helper ?

void PlayheadUI::set_backend(caf::actor backend) {

    if (backend_ == backend)
        return;

    scoped_actor sys{system()};

    bool had_backend = bool(backend_);

    if (backend_) {
        self()->demonitor(backend_);
        anon_send(backend_, playhead::play_atom_v, false); // make sure we stop playback
        anon_send(backend_, module::disconnect_from_ui_atom_v);
    }

    backend_ = backend;
    // get backend state..
    if (backend_events_) {

        try {
            request_receive<bool>(
                *sys, backend_events_, broadcast::leave_broadcast_atom_v, as_actor());
        } catch (const std::exception &) {
        }
        self()->demonitor(backend_events_);
        backend_events_ = caf::actor();
    }


    if (!backend_) {
        looping_              = playhead::LoopMode::LM_LOOP;
        play_rate_mode_       = TimeSourceMode::FIXED;
        playing_              = false;
        forward_              = true;
        fr_rate_              = FrameRate(timebase::k_flicks_24fps);
        fr_playhead_rate_     = FrameRate(timebase::k_flicks_24fps);
        velocity_multiplier_  = 1.0f;
        loop_start_           = 1;
        loop_end_             = 1;
        frames_               = 0;
        use_loop_range_       = false;
        key_playhead_index_   = 0;
        source_offset_frames_ = 0;
    } else {
        self()->monitor(backend_);
        try {

            auto detail = request_receive<ContainerDetail>(*sys, backend_, detail_atom_v);
            name_       = QString::fromStdString(detail.name_);
            uuid_       = detail.uuid_;

            backend_events_ = detail.group_;
            request_receive<bool>(
                *sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());
            self()->monitor(backend_events_);

            fr_rate_ = request_receive<FrameRate>(*sys, backend_, utility::rate_atom_v);
            rate_    = fr_rate_.to_seconds();
            fr_playhead_rate_ =
                request_receive<FrameRate>(*sys, backend_, playhead::playhead_rate_atom_v);
            playhead_rate_ = fr_playhead_rate_.to_seconds();
            velocity_multiplier_ =
                request_receive<float>(*sys, backend_, playhead::velocity_multiplier_atom_v);
            loop_start_ =
                request_receive<int>(*sys, backend_, playhead::simple_loop_start_atom_v);
            loop_end_ = request_receive<int>(*sys, backend_, playhead::simple_loop_end_atom_v);
            frames_   = request_receive<size_t>(*sys, backend_, playhead::duration_frames_atom_v);
            use_loop_range_ =
                request_receive<bool>(*sys, backend_, playhead::use_loop_range_atom_v);
            key_playhead_index_ =
                request_receive<int>(*sys, backend_, playhead::key_playhead_index_atom_v);
            source_offset_frames_ =
                request_receive<int>(*sys, backend_, media::source_offset_frames_atom_v);
            looping_ =
                (int)request_receive<playhead::LoopMode>(*sys, backend_, playhead::loop_atom_v);
            play_rate_mode_ = request_receive<TimeSourceMode>(
                *sys, backend_, playhead::play_rate_mode_atom_v);
            playing_ = request_receive<bool>(*sys, backend_, playhead::play_atom_v);
            forward_ = request_receive<bool>(*sys, backend_, playhead::play_forward_atom_v);

        } catch (const std::exception &e) {
            source_offset_frames_ = 0;
            spdlog::warn("B {} {}", __PRETTY_FUNCTION__, e.what());
        }
    }

    loop_mode_options_ = QList<QVariant>({
        QMap<QString, QVariant>(
            {{QString("text"), QString("Play Once")},
             {QString("value"), (int)playhead::LoopMode::LM_PLAY_ONCE}}),
        QMap<QString, QVariant>(
            {{QString("text"), QString("Loop")},
             {QString("value"), (int)playhead::LoopMode::LM_LOOP}}),
        QMap<QString, QVariant>(
            {{QString("text"), QString("Loop")},
             {QString("value"), (int)playhead::LoopMode::LM_PING_PONG}}),
    });

    emit framesChanged();
    emit secondsChanged();
    emit frameChanged();
    emit secondChanged();
    emit uuidChanged();
    emit backendChanged();
    emit velocityMultiplierChanged();
    emit playingChanged();
    emit loopModeChanged();
    emit nativeChanged();
    emit playRateModeChanged();
    emit rateChanged();
    emit playheadRateChanged();
    emit forwardChanged();

    emit timecodeStartChanged();
    emit timecodeChanged();
    emit timecodeEndChanged();

    emit keyPlayheadIndexChanged();
    emit sourceOffsetFramesChanged();

    media_changed();
    rebuild_cache();
    rebuild_bookmarks();
    emit cachedFramesChanged();
    emit bookmarkedFramesChanged();

    spdlog::debug("PlayheadUI set_backend {}", to_string(uuid_));

    if (backend_) {
        anon_send(backend_, module::connect_to_ui_atom_v);
    }

    if (bool(backend_) != had_backend) {
        emit isNullChanged();
    }
}

void PlayheadUI::initSystem(QObject *system_qobject) {
    init(dynamic_cast<QMLActor *>(system_qobject)->system());
}

void PlayheadUI::init(actor_system &system_) {
    QMLActor::init(system_);

    spdlog::debug("PlayheadUI init");

    // self()->set_down_handler([=](down_msg& msg) {
    // 	if(msg.source == store)
    // 		unsubscribe();
    // });

    // media_uuid_ = QUuid();
    // emit mediaUuidChanged(media_uuid_);

    self()->set_down_handler([=](down_msg &msg) {
        if (msg.source == backend_) {
            backend_ = caf::actor();
            set_backend(caf::actor());
        } else if (msg.source == backend_events_) {
            backend_events_ = caf::actor();
        }
    });

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {

            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg & /*msg*/) {
                // 		if(msg.source == store_events)
                // unsubscribe();
            },

            [=](utility::event_atom, media::source_offset_frames_atom, const int index) {
                source_offset_frames_ = index;
                emit(sourceOffsetFramesChanged());
            },

            [=](utility::event_atom,
                media_cache::cached_frames_atom,
                const std::vector<std::pair<int, int>> &cached_regions) {
                cache_detail_.clear();
                for (const auto &r : cached_regions) {
                    cache_detail_.push_back(QPoint(r.first, r.second));
                }
                emit cachedFramesChanged();
            },

            [=](utility::event_atom,
                bookmark::get_bookmarks_atom,
                const std::vector<std::tuple<utility::Uuid, std::string, int, int>>
                    &bookmarked_regions) {
                bookmark_detail_ = bookmarked_regions;

                bookmark_detail_ui_.clear();
                for (const auto &r : bookmark_detail_) {
                    bookmark_detail_ui_.push_back(new TimesliderMarker(
                        std::get<2>(r), std::get<3>(r), QStringFromStd(std::get<1>(r)), this));
                }
                emit bookmarkedFramesChanged();
            },

            [=](utility::event_atom, playhead::duration_frames_atom, const size_t frames) {
                // something changed in the playhead...
                // use this for media changes, which impact timeline
                if (frames_ != frames) {
                    frames_ = frames;
                    emit framesChanged();
                    // reaquire cache..
                    rebuild_cache();
                    rebuild_bookmarks();
                    // emit cachedFramesChanged();
                    // emit cachedFramesChanged();
                }
            },

            [=](utility::event_atom, playhead::key_playhead_index_atom, const int index) {
                key_playhead_index_ = index;
                emit(keyPlayheadIndexChanged());
                emit(compareLayerNameChanged());
                rebuild_cache();
                rebuild_bookmarks();
                // emit cachedFramesChanged();
            },

            [=](utility::event_atom, playhead::loop_atom, const playhead::LoopMode looping) {
                if (looping_ != (int)looping) {
                    looping_ = (playhead::LoopMode)looping;
                    emit loopModeChanged();
                }
            },

            [=](utility::event_atom,
                playhead::media_source_atom,
                const UuidActor &media_actor) {
                auto uuid = QUuidFromUuid(media_actor.uuid());
                if (media_uuid_ != uuid) {
                    media_uuid_ = uuid;
                    emit mediaUuidChanged(media_uuid_);
                }
            },

            [=](utility::event_atom, playhead::play_atom, const bool playing) {
                if (playing_ != playing) {
                    playing_ = playing;
                    emit playingChanged();
                }
            },

            [=](utility::event_atom, playhead::play_forward_atom, const bool forward) {
                if (forward_ != forward) {
                    forward_ = forward;
                    emit forwardChanged();
                }
            },

            [=](utility::event_atom,
                playhead::play_rate_mode_atom,
                const utility::TimeSourceMode tsm) {
                if (play_rate_mode_ != tsm) {
                    play_rate_mode_ = tsm;
                    emit nativeChanged();
                    emit playRateModeChanged();
                }
            },

            [=](utility::event_atom, playhead::playhead_rate_atom, const FrameRate &rate) {
                if (fr_playhead_rate_.count() != rate.count()) {
                    fr_playhead_rate_ = rate;
                    playhead_rate_    = fr_playhead_rate_.to_seconds();
                    emit playheadRateChanged();
                }
            },

            // pretty sure these are redundant..
            // [=](utility::event_atom, playhead::position_atom, const double second) {
            //     // something changed in the playhead...
            //     // use this for media changes, which impact timeline
            //     if (second_ != second) {
            //         second_ = second;
            //         emit secondChanged();
            //     }
            // },

            // [=](utility::event_atom, playhead::position_atom, const int frame) {
            //     // something changed in the playhead...
            //     // use this for media changes, which impact timeline
            //     if (frame_ != frame) {
            //         frame_ = frame;
            //         emit frameChanged();
            //     }
            // },

            [=](utility::event_atom,
                playhead::position_atom,
                const int frame,
                const int media_frame,
                const int media_logical_frame,
                const utility::FrameRate &media_rate,
                const timebase::flicks position,
                const utility::Timecode &timecode) {
                // spdlog::warn("frame {} media_frame {} media_logical_frame {}", frame,
                // media_frame, media_logical_frame);

                // spdlog::debug("position_atom changed {}", frame);
                auto new_ms = timebase::to_seconds(media_frame * media_rate);
                if (new_ms != media_second_) {
                    media_second_ = new_ms;
                    emit mediaSecondChanged();
                }

                if (media_frame != media_frame_) {
                    media_frame_ = media_frame;
                    // spdlog::debug("frame changed, emit {}", frame);
                    emit mediaFrameChanged();
                }
                new_ms = timebase::to_seconds(media_logical_frame * media_rate);
                if (new_ms != media_logical_second_) {
                    media_logical_second_ = new_ms;
                    emit mediaLogicalSecondChanged();
                }

                if (media_logical_frame != media_logical_frame_) {
                    media_logical_frame_ = media_logical_frame;
                    // spdlog::debug("frame changed, emit {}", frame);
                    emit mediaLogicalFrameChanged();
                }
                if (frame != frame_) {
                    frame_ = frame;
                    // spdlog::debug("frame changed, emit {}", frame);
                    emit frameChanged();
                }
                if (timebase::to_seconds(position) != second_) {
                    second_ = timebase::to_seconds(position);
                    // spdlog::debug("second changed {}", second_);
                    emit secondChanged();
                }

                const QString tc_str = QStringFromStd(to_string(timecode));
                if (tc_str != timecode_) {
                    timecode_ = tc_str;

                    timecode_frames_ = timecode.total_frames();

                    emit timecodeFramesChanged();
                    emit timecodeChanged();
                }
            },

            [=](utility::event_atom, playhead::simple_loop_end_atom, const int loop_end) {
                loop_end_ = loop_end;
                emit(loopEndChanged());
            },

            [=](utility::event_atom, playhead::simple_loop_start_atom, const int loop_start) {
                loop_start_ = loop_start;
                emit(loopStartChanged());
            },

            [=](utility::event_atom, playhead::use_loop_range_atom, const bool use_loop_range) {
                use_loop_range_ = use_loop_range;
                emit(useLoopRangeChanged());
            },

            [=](utility::event_atom,
                playhead::velocity_multiplier_atom,
                const float velocity_multiplier) {
                if (velocity_multiplier_ != velocity_multiplier) {
                    velocity_multiplier_ = velocity_multiplier;
                    emit velocityMultiplierChanged();
                }
            },

            [=](utility::event_atom, utility::change_atom) {},

            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {},

            [=](utility::event_atom, utility::name_atom, const std::string &name) {
                name_ = QString::fromStdString(name);
                emit nameChanged();
            },

            [=](utility::event_atom, utility::rate_atom, const FrameRate &rate) {
                if (fr_rate_.count() != rate.count()) {
                    fr_rate_ = rate;
                    rate_    = fr_rate_.to_seconds();
                    emit rateChanged();
                }
            }};
    });
}

void PlayheadUI::media_changed() {

    if (!backend_)
        return;

    try {
        scoped_actor sys{system()};

        auto media_actor = request_receive_wait<caf::actor>(
            *sys, backend_, std::chrono::milliseconds(250), playhead::media_atom_v);

        auto uuid = request_receive_wait<utility::Uuid>(
            *sys, media_actor, std::chrono::milliseconds(250), utility::uuid_atom_v);

        auto tmp = QUuidFromUuid(uuid);
        if (media_uuid_ != tmp) {
            media_uuid_ = tmp;
            emit mediaUuidChanged(media_uuid_);
        }
    } catch ([[maybe_unused]] const std::exception &e) {
        if (media_uuid_ != QUuid()) {
            media_uuid_ = QUuid();
            emit mediaUuidChanged(media_uuid_);
        }
    }
}

void PlayheadUI::setPlaying(const bool playing) {
    anon_send(backend_, playhead::play_atom_v, playing);
}

void PlayheadUI::setFrame(const int frame) {
    if (frame != frame_) {
        anon_send(backend_, playhead::scrub_frame_atom_v, std::max(0, frame));
    }
}

void PlayheadUI::setLoopStart(const int loop_start) {

    anon_send(backend_, playhead::simple_loop_start_atom_v, loop_start);
}

void PlayheadUI::setLoopEnd(const int loop_end) {

    anon_send(backend_, playhead::simple_loop_end_atom_v, loop_end);
}

void PlayheadUI::setUseLoopRange(const bool use_loop_range) {

    anon_send(backend_, playhead::use_loop_range_atom_v, use_loop_range);
}

bool PlayheadUI::jumpToNextSource() {

    try {
        scoped_actor sys{system()};
        return request_receive<bool>(*sys, backend_, playhead::skip_through_sources_atom_v, 1);
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return false;
}

bool PlayheadUI::jumpToPreviousSource() {
    try {
        scoped_actor sys{system()};
        return request_receive<bool>(*sys, backend_, playhead::skip_through_sources_atom_v, -1);
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return false;
}

void PlayheadUI::setLoopMode(const int looping) {
    anon_send(backend_, playhead::loop_atom_v, playhead::LoopMode(looping));
}

void PlayheadUI::setNative(const bool native) {
    anon_send(
        backend_,
        playhead::play_rate_mode_atom_v,
        (native ? TimeSourceMode::REMAPPED : TimeSourceMode::FIXED));
}

void PlayheadUI::setPlayRateMode(const int tsm) {
    anon_send(
        backend_, playhead::play_rate_mode_atom_v, static_cast<utility::TimeSourceMode>(tsm));
}

void PlayheadUI::setForward(const bool forward) {
    anon_send(backend_, playhead::play_forward_atom_v, forward);
}

void PlayheadUI::step(int step_frames) {

    anon_send(backend_, playhead::step_atom_v, step_frames);
}

void PlayheadUI::setPlayheadRate(const double rate) {
    anon_send(backend_, playhead::playhead_rate_atom_v, FrameRate(rate));
}

void PlayheadUI::setVelocityMultiplier(const float velocity_multiplier) {
    anon_send(backend_, playhead::velocity_multiplier_atom_v, velocity_multiplier);
}

void PlayheadUI::skip(const bool forwards) {
    anon_send(backend_, playhead::step_atom_v, forwards ? 1 : -1);
}

void PlayheadUI::setKeyPlayheadIndex(int i) {
    anon_send(backend_, playhead::key_playhead_index_atom_v, i);
}

void PlayheadUI::setSourceOffsetFrames(int i) {
    anon_send(backend_, media::source_offset_frames_atom_v, i);
}


QString PlayheadUI::compareLayerName() {
    char c[2];
    c[0] = 'A';
    c[1] = 0;
    c[0] += (char)key_playhead_index_;
    return QString(c);
}

void PlayheadUI::jumpToSource(const QUuid media_uuid) {
    anon_send(backend_, playhead::jump_atom_v, UuidFromQUuid(media_uuid));
}

void PlayheadUI::resetReadaheadRequests() {
    anon_send(backend_, playhead::jump_atom_v, frame_);
}

void PlayheadUI::setFitMode(const QString mode) {
    if (backend_) {
        anon_send(
            backend_,
            module::change_attribute_request_atom_v,
            std::string("Fit Mode"),
            (int)module::Attribute::Value,
            utility::JsonStore(qvariant_to_json(QVariant(mode))));
    }
}

void PlayheadUI::connectToUI() { anon_send(backend_, module::connect_to_ui_atom_v); }

void PlayheadUI::disconnectFromUI() { anon_send(backend_, module::disconnect_from_ui_atom_v); }

void PlayheadUI::rebuild_cache() { anon_send(backend_, media_cache::cached_frames_atom_v); }

void PlayheadUI::rebuild_bookmarks() { anon_send(backend_, bookmark::get_bookmarks_atom_v); }

int PlayheadUI::nextBookmark(const int search_from_frame) const {
    auto new_frame = search_from_frame;
    auto distance  = std::numeric_limits<int>::max();
    for (const auto &i : bookmark_detail_) {
        const auto &[u, c, s, e] = i;
        if (s > search_from_frame) {
            auto dist = s - search_from_frame;
            if (dist < distance) {
                distance  = dist;
                new_frame = s;
            }
        }
    }
    return std::max(new_frame, 0);
}

int PlayheadUI::previousBookmark(int search_from_frame) const {
    if (search_from_frame == -1)
        search_from_frame = frames_ - 1;

    auto new_frame = search_from_frame;
    auto distance  = std::numeric_limits<int>::max();
    for (const auto &i : bookmark_detail_) {
        const auto &[u, c, s, e] = i;
        if (s < search_from_frame) {
            auto dist = search_from_frame - s;
            if (dist < distance) {
                distance  = dist;
                new_frame = s;
            }
        }
    }
    return new_frame;
}

QVariantList PlayheadUI::getNearestBookmark(const int search_from_frame) const {
    QVariantList pos;
    auto distance = std::numeric_limits<int>::max();

    for (const auto &i : bookmark_detail_) {
        const auto &[u, c, s, e] = i;

        if (s < search_from_frame) {
            auto dist = search_from_frame - s;
            if (dist < distance) {
                distance = dist;
                pos.clear();
                pos.push_back(s);
                pos.push_back(e);
                pos.push_back(QVariant::fromValue(QUuidFromUuid(u)));
            }
        }
    }

    return pos;
}
