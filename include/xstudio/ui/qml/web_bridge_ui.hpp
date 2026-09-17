// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>

#include <caf/all.hpp>

CAF_PUSH_WARNINGS
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QVariantMap>
CAF_POP_WARNINGS

// include CMake auto-generated export hpp
#include "xstudio/ui/qml/studio_qml_export.h"

#include "xstudio/utility/frame_rate.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio::ui::qml {

// Native object exposed to pages loaded in the web browser panel, over
// QWebChannel. Registered as the "xstudioWebBridge" context property when
// built with BUILD_WEBENGINE.
//
// Every method returns a request id immediately and does its work on one
// worker thread, in call order. addMedia is the exception: adds are handed
// to a loader in call order, but a load is a wait on the playlist, so the
// loader passes each one on without waiting for the one before and answers
// each as it lands, in any order. The outcome arrives on the result() signal
// with that id and a map that always carries "ok" (bool) and, on failure,
// "error" (string). Methods that create something add "uuid"; insertClip
// adds "frames". The bridge only accepts handles it has handed out; a UUID
// copied from elsewhere is rejected.
//
// These are deliberately thin primitives, mirroring the Python timeline API.
// Workflow logic (how a payload becomes tracks, clips and gaps, what to show
// afterwards) belongs in the web page's JavaScript, where one server can be
// updated instead of every xSTUDIO install.
class STUDIO_QML_EXPORT WebBridgeUI : public QObject {

    Q_OBJECT

  public:
    explicit WebBridgeUI(QObject *parent = nullptr);
    ~WebBridgeUI() override;

    // Round-trip check: page JS calls ping() and gets "pong" back.
    Q_INVOKABLE [[nodiscard]] QString ping() const;

    // What the page is talking to, so a web tool can ask for a newer xSTUDIO:
    // {xstudio: "1.3.0", bridge: <API version>}.
    // Bump kBridgeApiVersion whenever a method or result field changes.
    static constexpr int kBridgeApiVersion = 1;
    Q_INVOKABLE [[nodiscard]] QVariantMap version() const;

    // One call per Python API method, nothing composite. Handles are UUID
    // strings; a page only gets handles to things the bridge created or
    // looked up for it.

    // session.playlists / session.create_playlist(name).
    // result: {uuid} ("" when no playlist has that name) / {uuid}
    Q_INVOKABLE int findPlaylist(const QString &name);
    Q_INVOKABLE int createPlaylist(const QString &name);

    // playlist.create_timeline(name, with_tracks=False). result: {uuid}
    Q_INVOKABLE int createTimeline(const QString &playlistUuid, const QString &name);

    // timeline.insert_video_track(name) / insert_audio_track(name), plus the
    // track's item_flag colour (RRGGBB, optional '#', "" for none). result: {uuid}
    Q_INVOKABLE int
    insertVideoTrack(const QString &timelineUuid, const QString &name, const QString &colour);
    Q_INVOKABLE int insertAudioTrack(const QString &timelineUuid, const QString &name);

    // playlist.add_media(url_or_path). result: {uuid}
    Q_INVOKABLE int addMedia(const QString &playlistUuid, const QString &url);

    // track.insert_clip(media), appended. result: {uuid, frames} where frames
    // is the clip's trimmed length, what the script reads back for gap sizes.
    Q_INVOKABLE int
    insertClip(const QString &trackUuid, const QString &mediaUuid, const QString &name);

    // session.viewed_container = timeline; session.inspected_container = timeline.
    Q_INVOKABLE int showTimeline(const QString &timelineUuid);

    // Switch the main window to a named layout ("Review", "Timeline",
    // "Present"; empty picks "Review"), so the page can hand the screen back
    // to the viewer. Handled in QML via layoutRequested. result: {ok}
    Q_INVOKABLE int selectLayout(const QString &name);

    // Bring the first tab showing the named panel type ("Viewport", ...) to
    // the front of its tab strip in the current layout. Handled in QML via
    // panelRequested, which answers with panelRequestDone. result: {ok}
    Q_INVOKABLE int selectPanel(const QString &name);
    Q_INVOKABLE void panelRequestDone(const int requestId, const bool found, const QString &name);

  signals:
    void result(int requestId, const QVariantMap &result);
    void layoutRequested(const QString &name);
    void panelRequested(int requestId, const QString &name);

  private:
    struct TimelineHandle {
        caf::actor timeline;
        caf::actor stack;
        utility::FrameRate rate;
    };
    struct TrackHandle {
        caf::actor track;
        utility::FrameRate rate;
    };

    caf::actor playlist_handle(const QString &uuid);
    TimelineHandle timeline_handle(const QString &uuid);
    TrackHandle track_handle(const QString &uuid);
    utility::UuidActor media_handle(const QString &uuid);
    caf::actor session_actor();
    // Register a timeline actor as a handle, fetching its rate and stack.
    utility::Uuid register_timeline(const utility::UuidActor &tl);

    // Worker-thread implementations. Each throws on failure.
    utility::Uuid insert_track(
        const TimelineHandle &tl,
        const std::string &name,
        const bool video,
        const std::string &colour);
    std::pair<utility::Uuid, int> insert_clip(
        const TrackHandle &tr, const utility::UuidActor &media, const std::string &name);

    // Queue fn on the worker thread; deliver its map on result() with a fresh id.
    int run(std::function<QVariantMap()> fn);
    // Emit result(id, r) from the object's own thread.
    void reply(const int id, const QVariantMap &r);
    // The loader's answer to an addMedia, called on the loader's thread.
    void media_loaded(const int id, const utility::UuidActor &media, const std::string &error);

    QThreadPool worker_;      // one thread: calls run in order
    caf::actor media_loader_; // addMedia loads: passed on, answered as they land
    int next_request_id_{1};

    std::mutex mutex_;
    std::map<utility::Uuid, caf::actor> playlists_;
    std::map<utility::Uuid, TimelineHandle> timelines_;
    std::map<utility::Uuid, TrackHandle> tracks_;
    std::map<utility::Uuid, utility::UuidActor> media_;
};

} // namespace xstudio::ui::qml
