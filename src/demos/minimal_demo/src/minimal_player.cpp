// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <caf/io/middleman.hpp>

#include "xstudio/ui/qt/viewport_widget.hpp"

#include "xstudio/atoms.hpp"
#include "xstudio/caf_utility/caf_setup.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/utility/serialise_headers.hpp"

CAF_PUSH_WARNINGS
#include <QApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QMainWindow>
#include <QOpenGLWidget>
CAF_POP_WARNINGS


int main(int argc, char **argv) {

    using namespace xstudio;

    utility::start_logger(spdlog::level::debug);
    ACTOR_INIT_GLOBAL_META()

    caf::core::init_global_meta_objects();
    caf::io::middleman::init_global_meta_objects();

    // As far as I can tell caf only allows config to be modified
    // through cli args. We prefer the 'sharing' policy rather than
    // 'stealing'. The latter results in multiple threads spinning
    // aggressively pushing CPU load very high during playback.
    const char *args[] = {argv[0], "--caf.scheduler.policy=sharing"};
    caf_utility::caf_config config{2, const_cast<char **>(args)};

    caf::actor_system system{config};

    caf::scoped_actor self{system};

    // Create the global actor that manages highest level objects
    caf::actor global_actor = self->spawn<global::GlobalActor>();

    // Tell the global_actor to create the 'studio' top level object that manages the 'session'
    // object(s)
    utility::request_receive<caf::actor>(
        *self, global_actor, global::create_studio_atom_v, "XStudio");

    // Create a session object
    auto session =
        utility::request_receive<caf::actor>(*self, global_actor, session::session_atom_v);

    // Create an utterly minimal Qt App with a single window
    QApplication app(argc, argv);
    QMainWindow *mainWindow = new QMainWindow();

    // Create the xstudio viewport widget/actor
    ui::qt::ViewportGLWidget *viewport = new ui::qt::ViewportGLWidget(mainWindow);
    viewport->init(system);
    mainWindow->setCentralWidget(viewport);
    mainWindow->show();

    // Get the session to create a playlist
    auto playlist = utility::request_receive_wait<utility::UuidUuidActor>(
                        *self,
                        session,
                        std::chrono::milliseconds(1000),
                        session::add_playlist_atom_v,
                        "Test")
                        .second.second;

    // Create a media actor from the path passed in as an arg
    auto suuid   = utility::Uuid::generate();
    auto media_1 = self->spawn<media::MediaActor>(
        "Media1",
        utility::Uuid(),
        utility::UuidActorVector({utility::UuidActor(
            suuid,
            self->spawn<media::MediaSourceActor>(
                "MediaSource1",
                utility::posix_path_to_uri(argv[1]),
                utility::FrameRate(timebase::k_flicks_24fps),
                suuid))}));

    // Add the media item to the playlist
    self->send(playlist, playlist::add_media_atom_v, media_1, utility::Uuid());

    // ask the playlist to create a playhead
    caf::actor playhead =
        utility::request_receive_wait<utility::UuidActor>(
            *self, playlist, std::chrono::milliseconds(1000), playlist::create_playhead_atom_v)
            .second;

    // pass the playhead to the viewport - it will 'attach' itself to the playhead
    // so that it is receiving video frames for display
    viewport->set_playhead(playhead);

    // start playback
    self->send(playhead, playhead::play_atom_v, true);

    // start Qt event loop
    app.exec();

    // cancel actors talking to them selves.
    system.clock().cancel_all();

    self->send_exit(global_actor, caf::exit_reason::user_shutdown);

    utility::stop_logger();

    std::exit(EXIT_SUCCESS);
}
