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
#include "xstudio/media/media.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/item.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"

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
    return {uuid, item.trimmed_frame_duration().frames()};
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
