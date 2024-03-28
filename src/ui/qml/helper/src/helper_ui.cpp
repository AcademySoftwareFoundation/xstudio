// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/media/media.hpp"

using namespace xstudio::ui::qml;

#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <QJSValue>
#include <QMimeData>
#include <QItemSelectionRange>

QMLActor::QMLActor(QObject *parent) : super(parent) {}

QMLActor::~QMLActor() {}

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
    return actorFromString(sys, StdFromQString(addr_str));
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

            auto display_transform_hash = utility::request_receive<std::string>(
                *sys, colour_pipe, colour_pipeline::display_colour_transform_hash_atom_v, mp);
            hash = std::hash<std::string>{}(static_cast<const std::string &>(
                display_transform_hash + mhash.first + std::to_string(mhash.second)));
        } catch (const std::exception &err) {
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
    } catch (const std::exception &err) {
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
        nlohmann::json rt;
        for (auto p = m.begin(); p != m.end(); ++p) {
            rt[p.key().toStdString()] = xstudio::ui::qml::qvariant_to_json(p.value());
        }
        return rt;
    } break;
    case QMetaType::QVariantList: {
        const QVariantList m = var.toList();
        nlohmann::json rt;
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
    } else if (json.is_object()) {
        QMap<QString, QVariant> m;
        for (auto p = json.begin(); p != json.end(); ++p) {
            m[p.key().c_str()] = json_to_qvariant(p.value());
        }
        return QVariant(m);
    } else if (json.is_array()) {

        if (json.size() == 5 && json[0].is_string() && json[0].get<std::string>() == "colour") {

            // it should be a color!
            const auto t = json.get<utility::ColourTriplet>();
            QColor c(
                static_cast<int>(round(t.r * 255.0f)),
                static_cast<int>(round(t.g * 255.0f)),
                static_cast<int>(round(t.b * 255.0f)));
            return QVariant(c);
        }
        QList<QVariant> rt;
        for (auto p = json.begin(); p != json.end(); ++p) {
            rt.append(json_to_qvariant(p.value()));
        }
        return rt;
    } else if (json.is_null()) {
        return QVariant();
    }
    throw std::runtime_error("Unknown json type, no conversion to QVariant");
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
