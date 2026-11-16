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
    static constexpr int kBridgeApiVersion = 2;
    Q_INVOKABLE [[nodiscard]] QVariantMap version() const;

    // One call per Python API method, nothing composite. Handles are UUID
    // strings; a page only gets handles to things the bridge created or
    // looked up for it.

    // session.playlists / session.create_playlist(name).
    // result: {uuid} ("" when no playlist has that name) / {uuid}
    Q_INVOKABLE int findPlaylist(const QString &name);
    Q_INVOKABLE int createPlaylist(const QString &name);

    // playlist.media_rate = FrameRate(fps). Set it before adding media.
    Q_INVOKABLE int setMediaRate(const QString &playlistUuid, const double fps);

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

    // track.insert_gap(frames), appended. result: {uuid}
    Q_INVOKABLE int insertGap(const QString &trackUuid, const int frames);

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

    // ---- reading back -----------------------------------------------------
    // These hand the page handles to things it did not create, so a page
    // that can call them can reach the whole session.

    // session.playlists. result: {playlists: [{uuid, name}]}. Registers each
    // as a playlist handle.
    Q_INVOKABLE int listPlaylists();

    // playlist.containers. result: {containers: [{uuid, name, type}]} for
    // the timelines, subsets and contact sheets in a playlist. Timelines and
    // contact sheets are registered as handles; subsets are reported only.
    Q_INVOKABLE int listContainers(const QString &playlistUuid);

    // playlist.media. result: {media: [{uuid, name, status, flagColour,
    // flagText, uri}]}, uri being the image source's reference as xSTUDIO
    // holds it, "" while the item is still being probed. Registers each as
    // a media handle.
    Q_INVOKABLE int listMedia(const QString &playlistUuid);

    // The timeline of that name in the playlist. result: {uuid} ("" when
    // none). Registers it as a timeline handle.
    Q_INVOKABLE int findTimeline(const QString &playlistUuid, const QString &name);

    // The timeline's playhead. result: {frame, playing, loopIn, loopOut,
    // useLoop}.
    Q_INVOKABLE int playheadState(const QString &timelineUuid);

    // playlist.playhead_selection.selected_sources. result: {media: [...]}
    // with the same entries as listMedia, in selection order. Registers each
    // as a media handle.
    Q_INVOKABLE int selectedMedia(const QString &playlistUuid);

    // ---- playback, on the timeline's playhead ------------------------------

    // playhead.playing = playing
    Q_INVOKABLE int play(const QString &timelineUuid, const bool playing);
    // playhead.position = frame
    Q_INVOKABLE int setFrame(const QString &timelineUuid, const int frame);
    // playhead.loop_in_point / loop_out_point / use_loop_range
    Q_INVOKABLE int setLoopRange(
        const QString &timelineUuid, const int inFrame, const int outFrame, const bool enabled);
    // playhead.looping: "Play Once", "Loop" or "Ping Pong"
    Q_INVOKABLE int setLoopMode(const QString &timelineUuid, const QString &mode);
    // playhead.compare_mode: "Off", "A/B", "String", "Grid", ...
    Q_INVOKABLE int setCompareMode(const QString &timelineUuid, const QString &mode);

    // ---- editing the timeline ---------------------------------------------

    // track.remove_child_at_index(index, count, add_gap)
    Q_INVOKABLE int removeClips(
        const QString &trackUuid, const int index, const int count, const bool addGap);
    // track.move_children(start, count, dest)
    Q_INVOKABLE int
    moveClips(const QString &trackUuid, const int start, const int count, const int dest);
    // track.split_child_at_index(index, frame)
    Q_INVOKABLE int splitClip(const QString &trackUuid, const int index, const int frame);
    // timeline.clear(): removes every track.
    Q_INVOKABLE int clearTimeline(const QString &timelineUuid);
    // item.enabled / item_name / item_flag on a track, clip or gap the
    // bridge created. Flag colour is RRGGBB, "" clears it.
    Q_INVOKABLE int setItemEnabled(const QString &itemUuid, const bool enabled);
    Q_INVOKABLE int setItemName(const QString &itemUuid, const QString &name);
    Q_INVOKABLE int setItemFlag(const QString &itemUuid, const QString &colour);

    // ---- media and playlists ----------------------------------------------

    // media.reflag(colour, text). Colour is RRGGBB, "" clears it.
    Q_INVOKABLE int
    setMediaFlag(const QString &mediaUuid, const QString &colour, const QString &text);
    // playlist.remove_media(media)
    Q_INVOKABLE int removeMedia(const QString &playlistUuid, const QString &mediaUuid);
    // playlist.clear()
    Q_INVOKABLE int clearPlaylist(const QString &playlistUuid);
    // session.remove_container(playlist)
    Q_INVOKABLE int removePlaylist(const QString &playlistUuid);
    // playlist.playhead_selection.set_selection(media)
    Q_INVOKABLE int setSelection(const QString &playlistUuid, const QStringList &mediaUuids);

    // ---- contact sheets -----------------------------------------------------

    // playlist.create_contact_sheet(name). result: {uuid}
    Q_INVOKABLE int createContactSheet(const QString &playlistUuid, const QString &name);
    // contact_sheet.add_media(media): the media must already be in the
    // playlist (from addMedia or listMedia); it is shared, not copied.
    Q_INVOKABLE int
    addMediaToContactSheet(const QString &contactSheetUuid, const QString &mediaUuid);

    // ---- session ------------------------------------------------------------

    // session.viewed_container = c; session.inspected_container = c, for a
    // playlist, timeline or contact sheet handle. showTimeline is this for
    // a timeline.
    Q_INVOKABLE int setViewedContainer(const QString &containerUuid);
    // session.save() / session.save_as(path)
    Q_INVOKABLE int save();
    Q_INVOKABLE int saveAs(const QString &path);

    // ---- telling the user ---------------------------------------------------

    // An xSTUDIO notification: kind is "info", "warn" or "processing".
    Q_INVOKABLE int notify(const QString &kind, const QString &text, const int expiresSeconds);
    // A note on a media item, all fields in one call. startFrame < 0 means
    // the whole item. result: {uuid}
    Q_INVOKABLE int addNote(
        const QString &mediaUuid,
        const QString &subject,
        const QString &text,
        const QString &category,
        const QString &colour,
        const int startFrame,
        const int durationFrames);

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
    caf::actor item_handle(const QString &uuid); // track, clip or gap
    caf::actor contact_sheet_handle(const QString &uuid);
    caf::actor session_actor();
    caf::actor timeline_playhead(const TimelineHandle &tl);
    caf::actor playlist_selection(const caf::actor &playlist);
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
    utility::Uuid insert_gap(const TrackHandle &tr, const int frames);

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
    std::map<utility::Uuid, caf::actor> items_; // tracks, clips and gaps
    std::map<utility::Uuid, caf::actor> contact_sheets_;
};

} // namespace xstudio::ui::qml
