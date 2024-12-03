// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/qt/viewport_widget.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/ui/mouse.hpp"

#include "xstudio/ui/qml/module_ui.hpp"          //NOLINT
#include "xstudio/ui/qml/embedded_python_ui.hpp" //NOLINT
#include "xstudio/ui/qml/helper_ui.hpp"          //NOLINT
#include "xstudio/ui/qml/bookmark_ui.hpp"        //NOLINT
#include "xstudio/ui/qml/studio_ui.hpp"          //NOLINT
#include "xstudio/ui/qml/thumbnail_provider_ui.hpp"
#include "xstudio/ui/qml/qml_viewport.hpp" //NOLINT
#include "xstudio/ui/mouse.hpp"

#include "QuickFuture"

Q_DECLARE_METATYPE(QList<QUuid>)
Q_DECLARE_METATYPE(QFuture<QUuid>)
Q_DECLARE_METATYPE(QFuture<QList<QUuid>>)

CAF_PUSH_WARNINGS
#include "qml_viewport.hpp"
#include "caf_system.hpp"
#include <QVBoxLayout>
CAF_POP_WARNINGS

using namespace xstudio;

PySideQmlViewport::PySideQmlViewport(QWidget *parent) : QQuickWidget(parent) {

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

    // Get the session to create a playlist
    auto playlist = utility::request_receive<utility::UuidUuidActor>(
                        *self, session, session::add_playlist_atom_v, "Test")
                        .second.actor();

    // ask the playlist to create a playhead
    caf::actor playhead = utility::request_receive<utility::UuidActor>(
                              *self, playlist, playlist::create_playhead_atom_v)
                              .actor();

    // Register our custom types
    qmlRegisterType<StudioUI>("xstudio.qml.studio", 1, 0, "Studio");
    qmlRegisterType<SemVer>("xstudio.qml.semver", 1, 0, "SemVer");
    qmlRegisterType<QMLViewport>("xstudio.qml.viewport", 1, 0, "Viewport");
    qmlRegisterType<PlaylistUI>("xstudio.qml.playlist", 1, 0, "Playlist");
    qmlRegisterType<MediaUI>("xstudio.qml.media", 1, 0, "Media");
    qmlRegisterType<MediaSourceUI>("xstudio.qml.media_source", 1, 0, "MediaSource");
    qmlRegisterType<MediaStreamUI>("xstudio.qml.media_stream", 1, 0, "MediaStream");
    qmlRegisterType<BookmarksUI>("xstudio.qml.bookmarks", 1, 0, "Bookmarks");
    qmlRegisterType<BookmarkDetailUI>("xstudio.qml.bookmarks", 1, 0, "BookmarkDetail");

    qmlRegisterType<SessionUI>("xstudio.qml.session", 1, 0, "Session");
    qmlRegisterType<ContainerGroupUI>("xstudio.qml.session", 1, 0, "ContainerGroupUI");
    qmlRegisterType<ContainerDividerUI>("xstudio.qml.session", 1, 0, "ContainerDividerUI");
    qmlRegisterType<TimelineUI>("xstudio.qml.timeline", 1, 0, "TimelineUI");
    qmlRegisterType<SubsetUI>("xstudio.qml.subset", 1, 0, "SubsetUI");
    qmlRegisterType<ContactSheetUI>("xstudio.qml.contact_sheet", 1, 0, "ContactSheetUI");
    qmlRegisterType<QMLUuid>("xstudio.qml.uuid", 1, 0, "QMLUuid");
    qmlRegisterType<ClipboardProxy>("xstudio.qml.clipboard", 1, 0, "Clipboard");

    qRegisterMetaType<MediaUI *>("MediaUI*");
    // qRegisterMetaType<BookmarkDetailUI*>("BookmarkDetailUI*");
    qRegisterMetaType<const BookmarkDetailUI *>("const BookmarkDetailUI*");

    QuickFuture::registerType<QUuid>();
    QuickFuture::registerType<QList<QUuid>>();

    // Add a CafSystemObject to the application - this is QObject that simply
    // holds a reference to the actor system so that we can access the system
    // in Qt main loop
    new CafSystemObject(QCoreApplication::instance(), CafSys::instance()->system());
    const QUrl url(QStringLiteral("qrc:/main_viewport_only.qml"));

    engine()->addImageProvider(QLatin1String("thumbnail"), new ThumbnailProvider);
    engine()->rootContext()->setContextProperty(
        "applicationDirPath", QGuiApplication::applicationDirPath());

    auto helper = new Helpers(engine());
    engine()->rootContext()->setContextProperty("helpers", helper);

    engine()->addImportPath("qrc:///");
    engine()->addImportPath("qrc:///extern");

    // gui plugins..
    engine()->addImportPath(QStringFromStd(xstudio_root("/plugin/qml")));
    engine()->addPluginPath(QStringFromStd(xstudio_root("/plugin/qml")));

    setSource(url);
}

void PySideQmlViewport::resizeEvent(QResizeEvent *event) {

    rootObject()->setWidth(event->size().width());
    rootObject()->setHeight(event->size().height());
    QQuickWidget::resizeEvent(event);
}
