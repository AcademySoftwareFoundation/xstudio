// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/qml/web_bridge_ui.hpp"

#include <algorithm>
#include <functional>

CAF_PUSH_WARNINGS
#include <QMetaObject>
#include <QtConcurrent>
CAF_POP_WARNINGS

#include <caf/actor_registry.hpp>
#include <caf/actor_system.hpp>
#include <caf/scoped_actor.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/playhead/enums.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/gap_actor.hpp"
#include "xstudio/timeline/item.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/notification_handler.hpp"

using namespace xstudio;
using namespace xstudio::ui::qml;
using namespace xstudio::utility;

namespace {

// Track flags are #AARRGGBB; pages pass RRGGBB with an optional '#'.
std::string flag_from_colour(const std::string &colour) {
    auto digits = colour;
    if (not digits.empty() and digits[0] == '#')
        digits.erase(0, 1);
    if (digits.size() == 6)
        return "#FF" + digits;
    if (digits.size() == 8)
        return "#" + digits;
    return "";
}

caf::uri uri_from_string(const std::string &url) {
    if (url.find("http://") == 0 or url.find("https://") == 0 or url.find("file://") == 0) {
        auto u = caf::make_uri(url);
        if (not u)
            throw std::runtime_error("Invalid URL: " + url);
        return *u;
    }
    return posix_path_to_uri(url);
}

// Child count of a timeline item: the insert handlers take an explicit
// position, and we always append.
int child_count(caf::scoped_actor &sys, const caf::actor &item_actor) {
    return static_cast<int>(
        request_receive<timeline::Item>(*sys, item_actor, timeline::item_atom_v)
            .children()
            .size());
}

QVariantMap with_uuid(const Uuid &uuid) {
    QVariantMap r;
    r["uuid"] = QStringFromStd(to_string(uuid));
    return r;
}

QString uuid_string(const Uuid &uuid) { return QStringFromStd(to_string(uuid)); }

std::string actor_name(caf::scoped_actor &sys, const caf::actor &a) {
    return request_receive<std::string>(*sys, a, utility::name_atom_v);
}
std::string actor_type(caf::scoped_actor &sys, const caf::actor &a) {
    return request_receive<std::string>(*sys, a, utility::type_atom_v);
}

// What the page gets for one media item: {uuid, name, status, flagColour,
// flagText, uri}. uri is the current image source's reference as xSTUDIO
// holds it (file:// or http(s)://, percent-encoded), "" while the item is
// still being probed and has no source yet.
QVariantMap media_entry(caf::scoped_actor &sys, const UuidActor &ua) {
    auto status =
        request_receive<media::MediaStatus>(*sys, ua.actor(), media::media_status_atom_v);
    auto [flag, text] = request_receive<std::tuple<std::string, std::string>>(
        *sys, ua.actor(), playlist::reflag_container_atom_v);
    std::string uri;
    try {
        uri = to_string(request_receive<MediaReference>(
                            *sys, ua.actor(), media::media_reference_atom_v, media::MT_IMAGE)
                            .uri());
    } catch (const std::exception &) {
        // "No MediaSources": not probed yet.
    }
    QVariantMap e;
    e["uuid"]       = QStringFromStd(to_string(ua.uuid()));
    e["name"]       = QStringFromStd(actor_name(sys, ua.actor()));
    e["status"]     = QStringFromStd(media::to_string(status));
    e["flagColour"] = QStringFromStd(flag);
    e["flagText"]   = QStringFromStd(text);
    e["uri"]        = QStringFromStd(uri);
    return e;
}

template <typename M>
auto lookup(std::mutex &m, M &map, const QString &uuid, const char *what) {
    std::lock_guard lock(m);
    auto it = map.find(Uuid(StdFromQString(uuid)));
    if (it == map.end())
        throw std::runtime_error(std::string("Unknown ") + what + " " + StdFromQString(uuid));
    return it->second;
}

// The queue for addMedia. Adds wait in this actor's mailbox; it takes them in
// order, passes each to the playlist and answers it when the playlist
// replies, without waiting for one reply before passing on the next add.
// The playlist lists the item straight away, in the order the adds were made,
// opens the file in the background and replies when its detail is known.
class MediaLoaderActor : public caf::event_based_actor {
  public:
    // Called on this actor's thread with the media item, or an error.
    using Done = std::function<void(const int, const UuidActor &, const std::string &)>;

    MediaLoaderActor(caf::actor_config &cfg, Done done)
        : caf::event_based_actor(cfg), done_(std::move(done)) {
        behavior_.assign([=](playlist::add_media_atom,
                             const int id,
                             const caf::actor &playlist,
                             const std::string &url) {
            caf::uri uri;
            try {
                uri = uri_from_string(url);
            } catch (const std::exception &err) {
                done_(id, UuidActor(), err.what());
                return;
            }
            mail(playlist::add_media_atom_v, media_name(uri), uri, Uuid())
                .request(playlist, caf::infinite)
                .then(
                    [=](const UuidActor &media) { done_(id, media, ""); },
                    [=](caf::error &err) { done_(id, UuidActor(), to_string(err)); });
        });
    }

    caf::behavior make_behavior() override { return behavior_; }
    const char *name() const override { return "WebBridgeMediaLoader"; }

  private:
    // The file's stem up to its first '.', as the playlist names media it
    // loads from a URI: 'shot_010.0001.exr' becomes 'shot_010'.
    static std::string media_name(const caf::uri &uri) {
        auto stem         = fs::path(uri_to_posix_path(uri)).stem().string();
        const auto dotpos = stem.find(".");
        if (dotpos && dotpos != std::string::npos)
            stem = std::string(stem, 0, dotpos);
        return stem;
    }

    Done done_;
    caf::behavior behavior_;
};

} // namespace

WebBridgeUI::WebBridgeUI(QObject *parent) : QObject(parent) {
    // One thread so calls from the page run in the order they were made.
    worker_.setMaxThreadCount(1);
    media_loader_ = CafSystemObject::get_actor_system().spawn<MediaLoaderActor>(
        [this](const int id, const UuidActor &media, const std::string &error) {
            media_loaded(id, media, error);
        });
}

WebBridgeUI::~WebBridgeUI() {
    worker_.waitForDone();
    // The loader calls back into this object; see it gone before we are.
    caf::scoped_actor sys{CafSystemObject::get_actor_system()};
    sys->send_exit(media_loader_, caf::exit_reason::user_shutdown);
    sys->wait_for(media_loader_);
}

QString WebBridgeUI::ping() const {
    spdlog::debug("WebBridgeUI::ping");
    return QStringLiteral("pong");
}

QVariantMap WebBridgeUI::version() const {
    QVariantMap r;
    r["xstudio"] = QStringLiteral(PROJECT_VERSION);
    r["bridge"]  = kBridgeApiVersion;
    return r;
}

int WebBridgeUI::run(std::function<QVariantMap()> fn) {
    const int id = next_request_id_++;
    (void)QtConcurrent::run(&worker_, [=]() {
        QVariantMap r;
        try {
            r = fn();
            if (not r.contains("ok"))
                r["ok"] = true;
        } catch (const std::exception &err) {
            spdlog::warn("WebBridgeUI request {}: {}", id, err.what());
            r.clear();
            r["ok"]    = false;
            r["error"] = QString::fromUtf8(err.what());
        }
        reply(id, r);
    });
    return id;
}

void WebBridgeUI::reply(const int id, const QVariantMap &r) {
    // Signals must leave from the object's own thread: QWebChannel
    // connected to them there.
    QMetaObject::invokeMethod(
        this, [this, id, r]() { emit result(id, r); }, Qt::QueuedConnection);
}

caf::actor WebBridgeUI::session_actor() {
    auto &system = CafSystemObject::get_actor_system();
    caf::scoped_actor sys{system};
    auto global = system.registry().template get<caf::actor>(global_registry);
    return request_receive<caf::actor>(*sys, global, session::session_atom_v);
}

caf::actor WebBridgeUI::playlist_handle(const QString &uuid) {
    return lookup(mutex_, playlists_, uuid, "playlist");
}
WebBridgeUI::TimelineHandle WebBridgeUI::timeline_handle(const QString &uuid) {
    return lookup(mutex_, timelines_, uuid, "timeline");
}
WebBridgeUI::TrackHandle WebBridgeUI::track_handle(const QString &uuid) {
    return lookup(mutex_, tracks_, uuid, "track");
}
UuidActor WebBridgeUI::media_handle(const QString &uuid) {
    return lookup(mutex_, media_, uuid, "media");
}
caf::actor WebBridgeUI::item_handle(const QString &uuid) {
    return lookup(mutex_, items_, uuid, "item");
}
caf::actor WebBridgeUI::contact_sheet_handle(const QString &uuid) {
    return lookup(mutex_, contact_sheets_, uuid, "contact sheet");
}

caf::actor WebBridgeUI::timeline_playhead(const TimelineHandle &tl) {
    auto &system = CafSystemObject::get_actor_system();
    caf::scoped_actor sys{system};
    return request_receive<UuidActor>(*sys, tl.timeline, playlist::get_playhead_atom_v).actor();
}

caf::actor WebBridgeUI::playlist_selection(const caf::actor &playlist) {
    auto &system = CafSystemObject::get_actor_system();
    caf::scoped_actor sys{system};
    return request_receive<caf::actor>(*sys, playlist, playlist::selection_actor_atom_v);
}

Uuid WebBridgeUI::register_timeline(const UuidActor &tl) {
    auto &system = CafSystemObject::get_actor_system();
    caf::scoped_actor sys{system};
    auto rate = request_receive<FrameRate>(*sys, tl.actor(), utility::rate_atom_v);
    // The timeline's only child is its stack; tracks go into that.
    auto stack = request_receive<timeline::Item>(*sys, tl.actor(), timeline::item_atom_v, 0);
    std::lock_guard lock(mutex_);
    timelines_[tl.uuid()] = TimelineHandle{tl.actor(), stack.actor(), rate};
    return tl.uuid();
}

// ---- worker-thread implementations -----------------------------------------

Uuid WebBridgeUI::insert_track(
    const TimelineHandle &tl, const std::string &name, const bool video, const std::string &colour) {
    auto &system = CafSystemObject::get_actor_system();
    caf::scoped_actor sys{system};

    const auto uuid = Uuid::generate();
    auto track      = system.spawn<timeline::TrackActor>(
        name,
        tl.rate,
        video ? media::MediaType::MT_IMAGE : media::MediaType::MT_AUDIO,
        uuid);

    request_receive<JsonStore>(
        *sys,
        tl.stack,
        timeline::insert_item_atom_v,
        child_count(sys, tl.stack),
        UuidActorVector({UuidActor(uuid, track)}));

    const auto flag = flag_from_colour(colour);
    if (not flag.empty())
        request_receive<JsonStore>(*sys, track, timeline::item_flag_atom_v, flag);

    std::lock_guard lock(mutex_);
    tracks_[uuid] = TrackHandle{track, tl.rate};
    items_[uuid]  = track;
    return uuid;
}

std::pair<Uuid, int>
WebBridgeUI::insert_clip(const TrackHandle &tr, const UuidActor &media, const std::string &name) {
    auto &system = CafSystemObject::get_actor_system();
    caf::scoped_actor sys{system};

    const auto uuid = Uuid::generate();
    auto clip       = system.spawn<timeline::ClipActor>(media, name, uuid);
    request_receive<JsonStore>(
        *sys,
        tr.track,
        timeline::insert_item_atom_v,
        child_count(sys, tr.track),
        UuidActorVector({UuidActor(uuid, clip)}));

    const auto item = request_receive<timeline::Item>(*sys, clip, timeline::item_atom_v);
    {
        std::lock_guard lock(mutex_);
        items_[uuid] = clip;
    }
    return {uuid, item.trimmed_frame_duration().frames()};
}

Uuid WebBridgeUI::insert_gap(const TrackHandle &tr, const int frames) {
    auto &system = CafSystemObject::get_actor_system();
    caf::scoped_actor sys{system};

    const auto uuid = Uuid::generate();
    auto gap        = system.spawn<timeline::GapActor>(
        "Gap", FrameRateDuration(std::max(frames, 0), tr.rate), uuid);
    request_receive<JsonStore>(
        *sys,
        tr.track,
        timeline::insert_item_atom_v,
        child_count(sys, tr.track),
        UuidActorVector({UuidActor(uuid, gap)}));
    std::lock_guard lock(mutex_);
    items_[uuid] = gap;
    return uuid;
}

// ---- page-facing methods ----------------------------------------------------
// Each does one Python-API thing on the worker thread and reports on result().

int WebBridgeUI::findPlaylist(const QString &name) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playlist = request_receive<caf::actor>(
            *sys, session_actor(), session::get_playlist_atom_v, StdFromQString(name));
        QVariantMap r;
        r["uuid"] = QString();
        if (playlist) {
            auto uuid = request_receive<Uuid>(*sys, playlist, utility::uuid_atom_v);
            std::lock_guard lock(mutex_);
            playlists_[uuid] = playlist;
            r["uuid"]        = QStringFromStd(to_string(uuid));
        }
        return r;
    });
}

int WebBridgeUI::createPlaylist(const QString &name) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto ua = request_receive<UuidUuidActor>(
                      *sys,
                      session_actor(),
                      session::add_playlist_atom_v,
                      StdFromQString(name),
                      Uuid(),
                      false)
                      .second;
        std::lock_guard lock(mutex_);
        playlists_[ua.uuid()] = ua.actor();
        return with_uuid(ua.uuid());
    });
}

int WebBridgeUI::setMediaRate(const QString &playlistUuid, const double fps) {
    return run([=]() {
        auto playlist = playlist_handle(playlistUuid);
        // Same as the Python binding: FrameRate(fps) is 1/fps seconds per frame.
        caf::anon_mail(session::media_rate_atom_v, FrameRate(1.0 / (fps > 0.0 ? fps : 24.0)))
            .send(playlist);
        return QVariantMap();
    });
}

int WebBridgeUI::createTimeline(const QString &playlistUuid, const QString &name) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playlist = playlist_handle(playlistUuid);
        auto tl       = request_receive<UuidUuidActor>(
                      *sys,
                      playlist,
                      playlist::create_timeline_atom_v,
                      StdFromQString(name),
                      Uuid(),
                      false,
                      false)
                      .second;
        return with_uuid(register_timeline(tl));
    });
}

int WebBridgeUI::insertVideoTrack(
    const QString &timelineUuid, const QString &name, const QString &colour) {
    return run([=]() {
        return with_uuid(insert_track(
            timeline_handle(timelineUuid), StdFromQString(name), true, StdFromQString(colour)));
    });
}

int WebBridgeUI::insertAudioTrack(const QString &timelineUuid, const QString &name) {
    return run([=]() {
        return with_uuid(
            insert_track(timeline_handle(timelineUuid), StdFromQString(name), false, ""));
    });
}

int WebBridgeUI::addMedia(const QString &playlistUuid, const QString &url) {
    // The worker only hands the add to the loader, so adds start in call
    // order without holding up the calls behind them; media_loaded() answers.
    const int id = next_request_id_++;
    (void)QtConcurrent::run(&worker_, [=]() {
        try {
            caf::anon_mail(
                playlist::add_media_atom_v,
                id,
                playlist_handle(playlistUuid),
                StdFromQString(url))
                .send(media_loader_);
        } catch (const std::exception &err) {
            media_loaded(id, UuidActor(), err.what());
        }
    });
    return id;
}

void WebBridgeUI::media_loaded(const int id, const UuidActor &media, const std::string &error) {
    QVariantMap r;
    if (error.empty()) {
        {
            std::lock_guard lock(mutex_);
            media_[media.uuid()] = media;
        }
        r       = with_uuid(media.uuid());
        r["ok"] = true;
    } else {
        spdlog::warn("WebBridgeUI request {}: {}", id, error);
        r["ok"]    = false;
        r["error"] = QStringFromStd(error);
    }
    reply(id, r);
}

int WebBridgeUI::insertClip(
    const QString &trackUuid, const QString &mediaUuid, const QString &name) {
    return run([=]() {
        auto [uuid, frames] =
            insert_clip(track_handle(trackUuid), media_handle(mediaUuid), StdFromQString(name));
        auto r      = with_uuid(uuid);
        r["frames"] = frames;
        return r;
    });
}

int WebBridgeUI::insertGap(const QString &trackUuid, const int frames) {
    return run([=]() { return with_uuid(insert_gap(track_handle(trackUuid), frames)); });
}

int WebBridgeUI::showTimeline(const QString &timelineUuid) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto tl      = timeline_handle(timelineUuid);
        auto uuid    = request_receive<Uuid>(*sys, tl.timeline, utility::uuid_atom_v);
        auto session = session_actor();
        caf::anon_mail(session::viewport_active_media_container_atom_v, uuid).send(session);
        caf::anon_mail(session::active_media_container_atom_v, uuid).send(session);
        return QVariantMap();
    });
}

// ---- reading back -------------------------------------------------------------

int WebBridgeUI::listPlaylists() {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playlists = request_receive<std::vector<UuidActor>>(
            *sys, session_actor(), session::get_playlists_atom_v);
        QVariantList list;
        for (const auto &ua : playlists) {
            QVariantMap e;
            e["uuid"] = uuid_string(ua.uuid());
            e["name"] = QStringFromStd(actor_name(sys, ua.actor()));
            list.push_back(e);
            std::lock_guard lock(mutex_);
            playlists_[ua.uuid()] = ua.actor();
        }
        QVariantMap r;
        r["playlists"] = list;
        return r;
    });
}

int WebBridgeUI::listContainers(const QString &playlistUuid) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playlist   = playlist_handle(playlistUuid);
        auto containers = request_receive<std::vector<UuidActor>>(
            *sys, playlist, playlist::get_container_atom_v, true);
        QVariantList list;
        for (const auto &ua : containers) {
            const auto type = actor_type(sys, ua.actor());
            QVariantMap e;
            e["uuid"] = uuid_string(ua.uuid());
            e["name"] = QStringFromStd(actor_name(sys, ua.actor()));
            e["type"] = QStringFromStd(type);
            list.push_back(e);
            if (type == "Timeline") {
                register_timeline(ua);
            } else if (type == "ContactSheet") {
                std::lock_guard lock(mutex_);
                contact_sheets_[ua.uuid()] = ua.actor();
            }
        }
        QVariantMap r;
        r["containers"] = list;
        return r;
    });
}

int WebBridgeUI::listMedia(const QString &playlistUuid) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playlist = playlist_handle(playlistUuid);
        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);
        QVariantList list;
        for (const auto &ua : media) {
            list.push_back(media_entry(sys, ua));
            std::lock_guard lock(mutex_);
            media_[ua.uuid()] = ua;
        }
        QVariantMap r;
        r["media"] = list;
        return r;
    });
}

int WebBridgeUI::findTimeline(const QString &playlistUuid, const QString &name) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playlist   = playlist_handle(playlistUuid);
        auto containers = request_receive<std::vector<UuidActor>>(
            *sys, playlist, playlist::get_container_atom_v, true);
        const auto wanted = StdFromQString(name);
        QVariantMap r;
        r["uuid"] = QString();
        for (const auto &ua : containers) {
            if (actor_type(sys, ua.actor()) == "Timeline" and
                actor_name(sys, ua.actor()) == wanted) {
                r["uuid"] = uuid_string(register_timeline(ua));
                break;
            }
        }
        return r;
    });
}

int WebBridgeUI::playheadState(const QString &timelineUuid) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playhead = timeline_playhead(timeline_handle(timelineUuid));
        QVariantMap r;
        r["frame"]   = request_receive<int>(*sys, playhead, playhead::logical_frame_atom_v);
        r["playing"] = request_receive<bool>(*sys, playhead, playhead::play_atom_v);
        r["loopIn"] = request_receive<int>(*sys, playhead, playhead::simple_loop_start_atom_v);
        r["loopOut"] = request_receive<int>(*sys, playhead, playhead::simple_loop_end_atom_v);
        r["useLoop"] = request_receive<bool>(*sys, playhead, playhead::use_loop_range_atom_v);
        return r;
    });
}

int WebBridgeUI::selectedMedia(const QString &playlistUuid) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto selection = playlist_selection(playlist_handle(playlistUuid));
        auto selected  = request_receive<UuidActorVector>(
            *sys, selection, playhead::get_selected_sources_atom_v);
        QVariantList list;
        for (const auto &ua : selected) {
            list.push_back(media_entry(sys, ua));
            std::lock_guard lock(mutex_);
            media_[ua.uuid()] = ua;
        }
        QVariantMap r;
        r["media"] = list;
        return r;
    });
}

// ---- playback -----------------------------------------------------------------

int WebBridgeUI::play(const QString &timelineUuid, const bool playing) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playhead = timeline_playhead(timeline_handle(timelineUuid));
        request_receive<bool>(*sys, playhead, playhead::play_atom_v, playing);
        return QVariantMap();
    });
}

int WebBridgeUI::setFrame(const QString &timelineUuid, const int frame) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playhead = timeline_playhead(timeline_handle(timelineUuid));
        request_receive<bool>(*sys, playhead, playhead::jump_atom_v, frame);
        return QVariantMap();
    });
}

int WebBridgeUI::setLoopRange(
    const QString &timelineUuid, const int inFrame, const int outFrame, const bool enabled) {
    return run([=]() {
        auto playhead = timeline_playhead(timeline_handle(timelineUuid));
        caf::anon_mail(playhead::simple_loop_start_atom_v, inFrame).send(playhead);
        caf::anon_mail(playhead::simple_loop_end_atom_v, outFrame).send(playhead);
        caf::anon_mail(playhead::use_loop_range_atom_v, enabled).send(playhead);
        return QVariantMap();
    });
}

int WebBridgeUI::setLoopMode(const QString &timelineUuid, const QString &mode) {
    return run([=]() {
        static const std::map<std::string, playhead::LoopMode> modes = {
            {"Play Once", playhead::LM_PLAY_ONCE},
            {"Loop", playhead::LM_LOOP},
            {"Ping Pong", playhead::LM_PING_PONG}};
        auto it = modes.find(StdFromQString(mode));
        if (it == modes.end())
            throw std::runtime_error(
                "Unknown loop mode '" + StdFromQString(mode) +
                "' (Play Once, Loop, Ping Pong)");
        auto playhead = timeline_playhead(timeline_handle(timelineUuid));
        caf::anon_mail(playhead::loop_atom_v, it->second).send(playhead);
        return QVariantMap();
    });
}

int WebBridgeUI::setCompareMode(const QString &timelineUuid, const QString &mode) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playhead = timeline_playhead(timeline_handle(timelineUuid));
        request_receive<bool>(
            *sys, playhead, playhead::compare_mode_atom_v, StdFromQString(mode));
        return QVariantMap();
    });
}

// ---- editing the timeline -----------------------------------------------------

int WebBridgeUI::removeClips(
    const QString &trackUuid, const int index, const int count, const bool addGap) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto tr = track_handle(trackUuid);
        request_receive<std::pair<JsonStore, std::vector<timeline::Item>>>(
            *sys, tr.track, timeline::remove_item_atom_v, index, count, addGap);
        return QVariantMap();
    });
}

int WebBridgeUI::moveClips(
    const QString &trackUuid, const int start, const int count, const int dest) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto tr = track_handle(trackUuid);
        request_receive<JsonStore>(
            *sys, tr.track, timeline::move_item_atom_v, start, count, dest);
        return QVariantMap();
    });
}

int WebBridgeUI::splitClip(const QString &trackUuid, const int index, const int frame) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto tr = track_handle(trackUuid);
        request_receive<JsonStore>(*sys, tr.track, timeline::split_item_atom_v, index, frame);
        return QVariantMap();
    });
}

int WebBridgeUI::clearTimeline(const QString &timelineUuid) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto tl          = timeline_handle(timelineUuid);
        const auto count = child_count(sys, tl.stack);
        if (count > 0)
            request_receive<JsonStore>(
                *sys, tl.stack, timeline::erase_item_atom_v, 0, count, false);
        return QVariantMap();
    });
}

int WebBridgeUI::setItemEnabled(const QString &itemUuid, const bool enabled) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        request_receive<JsonStore>(
            *sys, item_handle(itemUuid), plugin_manager::enable_atom_v, enabled);
        return QVariantMap();
    });
}

int WebBridgeUI::setItemName(const QString &itemUuid, const QString &name) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        request_receive<JsonStore>(
            *sys, item_handle(itemUuid), timeline::item_name_atom_v, StdFromQString(name));
        return QVariantMap();
    });
}

int WebBridgeUI::setItemFlag(const QString &itemUuid, const QString &colour) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        request_receive<JsonStore>(
            *sys,
            item_handle(itemUuid),
            timeline::item_flag_atom_v,
            flag_from_colour(StdFromQString(colour)));
        return QVariantMap();
    });
}

// ---- media and playlists ------------------------------------------------------

int WebBridgeUI::setMediaFlag(
    const QString &mediaUuid, const QString &colour, const QString &text) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        request_receive<bool>(
            *sys,
            media_handle(mediaUuid).actor(),
            playlist::reflag_container_atom_v,
            std::make_tuple(
                std::optional<std::string>(flag_from_colour(StdFromQString(colour))),
                std::optional<std::string>(StdFromQString(text))));
        return QVariantMap();
    });
}

int WebBridgeUI::removeMedia(const QString &playlistUuid, const QString &mediaUuid) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playlist = playlist_handle(playlistUuid);
        auto media    = media_handle(mediaUuid);
        request_receive<bool>(*sys, playlist, playlist::remove_media_atom_v, media.uuid());
        std::lock_guard lock(mutex_);
        media_.erase(media.uuid());
        return QVariantMap();
    });
}

int WebBridgeUI::clearPlaylist(const QString &playlistUuid) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playlist = playlist_handle(playlistUuid);
        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);
        UuidVector uuids;
        for (const auto &ua : media)
            uuids.push_back(ua.uuid());
        if (not uuids.empty())
            request_receive<bool>(*sys, playlist, playlist::remove_media_atom_v, uuids);
        std::lock_guard lock(mutex_);
        for (const auto &u : uuids)
            media_.erase(u);
        return QVariantMap();
    });
}

int WebBridgeUI::removePlaylist(const QString &playlistUuid) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playlist   = playlist_handle(playlistUuid);
        const auto uuid = request_receive<Uuid>(*sys, playlist, utility::uuid_atom_v);
        request_receive<bool>(*sys, session_actor(), playlist::remove_container_atom_v, uuid);
        std::lock_guard lock(mutex_);
        playlists_.erase(uuid);
        return QVariantMap();
    });
}

int WebBridgeUI::setSelection(const QString &playlistUuid, const QStringList &mediaUuids) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto selection = playlist_selection(playlist_handle(playlistUuid));
        UuidVector uuids;
        for (const auto &m : mediaUuids)
            uuids.push_back(media_handle(m).uuid());
        request_receive<bool>(*sys, selection, playlist::select_media_atom_v, uuids);
        return QVariantMap();
    });
}

// ---- contact sheets -----------------------------------------------------------

int WebBridgeUI::createContactSheet(const QString &playlistUuid, const QString &name) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto playlist = playlist_handle(playlistUuid);
        auto cs       = request_receive<UuidUuidActor>(
                      *sys,
                      playlist,
                      playlist::create_contact_sheet_atom_v,
                      StdFromQString(name),
                      Uuid(),
                      false)
                      .second;
        std::lock_guard lock(mutex_);
        contact_sheets_[cs.uuid()] = cs.actor();
        return with_uuid(cs.uuid());
    });
}

int WebBridgeUI::addMediaToContactSheet(
    const QString &contactSheetUuid, const QString &mediaUuid) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto cs    = contact_sheet_handle(contactSheetUuid);
        auto media = media_handle(mediaUuid);
        request_receive<UuidActor>(*sys, cs, playlist::add_media_atom_v, media, Uuid());
        return QVariantMap();
    });
}

// ---- session ------------------------------------------------------------------

int WebBridgeUI::setViewedContainer(const QString &containerUuid) {
    return run([=]() {
        Uuid uuid;
        {
            std::lock_guard lock(mutex_);
            const Uuid key(StdFromQString(containerUuid));
            if (playlists_.count(key) or timelines_.count(key) or contact_sheets_.count(key))
                uuid = key;
        }
        if (uuid.is_null())
            throw std::runtime_error("Unknown container " + StdFromQString(containerUuid));
        auto session = session_actor();
        caf::anon_mail(session::viewport_active_media_container_atom_v, uuid).send(session);
        caf::anon_mail(session::active_media_container_atom_v, uuid).send(session);
        return QVariantMap();
    });
}

int WebBridgeUI::save() {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        request_receive<size_t>(*sys, session_actor(), global_store::save_atom_v);
        return QVariantMap();
    });
}

int WebBridgeUI::saveAs(const QString &path) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        request_receive<size_t>(
            *sys, session_actor(), global_store::save_atom_v, posix_path_to_uri(StdFromQString(path)));
        return QVariantMap();
    });
}

// ---- telling the user ---------------------------------------------------------

int WebBridgeUI::notify(const QString &kind, const QString &text, const int expiresSeconds) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        const auto k       = StdFromQString(kind);
        const auto t       = StdFromQString(text);
        const auto expires = std::chrono::seconds(std::max(expiresSeconds, 1));
        Notification n;
        if (k == "info")
            n = Notification::InfoNotification(t, expires);
        else if (k == "warn")
            n = Notification::WarnNotification(t, expires);
        else if (k == "processing")
            n = Notification::ProcessingNotification(t);
        else
            throw std::runtime_error("Unknown notification kind '" + k + "' (info, warn, processing)");
        request_receive<bool>(*sys, session_actor(), utility::notification_atom_v, n);
        return QVariantMap();
    });
}

int WebBridgeUI::addNote(
    const QString &mediaUuid,
    const QString &subject,
    const QString &text,
    const QString &category,
    const QString &colour,
    const int startFrame,
    const int durationFrames) {
    return run([=]() {
        auto &system = CafSystemObject::get_actor_system();
        caf::scoped_actor sys{system};
        auto media     = media_handle(mediaUuid);
        auto bookmarks = request_receive<caf::actor>(
            *sys, session_actor(), bookmark::get_bookmark_atom_v);
        auto note =
            request_receive<UuidActor>(*sys, bookmarks, bookmark::add_bookmark_atom_v, media);

        bookmark::BookmarkDetail detail;
        detail.subject_  = StdFromQString(subject);
        detail.note_     = StdFromQString(text);
        detail.category_ = StdFromQString(category);
        detail.colour_   = StdFromQString(colour);
        if (startFrame >= 0) {
            auto rate = request_receive<FrameRate>(
                *sys, media.actor(), utility::rate_atom_v, media::MT_IMAGE);
            detail.start_    = FrameRateDuration(startFrame, rate).duration();
            detail.duration_ = FrameRateDuration(std::max(durationFrames, 1), rate).duration();
        }
        request_receive<bool>(*sys, note.actor(), bookmark::bookmark_detail_atom_v, detail);
        return with_uuid(note.uuid());
    });
}

// Layout and tab selection live in QML; the bridge relays the request from
// its own thread and the panel answers through the *Done slots below.

int WebBridgeUI::selectLayout(const QString &name) {
    return run([=]() {
        QMetaObject::invokeMethod(
            this, [this, name]() { emit layoutRequested(name); }, Qt::QueuedConnection);
        return QVariantMap();
    });
}

int WebBridgeUI::selectPanel(const QString &name) {
    const int id = next_request_id_++;
    QMetaObject::invokeMethod(
        this, [this, id, name]() { emit panelRequested(id, name); }, Qt::QueuedConnection);
    return id;
}

void WebBridgeUI::panelRequestDone(const int requestId, const bool found, const QString &name) {
    QVariantMap r;
    r["ok"] = found;
    if (not found)
        r["error"] = QStringLiteral("No panel of type '%1' in the current layout").arg(name);
    emit result(requestId, r);
}
