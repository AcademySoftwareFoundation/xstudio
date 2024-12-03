// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/frame_range.hpp"
#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/media/media.hpp"

using namespace xstudio::ui::qml;

#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <QJSValue>
#include <QMimeData>
#include <QItemSelectionRange>
#include <QVector4D>
#include <QPainter>
#include <QQmlProperty>

CafSystemObject::CafSystemObject(QObject *parent, caf::actor_system &sys)
    : QObject(parent), system_ref_(sys) {
    setObjectName("CafSystemObject");
}

caf::actor_system &CafSystemObject::get_actor_system() {
    if (!QCoreApplication::instance()) {
        throw std::runtime_error(
            "CafSystemObject::get_actor_system() called without QCoreApplication instance.");
    }
    auto obj = QCoreApplication::instance()->findChild<CafSystemObject *>("CafSystemObject");
    if (!obj) {
        throw std::runtime_error(
            "CafSystemObject::get_actor_system() called without CafSystemObject instance.");
    }
    return obj->actor_system();
}


std::string xstudio::ui::qml::actorToString(actor_system &sys, const caf::actor &actor) {
    return xstudio::utility::actor_to_string(sys, actor);
}

caf::actor xstudio::ui::qml::actorFromString(actor_system &sys, const std::string &str_addr) {
    return xstudio::utility::actor_from_string(sys, str_addr);
}


QString xstudio::ui::qml::actorToQString(actor_system &sys, const caf::actor &actor) {
    return QStringFromStd(actorToString(sys, actor));
}

caf::actor xstudio::ui::qml::actorFromQString(actor_system &sys, const QString &addr_str) {
    std::string addr = StdFromQString(addr_str);
    return actorFromString(sys, addr);
}


QString xstudio::ui::qml::getThumbnailURL(
    actor_system &system, const caf::actor &actor, const int frame, const bool cache_to_disk) {
    //  we introduce a random component to allow reaquiring of thumb.
    // this is for the default thumb which maybe generated before the media source is fully
    // loaded.
    QString thumburl("qrc:///feather_icons/film.svg");
    // spdlog::stopwatch sw;

    try {
        binary_serializer::container_type buf;
        binary_serializer bs{system, buf};

        auto e = bs.apply(caf::actor_cast<caf::actor_addr>(actor));
        if (not e)
            throw std::runtime_error(to_string(bs.get_error()));

        // we need to make sure params is part of the url..
        scoped_actor sys{system};
        size_t hash = 0;
        try {
            auto colour_pipe_manager =
                system.registry().get<caf::actor>(colour_pipeline_registry);
            auto colour_pipe = utility::request_receive<caf::actor>(
                *sys,
                colour_pipe_manager,
                colour_pipeline::get_thumbnail_colour_pipeline_atom_v);

            auto mp = utility::request_receive<media::AVFrameID>(
                *sys, actor, media::get_media_pointer_atom_v, media::MT_IMAGE, frame);

            auto mhash = utility::request_receive<std::pair<std::string, uintmax_t>>(
                *sys, actor, media::checksum_atom_v);

            auto display_transform_hash = utility::request_receive<size_t>(
                *sys, colour_pipe, colour_pipeline::display_colour_transform_hash_atom_v, mp);
            hash = std::hash<std::string>{}(static_cast<const std::string &>(
                std::to_string(display_transform_hash) + mhash.first +
                std::to_string(mhash.second)));
        } catch ([[maybe_unused]] const std::exception &err) {
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        auto actor_str = utility::make_hex_string(std::begin(buf), std::end(buf));

        auto thumbstr = std::string(fmt::format(
            "image://thumbnail/{}/{}/{}/{}",
            actor_str,
            frame,
            (cache_to_disk ? "1" : "0"),
            hash));
        thumburl      = QStringFromStd(thumbstr);
    } catch ([[maybe_unused]] const std::exception &err) {
        // spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    // spdlog::warn("getThumbnailURL {:.3} {}", sw, StdFromQString(thumburl));

    return thumburl;
}

nlohmann::json xstudio::ui::qml::qvariant_to_json(const QVariant &var) {

    switch (QMetaType::Type(var.type())) {
    case QMetaType::Bool:
        return nlohmann::json(var.toBool());
        break;
    case QMetaType::Double:
        return nlohmann::json((double)var.toReal());
        break;
    case QMetaType::Float:
        return nlohmann::json(var.toFloat());
        break;
    case QMetaType::Long:
    case QMetaType::LongLong:
    case QMetaType::Short:
    case QMetaType::ULong:
    case QMetaType::ULongLong:
    case QMetaType::UShort:
    case QMetaType::UInt:
    case QMetaType::SChar:
    case QMetaType::UChar:
    case QMetaType::Int:
        return nlohmann::json(var.toInt());
        break;
    case QMetaType::QString:
        return nlohmann::json(var.toString().toStdString());
        break;
    case QMetaType::QUuid:
        return nlohmann::json(UuidFromQUuid(var.toUuid()));
        break;
    case QMetaType::QUrl:
        return nlohmann::json(StdFromQString(var.toUrl().toString()));
        break;
    case QMetaType::QColor: {
        auto c = var.value<QColor>();
        return nlohmann::json(utility::ColourTriplet(
            float(c.red()) / 255.0f, float(c.green()) / 255.0f, float(c.blue()) / 255.0f));
    } break;
    case QMetaType::QByteArray:
        return nlohmann::json::parse(var.toByteArray().toStdString());
        break;
    case QMetaType::QVariantMap: {
        const QVariantMap m = var.toMap();
        auto rt             = R"({})"_json;
        for (auto p = m.begin(); p != m.end(); ++p) {
            rt[p.key().toStdString()] = xstudio::ui::qml::qvariant_to_json(p.value());
        }
        return rt;
    } break;
    case QMetaType::QVariantList: {
        const QVariantList m = var.toList();
        auto rt              = R"([])"_json;
        for (const auto &p : m) {
            rt.push_back(xstudio::ui::qml::qvariant_to_json(p));
        }
        return rt;
    } break;
    default: {
        // No QMetaType for QJSValue
        if (var.canConvert<QJSValue>()) {
            const auto m = var.value<QJSValue>();
            return xstudio::ui::qml::qvariant_to_json(m.toVariant());
        }
        QString err;
        QDebug errStream(&err);
        errStream << "Unable to convert QVariant to json: " << var;
        throw std::runtime_error(err.toStdString().c_str());
    } break;
    }
}

QVariant xstudio::ui::qml::json_to_qvariant(const nlohmann::json &json) {

    if (json.is_number_float()) {
        return QVariant(json.get<float>());
    } else if (json.is_number_integer()) {
        return QVariant(json.get<int>());
    } else if (json.is_string()) {
        return QVariant(json.get<std::string>().c_str());
    } else if (json.is_boolean()) {
        return QVariant(json.get<bool>());
    } else if (json.is_array()) {

        if (json.size() == 5 && json[0].is_string() && json[0].get<std::string>() == "colour") {

            // it should be a color!
            const auto t = json.get<utility::ColourTriplet>();
            QColor c(
                static_cast<int>(round(t.r * 255.0f)),
                static_cast<int>(round(t.g * 255.0f)),
                static_cast<int>(round(t.b * 255.0f)));
            return QVariant(c);

        } else if (
            json.size() == 6 && json[0].is_string() && json[0].get<std::string>() == "vec4") {

            // it should be a color!
            const auto t = json.get<Imath::V4f>();
            QVector4D rt;
            rt[0] = t[0];
            rt[1] = t[1];
            rt[2] = t[2];
            rt[3] = t[3];
            return QVariant(rt);
        }
        QList<QVariant> rt;
        for (auto p = json.begin(); p != json.end(); ++p) {
            rt.append(json_to_qvariant(p.value()));
        }
        return rt;
    } else if (json.is_object()) {
        QMap<QString, QVariant> m;
        for (auto p = json.begin(); p != json.end(); ++p) {
            m[p.key().c_str()] = json_to_qvariant(p.value());
        }
        return QVariant(m);
    } else if (json.is_null()) {
        return QVariant();
    }
    throw std::runtime_error("Unknown json type, no conversion to QVariant");
}


KeyEventsItem::KeyEventsItem(QQuickItem *parent) : QQuickItem(parent) {

    keypress_monitor_ = CafSystemObject::get_actor_system().registry().template get<caf::actor>(
        xstudio::keyboard_events);
    setAcceptHoverEvents(true);
}

bool KeyEventsItem::event(QEvent *event) {

    // Key events are forwarded to keypress_monitor_ - we pass our 'context'
    // property so we have an idea WHERE (i.e. what UI element) the hotkey was
    // pressed

    if (window_name_.empty())
        window_name_ = StdFromQString(item_window_name(parent()));

    /* This is where keyboard events are captured and sent to the backend!! */
    if (event->type() == QEvent::KeyPress) {

        auto key_event = dynamic_cast<QKeyEvent *>(event);
        if (key_event) {
            anon_send(
                keypress_monitor_,
                ui::keypress_monitor::key_down_atom_v,
                key_event->key(),
                context_,
                window_name_,
                key_event->isAutoRepeat());
        }
    } else if (event->type() == QEvent::KeyRelease) {

        auto key_event = dynamic_cast<QKeyEvent *>(event);
        if (key_event && !key_event->isAutoRepeat()) {
            anon_send(
                keypress_monitor_,
                ui::keypress_monitor::key_up_atom_v,
                key_event->key(),
                context_,
                window_name_);
        }
    } else if (
        event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave ||
        event->type() == QEvent::DragLeave || event->type() == QEvent::GraphicsSceneDragLeave ||
        event->type() == QEvent::GraphicsSceneHoverLeave) {
        anon_send(keypress_monitor_, ui::keypress_monitor::all_keys_up_atom_v, context_);
    } else if (event->type() == QEvent::HoverEnter) {
        forceActiveFocus(Qt::MouseFocusReason);
    }
    return QQuickItem::event(event);
}

void KeyEventsItem::keyPressEvent(QKeyEvent *event) {

    if (window_name_.empty())
        window_name_ = StdFromQString(item_window_name(parent()));

    anon_send(
        keypress_monitor_,
        ui::keypress_monitor::text_entry_atom_v,
        StdFromQString(event->text()),
        context_,
        window_name_);
}
void KeyEventsItem::keyReleaseEvent(QKeyEvent *event) {

    if (window_name_.empty())
        window_name_ = StdFromQString(item_window_name(parent()));

    if (!event->isAutoRepeat()) {
        anon_send(
            keypress_monitor_,
            ui::keypress_monitor::key_up_atom_v,
            event->key(),
            context_,
            window_name_);
    }
}

ClipboardProxy::ClipboardProxy(QObject *parent) : QObject(parent) {
    QClipboard *clipboard = QGuiApplication::clipboard();
    connect(clipboard, &QClipboard::dataChanged, this, &ClipboardProxy::dataChanged);
    connect(clipboard, &QClipboard::selectionChanged, this, &ClipboardProxy::selectionChanged);
}

void ClipboardProxy::setDataText(const QString &text) {
    QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
}

QString ClipboardProxy::dataText() const {
    return QGuiApplication::clipboard()->text(QClipboard::Clipboard);
}

QVariant ClipboardProxy::data() const {
    auto md = QGuiApplication::clipboard()->mimeData();
    if (md)
        return QVariant::fromValue(md->data("QVariant"));

    return QVariant();
}

void ClipboardProxy::setData(const QVariant &data) {
    auto mimedata = new QMimeData();
    mimedata->setData("QVariant", data.toByteArray());
    QGuiApplication::clipboard()->setMimeData(mimedata, QClipboard::Clipboard);
}


void ClipboardProxy::setSelectionText(const QString &text) {
    QGuiApplication::clipboard()->setText(text, QClipboard::Selection);
}

QString ClipboardProxy::selectionText() const {
    return QGuiApplication::clipboard()->text(QClipboard::Selection);
}

/*QQuickItem* Helpers::findItemByName(QList<QObject*> nodes, const QString &name) {
    for(auto node : nodes){
        // search for node
        if (node && node->objectName() == name){
             return dynamic_cast<QQuickItem*>(node);
        }
         // search in children
        else if (node && node->children().size() > 0){
            QQuickItem* item = findItemByName(node->children(), name);
            if (item)
                return item;
        }
    }
     // not found
    return nullptr;
}*/

QDateTime Helpers::getFileMTime(const QUrl &url) const {
    auto mtim = utility::get_file_mtime(UriFromQUrl(url));
    return QDateTime::fromMSecsSinceEpoch(
        std::chrono::duration_cast<std::chrono::milliseconds>(mtim.time_since_epoch()).count());
}

QModelIndexList Helpers::getParentIndexesFromRange(const QItemSelection &l) const {
    return getParentIndexes(l.indexes());
}

QModelIndexList Helpers::getParentIndexes(const QModelIndexList &l) const {
    auto result = QModelIndexList();

    for (auto i : l) {
        while (i.isValid()) {
            i = i.parent();
            result.push_front(i);
        }
    }

    return result;
}

#ifdef __linux__
#include <QtDBus/QtDBus>
#endif

void Helpers::inhibitScreenSaver(const bool inhibit) const {
    // quint32 screensaver_cookie_{0};

    spdlog::debug("inhibitScreenSaver {}", inhibit);

#ifdef __linux__
    const int MAX_SERVICES = 1;

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        QString services[MAX_SERVICES] = {
            "org.freedesktop.ScreenSaver",
            // "org.gnome.SessionManager"
        };
        QString paths[MAX_SERVICES] = {
            "/org/freedesktop/ScreenSaver",
            // "/org/gnome/SessionManager"
        };

        static quint32 cookies[2] = {0, 0};

        for (int i = 0; i < MAX_SERVICES; i++) {
            QDBusInterface screenSaverInterface(services[i], paths[i], services[i], bus);

            if (!screenSaverInterface.isValid())
                continue;

            QDBusReply<uint> reply;

            if (inhibit)
                reply = screenSaverInterface.call("Inhibit", "xStudio", "Playing");
            else if (cookies[i]) {
                reply      = screenSaverInterface.call("UnInhibit", cookies[i]);
                cookies[i] = 0;
            }

            if (inhibit) {
                if (reply.isValid())
                    cookies[i] = reply.value();
                else {
                    QDBusError error = reply.error();
                    spdlog::warn(
                        "{} {} {}",
                        __PRETTY_FUNCTION__,
                        StdFromQString(error.message()),
                        StdFromQString(error.name()));
                }
            }
        }
    }

#endif
}


bool Helpers::startDetachedProcess(
    const QString &program,
    const QStringList &arguments,
    const QString &workingDirectory,
    qint64 *pid) const {
    return QProcess::startDetached(program, arguments, workingDirectory, pid);
}


QString Helpers::readFile(const QUrl &url) const {
    QFile data(QStringFromStd(utility::uri_to_posix_path(UriFromQUrl(url))));

    if (data.open(QFile::ReadOnly)) {
        QTextStream out(&data);
        return out.readAll();
    }

    return "";
}

QObject *Helpers::contextPanel(QObject *obj) const {

    // traverse up the qml context hierarchy until we hit an object named
    // XsPanelParent - then we've gone one level beyond the actual panel
    // item that we want
    if (qmlContext(obj)) {
        QQmlContext *c = qmlContext(obj);
        QObject *pobj  = obj;
        while (c) {
            QObject *cobj = c->contextObject();
            if (cobj && cobj->objectName() == "XsPanelParent") {
                return pobj;
            }
            pobj = cobj;
            c    = c->parentContext();
        }
    }
    return nullptr;
}

void Helpers::setMenuPathPosition(
    const QString &menu_path, const QString &menu_name, const float position) const {

    if (!menu_path.isEmpty() && !menu_name.isEmpty()) {

        auto central_models_data_actor =
            CafSystemObject::get_actor_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        anon_send(
            central_models_data_actor,
            ui::model_data::insert_or_update_menu_node_atom_v,
            StdFromQString(menu_name),
            StdFromQString(menu_path),
            position);
    }
}

ImagePainter::ImagePainter(QQuickItem *parent) : QQuickPaintedItem(parent) {}

void ImagePainter::paint(QPainter *painter) {

    if (image_.isNull())
        return;

    const float image_aspect  = float(image_.width()) / float(image_.height());
    const float canvas_aspect = float(width()) / float(height());

    if (!fill_) {
        if (image_aspect > canvas_aspect) {
            float bottom =
                float(height()) * 0.5f * (image_aspect - canvas_aspect) / image_aspect;
            painter->drawImage(
                QRectF(0.0f, bottom, width(), height() - (bottom * 2.0f)), image_);
        } else {
            float left = float(width()) * 0.5f * (canvas_aspect - image_aspect) / canvas_aspect;
            painter->drawImage(QRectF(left, 0.0f, width() - (left * 2.0f), height()), image_);
        }
    } else {
        painter->drawImage(QRectF(0.0f, -5.0f, width(), height() + 5.0f), image_);
    }
}

MarkerModel::MarkerModel(QObject *parent) : JSONTreeModel(parent) {
    auto role_names = std::vector<std::string>(
        {{"commentRole"},
         {"durationRole"},
         {"flagRole"},
         {"layerRole"},
         {"nameRole"},
         {"rateRole"},
         {"startRole"}});

    setRoleNames(role_names);

    connect(this, &MarkerModel::rowsInserted, this, &MarkerModel::markerDataChanged);
    connect(this, &MarkerModel::rowsRemoved, this, &MarkerModel::markerDataChanged);
}

bool MarkerModel::setMarkerData(const QVariant &qdata) {
    auto result = true;
    try {
        auto data = mapFromValue(qdata);
        if (modelData().is_null() or data != modelData().at("children")) {
            setModelData(data);
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }
    return result;
}

QVariant MarkerModel::markerData() const {
    // auto data = R"([])"_json;
    // try {
    //     data = modelData().at("children");
    // } catch(...) {}

    return QVariantFromJson(modelData());
}

QVariant MarkerModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();
    try {
        const auto &j = indexToData(index);
        switch (role) {
        case JSONTreeModel::Roles::idRole:
            result = QVariant::fromValue(QUuidFromUuid(j.value("uuid", utility::Uuid())));
            break;
        case Roles::flagRole:
            result = QString::fromStdString(j.value("flag", std::string("")));
            break;
        case Roles::nameRole:
            result = QString::fromStdString(j.value("name", std::string("")));
            break;
        case Roles::commentRole:
            result = QVariantFromJson(j.value("prop", R"({})"_json));
            break;
        case Roles::startRole:
            result = QVariant::fromValue(j.at("range").value("start", 0l));
            break;
        case Roles::durationRole:
            result = QVariant::fromValue(j.at("range").value("duration", 0l));
            break;
        case Roles::rateRole:
            result = QVariant::fromValue(j.at("range").value("rate", 0l));
            break;
        case Roles::layerRole: {
            auto layer = 0;

            for (auto i = 0; i < index.row(); i++) {
                auto previ = MarkerModel::index(i, 0, index.parent());
                if (previ.data(startRole) == index.data(startRole))
                    layer++;
            }
            result = layer;
        } break;
        default:
            result = JSONTreeModel::data(index, role);
            break;
        }
    } catch (const std::exception &err) {
        // spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool MarkerModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    QVector<int> roles({role});
    auto result = false;

    try {
        auto &j = indexToData(index);
        switch (role) {

        case Roles::nameRole: {
            auto data = mapFromValue(value);
            if (j["name"] != data) {
                j["name"] = data;
                result    = true;
                emit dataChanged(index, index, roles);
                emit markerDataChanged();
            }
        } break;

        case Roles::commentRole: {
            auto data = mapFromValue(value);
            if (j["prop"] != data) {
                j["prop"] = data;
                result    = true;
                emit dataChanged(index, index, roles);
                emit markerDataChanged();
            }
        } break;

        case Roles::flagRole: {
            auto data = mapFromValue(value);
            if (j["flag"] != data) {
                j["flag"] = data;
                result    = true;
                emit dataChanged(index, index, roles);
                emit markerDataChanged();
            }
        } break;

        default:
            result = JSONTreeModel::setData(index, value, role);
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn(
            "{} {} {} {}",
            __PRETTY_FUNCTION__,
            err.what(),
            role,
            StdFromQString(value.toString()));
    }

    return result;
}

QModelIndex MarkerModel::addMarker(
    const int frame,
    const double rate,
    const QString &name,
    const QString &flag,
    const QVariant &metadata) {
    auto result = QModelIndex();
    auto marker =
        R"({"name": null, "prop": null,"range": null,"flag": null, "uuid":null})"_json;

    marker["uuid"]  = utility::Uuid::generate();
    marker["name"]  = StdFromQString(name);
    marker["flag"]  = StdFromQString(flag);
    marker["prop"]  = mapFromValue(metadata);
    marker["range"] = utility::FrameRange(
        utility::FrameRate(frame * timebase::to_flicks(1.0 / rate)),
        utility::FrameRate(),
        utility::FrameRate(1.0 / rate));

    auto row = rowCount();
    if (insertRows(row, 1, QModelIndex(), marker)) {
        result = index(row, 0, QModelIndex());
    }

    return result;
}

void PropertyFollower::setTarget(QObject *target) {

    if (target_ != target) {
        target_ = target;
        emit targetChanged();
        if (target_) {
            the_property_ = QQmlProperty(target_, property_name_);
            the_property_.connectNotifySignal(this, SIGNAL(propertyValueChanged()));
        }
    }
}

void PropertyFollower::setPropertyName(const QString propertyName) {

    if (property_name_ != propertyName) {
        property_name_ = propertyName;
        emit propertyNameChanged();
        if (target_) {
            the_property_ = QQmlProperty(target_, property_name_);
            the_property_.connectNotifySignal(this, SIGNAL(propertyValueChanged()));
        }
    }
}
