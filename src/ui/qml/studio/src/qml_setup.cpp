#include "xstudio/ui/mouse.hpp"
#include "xstudio/ui/qml/bookmark_model_ui.hpp"      //NOLINT
#include "xstudio/ui/qml/conform_ui.hpp"             //NOLINT
#include "xstudio/ui/qml/embedded_python_ui.hpp"     //NOLINT
#include "xstudio/ui/qml/QTreeModelToTableModel.hpp" //NOLINT
#include "xstudio/ui/qml/global_store_model_ui.hpp"  //NOLINT
#include "xstudio/ui/qml/helper_ui.hpp"              //NOLINT
#include "xstudio/ui/qml/hotkey_ui.hpp"              //NOLINT
#include "xstudio/ui/qml/log_ui.hpp"                 //NOLINT
#include "xstudio/ui/qml/model_data_ui.hpp"          //NOLINT
#include "xstudio/ui/qml/module_data_ui.hpp"         //NOLINT
#include "xstudio/ui/qml/qml_viewport.hpp"           //NOLINT
#include "xstudio/ui/qml/session_model_ui.hpp"       //NOLINT
#include "xstudio/ui/qml/shotgun_provider_ui.hpp"
#include "xstudio/ui/qml/studio_ui.hpp" //NOLINT
#include "xstudio/ui/qml/thumbnail_provider_ui.hpp"

#include <QApplication>
#include <QFontDatabase>

void xstudio::ui::qml::setup_xstudio_qml_emgine(QQmlEngine *engine, caf::actor_system &system) {

    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    engine->rootContext()->setContextProperty("systemFixedWidthFontFamily", fixedFont.family());

    qmlRegisterType<SemVer>("xstudio.qml.semver", 1, 0, "SemVer");
    qmlRegisterType<HotkeyUI>("xstudio.qml.viewport", 1, 0, "XsHotkey");
    qmlRegisterType<HotkeysUI>("xstudio.qml.viewport", 1, 0, "XsHotkeysInfo");
    qmlRegisterType<HotkeyReferenceUI>("xstudio.qml.viewport", 1, 0, "XsHotkeyReference");
    qmlRegisterType<PressedKeysMonitor>("xstudio.qml.viewport", 1, 0, "XsPressedKeysMonitor");

    qmlRegisterType<KeyEventsItem>("xstudio.qml.helpers", 1, 0, "XsHotkeyArea");

    qmlRegisterType<QMLViewport>("xstudio.qml.viewport", 1, 0, "Viewport");

    qmlRegisterType<BookmarkCategoryModel>(
        "xstudio.qml.bookmarks", 1, 0, "XsBookmarkCategories");
    qmlRegisterType<BookmarkModel>("xstudio.qml.bookmarks", 1, 0, "XsBookmarkModel");
    qmlRegisterType<BookmarkFilterModel>(
        "xstudio.qml.bookmarks", 1, 0, "XsBookmarkFilterModel");

    qmlRegisterType<EmbeddedPythonUI>("xstudio.qml.embedded_python", 1, 0, "EmbeddedPython");

    qmlRegisterType<QMLUuid>("xstudio.qml.uuid", 1, 0, "QMLUuid");
    qmlRegisterType<ClipboardProxy>("xstudio.qml.clipboard", 1, 0, "Clipboard");
    qmlRegisterType<ImagePainter>("xstudio.qml.helpers", 1, 0, "XsImagePainter");
    qmlRegisterType<MarkerModel>("xstudio.qml.helpers", 1, 0, "XsMarkerModel");
    qmlRegisterType<PropertyFollower>("xstudio.qml.helpers", 1, 0, "XsPropertyFollower");

    qmlRegisterType<GlobalStoreModel>(
        "xstudio.qml.global_store_model", 1, 0, "XsGlobalStoreModel");
    qmlRegisterType<PublicPreferencesModel>(
        "xstudio.qml.global_store_model", 1, 0, "XsPreferencesModel");
    qmlRegisterType<ModelProperty>("xstudio.qml.helpers", 1, 0, "XsModelProperty");
    qmlRegisterType<JSONTreeFilterModel>("xstudio.qml.helpers", 1, 0, "XsFilterModel");
    qmlRegisterType<TimeCode>("xstudio.qml.helpers", 1, 0, "XsTimeCode");
    qmlRegisterType<ModelRowCount>("xstudio.qml.helpers", 1, 0, "XsModelRowCount");
    qmlRegisterType<ModelPropertyMap>("xstudio.qml.helpers", 1, 0, "XsModelPropertyMap");
    qmlRegisterType<PreferencePropertyMap>("xstudio.qml.helpers", 1, 0, "XsPreferenceMap");
    qmlRegisterType<ModelNestedPropertyMap>(
        "xstudio.qml.helpers", 1, 0, "XsModelNestedPropertyMap");
    qmlRegisterType<ModelPropertyTree>("xstudio.qml.helpers", 1, 0, "XsModelPropertyTree");
    qmlRegisterType<QTreeModelToTableModel>(
        "xstudio.qml.helpers", 1, 0, "QTreeModelToTableModel");


    qmlRegisterType<ConformEngineUI>("xstudio.qml.conform", 1, 0, "XsConformEngine");

    qmlRegisterType<SessionModel>("xstudio.qml.session", 1, 0, "XsSessionModel");

    qmlRegisterType<MenusModelData>("xstudio.qml.models", 1, 0, "XsMenusModel");
    qmlRegisterType<ModulesModelData>("xstudio.qml.models", 1, 0, "XsModuleData");
    qmlRegisterType<PanelsModel>("xstudio.qml.models", 1, 0, "XsPanelsLayoutModel");
    qmlRegisterType<MediaListColumnsModel>(
        "xstudio.qml.models", 1, 0, "XsMediaListColumnsModel");
    qmlRegisterType<MediaListFilterModel>("xstudio.qml.models", 1, 0, "XsMediaListFilterModel");

    qmlRegisterType<ViewsModelData>("xstudio.qml.models", 1, 0, "XsViewsModel");
    qmlRegisterType<PopoutWindowsData>("xstudio.qml.models", 1, 0, "XsPopoutWindowsData");
    qmlRegisterType<SingletonsModelData>("xstudio.qml.models", 1, 0, "XsSingletonItemsModel");

    qmlRegisterType<MenuModelItem>("xstudio.qml.models", 1, 0, "XsMenuModelItem");
    qmlRegisterType<PanelMenuModelFilter>("xstudio.qml.models", 1, 0, "XsPanelMenuModelFilter");

    qRegisterMetaType<QQmlPropertyMap *>("QQmlPropertyMap*");

    // QuickFuture::registerType<QList<QPersistentModelIndex>>();

    // Add a CafSystemObject to the application - this is QObject that simply
    // holds a reference to the actor system so that we can access the system
    // in Qt main loop
    new CafSystemObject(qApp, system);

    const QUrl url(QStringLiteral("qrc:/main.qml"));

    ;
    engine->addImageProvider(QLatin1String("thumbnail"), new ThumbnailProvider);
    engine->addImageProvider(QLatin1String("shotgun"), new ShotgunProvider);
    engine->rootContext()->setContextProperty(
        "applicationDirPath", QGuiApplication::applicationDirPath());

    auto helper = new Helpers(engine);
    engine->rootContext()->setContextProperty("helpers", helper);

    auto studio = new StudioUI(system, qApp);
    engine->rootContext()->setContextProperty("studio", studio);

    auto logger      = new LogModel(engine);
    auto proxylogger = new LogFilterModel(engine);
    proxylogger->setSourceModel(logger);

    engine->rootContext()->setContextProperty("CurrentDirPath", QString(QDir::currentPath()));
    engine->rootContext()->setContextProperty("logger", proxylogger);
    // connect logger.
    auto logsink = std::make_shared<spdlog::sinks::qtlog_sink_mt>(logger);
    spdlog::get("xstudio")->sinks().push_back(logsink);

    engine->addImportPath("qrc:///");
    engine->addImportPath("qrc:///extern");

    // gui plugins..
    engine->addImportPath(QStringFromStd(xstudio_plugin_dir("/qml")));
    engine->addPluginPath(QStringFromStd(xstudio_plugin_dir("")));
    engine->addPluginPath(QStringFromStd(xstudio_plugin_dir("/qml")));

    // env var XSTUDIO_PLUGIN_PATH is search path for plugins. Add
    // subfolders named qml for qt to look for .qml files installed
    // with plugins
    char *plugin_path = std::getenv("XSTUDIO_PLUGIN_PATH");
    if (plugin_path) {
        for (const auto &p : xstudio::utility::split(plugin_path, ':')) {
            engine->addPluginPath(QStringFromStd(p + "/qml"));
            engine->addImportPath(QStringFromStd(p + "/qml"));
        }
    }
}