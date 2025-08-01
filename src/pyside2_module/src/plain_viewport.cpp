// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/qt/viewport_widget.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/ui/mouse.hpp"

CAF_PUSH_WARNINGS
#include "plain_viewport.hpp"
#include "caf_system.hpp"
#include <QVBoxLayout>
CAF_POP_WARNINGS

using namespace xstudio;

PlainViewport::PlainViewport(QWidget *parent) : QWidget(parent) {

    // As far as I can tell caf only allows config to be modified
    // through cli args. We prefer the 'sharing' policy rather than
    // 'stealing'. The latter results in multiple threads spinning
    // aggressively pushing CPU load very high during playback.

    caf::scoped_actor self{CafSys::instance()->system()};

    // Tell the global_actor to create the 'studio' top level object that manages the 'session'
    // object(s)
    utility::request_receive<caf::actor>(
        *self, CafSys::instance()->global_actor(), global::create_studio_atom_v, "XStudio");

    // Create a session object
    auto session = utility::request_receive<caf::actor>(
        *self, CafSys::instance()->global_actor(), session::session_atom_v);

    // Create the xstudio viewport widget/actor
    viewport_widget_ = new ui::qt::ViewportGLWidget(this);
    viewport_widget_->init(CafSys::instance()->system());

    QVBoxLayout *layout = new QVBoxLayout((QWidget *)this);
    layout->addWidget(viewport_widget_, 1);

    // Get the session to create a playlist
    auto playlist = utility::request_receive<utility::UuidUuidActor>(
                        *self, session, session::add_playlist_atom_v, "Test")
                        .second.actor();

    // ask the playlist to create a playhead
    caf::actor playhead = utility::request_receive<utility::UuidActor>(
                              *self, playlist, playlist::create_playhead_atom_v)
                              .actor();

    // pass the playhead to the viewport - it will 'attach' itself to the playhead
    // so that it is receiving video frames for display
    viewport_widget_->set_playhead(playhead);
}

PlainViewport::~PlainViewport() {}

void PlainViewport::resizeEvent(QResizeEvent *event) { qDebug() << event; }
