// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/playhead/playhead.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/playhead/playhead_selection_actor.hpp"
#include "xstudio/ui/qml/playlist_selection_ui.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/timecode.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

PlaylistSelectionUI::PlaylistSelectionUI(QObject *parent)
    : QMLActor(parent), backend_(), backend_events_() {}

// helper ?

void PlaylistSelectionUI::set_backend(caf::actor backend) {

    scoped_actor sys{system()};

    backend_ = backend;
    // get backend state..
    if (backend_events_) {
        try {
            request_receive<bool>(
                *sys, backend_events_, broadcast::leave_broadcast_atom_v, as_actor());
        } catch (const std::exception &e) {
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
        backend_events_ = caf::actor();
    }

    try {
        auto detail     = request_receive<ContainerDetail>(*sys, backend_, detail_atom_v);
        backend_events_ = detail.group_;
        request_receive<bool>(
            *sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    // this call triggers the backend to send us back the current selection
    anon_send(backend_, playhead::get_selection_atom_v, as_actor());
}

void PlaylistSelectionUI::initSystem(QObject *system_qobject) {
    init(dynamic_cast<QMLActor *>(system_qobject)->system());
}

void PlaylistSelectionUI::init(actor_system &system_) {

    QMLActor::init(system_);

    spdlog::debug("PlaylistSelectionUI init");

    scoped_actor sys{system()};

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {

            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg & /*msg*/) {
                // 		if(msg.source == store_events)
                // unsubscribe();
            },

            [=](utility::event_atom, playhead::source_atom, const std::vector<caf::actor> &) {
                // This comes from the 'PlaylistSelectionActor' and tells us that
                // the list of viewables has changed - we then request the selection
                // as a Uuid list which is received just below
                anon_send(backend_, playhead::get_selection_atom_v, as_actor());
            },

            [=](utility::event_atom, utility::change_atom) {
                // something changed in the backend... update the selection
                // list for QML
                anon_send(backend_, playhead::get_selection_atom_v, as_actor());
            },

            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {},

            [=](utility::event_atom,
                playhead::selection_changed_atom,
                const UuidList &selection) {
                // this message is returned by the backend when we send it
                // get_selection_atom
                selected_media_.clear();
                for (const auto &uuid : selection) {
                    selected_media_.append(QUuidFromUuid(uuid));
                }
                emit selectionChanged();
            }};
    });
}

void PlaylistSelectionUI::newSelection(const QVariantList &selection_list) {

    utility::UuidList selection;
    selected_media_.clear();
    for (auto v : selection_list) {
        selection.push_back(UuidFromQUuid(v.toUuid()));
        // update our local copy as there is implied latency in this code.
        selected_media_.push_back(v.toUuid());
    }

    anon_send(backend_, playlist::select_media_atom_v, selection);
}


QStringList PlaylistSelectionUI::selectedMediaFilePaths() {

    QStringList result;
    for (const auto &i : get_selected_media_refences())
        result.append(QStringFromStd(uri_to_posix_path(i.uri())).replace("{:04d}", "####"));
    return result;
}

QStringList PlaylistSelectionUI::selectedMediaFileNames() {

    QStringList results = selectedMediaFilePaths();
    for (auto &path : results) {
        path = path.mid(path.lastIndexOf("/") + 1);
    }
    return results;
}

QList<QUrl> PlaylistSelectionUI::selectedMediaURLs() {
    QList<QUrl> result;

    int file_frame;
    for (const auto &i : get_selected_media_refences()) {
        auto _uri = i.uri(0, file_frame);
        if (_uri)
            result.append(QUrlFromUri(*_uri));
    }

    return result;
}

std::vector<MediaReference> PlaylistSelectionUI::get_selected_media_refences() {
    std::vector<MediaReference> result;

    try {
        scoped_actor sys{system()};
        auto selection = request_receive_wait<std::vector<caf::actor>>(
            *sys,
            backend_,
            std::chrono::milliseconds(2500),
            playhead::get_selected_sources_atom_v);

        for (auto &actor : selection) {
            auto source = request_receive_wait<UuidActor>(
                *sys,
                actor,
                std::chrono::milliseconds(2500),
                media::current_media_source_atom_v);

            result.push_back(request_receive_wait<MediaReference>(
                *sys,
                source.actor(),
                std::chrono::milliseconds(2500),
                media::media_reference_atom_v));
        }
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return result;
}

QVariantList PlaylistSelectionUI::currentSelection() const {
    QVariantList result;

    for (const auto &i : selected_media_)
        result.push_back(QVariant::fromValue(i));

    return result;
}


void PlaylistSelectionUI::deleteSelected() {
    anon_send(backend_, playhead::delete_selection_from_playlist_atom_v);
}

void PlaylistSelectionUI::gatherSourcesForSelected() {
    anon_send(backend_, media_hook::gather_media_sources_atom_v);
}

void PlaylistSelectionUI::evictSelected() {
    anon_send(backend_, playhead::evict_selection_from_cache_atom_v);
}

void PlaylistSelectionUI::moveSelectionByIndex(const int index) {
    anon_send(backend_, playhead::select_next_media_atom_v, index);
}
