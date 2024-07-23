// SPDX-License-Identifier: Apache-2.0

// #include "xstudio/ui/qml/job_control_ui.hpp"
#include "xstudio/ui/qml/snapshot_model_ui.hpp"
// #include "xstudio/ui/qml/caf_response_ui.hpp"

CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
// #include <QSignalSpy>
CAF_POP_WARNINGS

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

SnapshotModel::SnapshotModel(QObject *parent) : JSONTreeModel(parent) {

    setRoleNames(std::vector<std::string>({
        {"childrenRole"},
        {"mtimeRole"},
        {"nameRole"},
        {"pathRole"},
        {"typeRole"},
    }));

    try {
        items_.bind_ignore_entry_func(ignore_not_session);
        setModelData(items_.dump());
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

Q_INVOKABLE void SnapshotModel::rescan(const QModelIndex &index, const int depth) {
    auto changed         = false;
    FileSystemItem *item = nullptr;

    if (index.isValid()) {
        auto path = UriFromQUrl(index.data(pathRole).toUrl());
        item      = items_.find_by_path(path);
    } else {
        item = &items_;
    }

    if (item) {
        changed = item->scan(depth);

        if (changed) {
            auto jsn = item->dump();
            sortByName(jsn);
            if (index.isValid())
                setData(index, QVariantMapFromJson(jsn), JSONRole);
            else
                setModelData(jsn);
        }
    }
}


void SnapshotModel::setPaths(const QVariant &value) {
    try {
        if (not value.isNull()) {
            paths_ = value;
            emit pathsChanged();

            auto jsn = mapFromValue(paths_);
            if (jsn.is_array()) {
                items_.clear();

                for (const auto &i : jsn) {
                    if (i.count("name") and i.count("path")) {
                        auto uri = caf::make_uri(i.at("path"));
                        if (uri)
                            items_.insert(items_.end(), FileSystemItem(i.at("name"), *uri));
                    }
                }
                rescan(QModelIndex(), 1);
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

nlohmann::json SnapshotModel::sortByNameType(const nlohmann::json &jsn) const {
    auto result = jsn;

    if (result.is_array()) {
        std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) -> bool {
            try {
                if (a.at("type") == b.at("type"))
                    return a.at("name") < b.at("name");
                return a.at("type") < b.at("type");
            } catch (const std::exception &err) {
                spdlog::warn("{}", err.what());
            }
            return false;
        });
    }

    return result;
}

void SnapshotModel::sortByName(nlohmann::json &jsn) {
    // this needs
    if (jsn.is_object() and jsn.count("children") and jsn.at("children").is_array()) {
        jsn["children"] = sortByNameType(jsn["children"]);

        for (auto &item : jsn["children"]) {
            sortByName(item);
        }
    }
}


QVariant SnapshotModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        if (index.isValid()) {
            const auto &j = indexToData(index);

            switch (role) {
            case Roles::typeRole:
                if (j.count("type_name"))
                    result = QString::fromStdString(j.at("type_name"));
                break;

            case Roles::pathRole:
                if (j.count("path")) {
                    auto uri = caf::make_uri(j.at("path"));
                    if (uri)
                        result = QVariant::fromValue(QUrlFromUri(*uri));
                }
                break;


            case Roles::mtimeRole:
                if (j.count("last_write") and not j.at("last_write").is_null()) {
                    result = QVariant::fromValue(QDateTime::fromMSecsSinceEpoch(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            j.at("last_write").get<fs::file_time_type>().time_since_epoch())
                            .count()));
                }
                break;

            case Qt::DisplayRole:
            case Roles::nameRole:
                if (j.count("name")) {
                    result = QString::fromStdString(j.at("name"));
                }
                break;
            case Roles::childrenRole:
                if (j.count("children")) {
                    result = QVariantMapFromJson(j.at("children"));
                }
                break;
            default:
                result = JSONTreeModel::data(index, role);
                break;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn(
            "{} {} {} {} {}",
            __PRETTY_FUNCTION__,
            err.what(),
            role,
            StdFromQString(roleName(role)),
            index.row());
    }

    return result;
}

bool SnapshotModel::createFolder(const QModelIndex &index, const QString &name) {
    auto result = false;

    // get path..
    if (index.isValid()) {
        auto uri = caf::make_uri(StdFromQString(index.data(pathRole).toString()));
        if (uri) {
            auto path     = uri_to_posix_path(*uri);
            auto new_path = fs::path(path) / StdFromQString(name);
            try {
                fs::create_directory(new_path);
                rescan(index);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        }
    }

    return result;
}

QUrl SnapshotModel::buildSavePath(const QModelIndex &index, const QString &name) const {
    // get path..
    auto result = QUrl();

    if (index.isValid()) {
        auto uri = caf::make_uri(StdFromQString(index.data(pathRole).toString()));
        if (uri) {
            auto path     = uri_to_posix_path(*uri);
            auto new_path = fs::path(path) / std::string(StdFromQString(name) + ".xsz");
            result        = QUrlFromUri(posix_path_to_uri(new_path.string()));
        }
    }

    return result;
}
