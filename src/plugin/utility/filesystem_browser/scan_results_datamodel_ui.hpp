// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QFuture>
#include <QList>
#include <QUuid>
#include <QVariantList>
#include <QQmlExtensionPlugin>
#include <QAbstractItemModel>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"

namespace xstudio::file_system_browser {

class ScanResultsNode;

/*
ScanResultsModel class.

This class is a QtAbstractItemModel that manages the results of the file system
scan performed by the python component. Using a QAbstractItemModel 
gives us the benefit of many under-the-hood Qt optimisations when displaying
data in a QML view.
*/
class ScanResultsModel : public caf::mixin::actor_object<QAbstractItemModel> {

    Q_OBJECT

    Q_PROPERTY(int viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(int numThumbnailCols READ numThumbnailCols WRITE setNumThumbnailCols NOTIFY numThumbnailColsChanged)
    Q_PROPERTY(
        QModelIndex rootIndex READ rootIndex NOTIFY rootIndexChanged)
    Q_PROPERTY(QString searchString READ searchString WRITE setSearchString NOTIFY searchStringChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending WRITE setSortAscending NOTIFY sortAscendingChanged)
    Q_PROPERTY(QString currentFilterTime READ currentFilterTime WRITE setCurrentFilterTime NOTIFY currentFilterTimeChanged)
    Q_PROPERTY(QString currentFilterVersion READ currentFilterVersion WRITE setCurrentFilterVersion NOTIFY currentFilterVersionChanged)
    Q_PROPERTY(QString sortRole READ sortRole WRITE setSortRole NOTIFY sortRoleChanged)
    Q_PROPERTY(int numMediaFiles READ numMediaFiles NOTIFY numMediaFilesChanged)
    Q_PROPERTY(int numRootFolders READ numRootFolders NOTIFY numRootFoldersChanged)

    QML_NAMED_ELEMENT(ScanResultsModel)

  public:

    enum LogRoles { 
        DATE = Qt::UserRole + 1,
        DATE_STRING,
        STEM,
        DEPTH,
        EXTENSION,
        FRAMES,
        FILE_COUNT,
        TOTAL_FILE_COUNT,
        IS_FOLDER,
        IS_LATEST_VERSION,
        IS_SEQUENCE,
        IS_EXPANDED,
        IS_SCANNED,
        IS_VISIBLE,
        IS_EMPTY,
        NAME,
        NAME_AND_COUNT,
        OWNER,
        PATH,
        RELPATH,
        SIZE,
        SIZE_STR,
        THUMBNAIL_FRAME,
        TYPE,
        VERSION,
        VERSION_GROUP,
        VERSION_RANK,
        VERSION_STREAM_KEY
    };

    // Provide a mapping from DATA_MODEL_ROLE to strings. This is needed to define
    // the names of the 'roleData' elements that our QML Model will expose
    inline static const std::map<int, std::string> data_model_roles = {
        {DATE, "date"},
        {DATE_STRING, "date_string"},        
        {STEM, "stem"},
        {EXTENSION, "extension"},
        {FRAMES, "frames"},
        {IS_FOLDER, "is_folder"},
        {IS_LATEST_VERSION, "is_latest_version"},
        {IS_SEQUENCE, "is_sequence"},
        {IS_EXPANDED, "is_expanded"},
        {IS_SCANNED, "is_scanned"},
        {IS_VISIBLE, "is_visible"},
        {IS_EMPTY, "is_empty"},
        {NAME, "name"},
        {NAME_AND_COUNT, "name_and_count"},
        {OWNER, "owner"},
        {PATH, "path"},
        {RELPATH, "relpath"},
        {TOTAL_FILE_COUNT, "total_file_count"},
        {FILE_COUNT, "file_count"},
        {SIZE, "size"},
        {SIZE_STR, "size_str"},
        {THUMBNAIL_FRAME, "thumbnailFrame"},
        {TYPE, "type"},
        {VERSION, "version"},
        {VERSION_GROUP, "version_group"},
        {VERSION_RANK, "version_rank"},
        {VERSION_STREAM_KEY, "version_stream_key"}};

    using super = caf::mixin::actor_object<QAbstractItemModel>;

    ScanResultsModel(QObject *parent = nullptr);
    ~ScanResultsModel() override = default;

    void init(caf::actor_system &system);

    [[nodiscard]] caf::actor_system &system() const {
        return const_cast<caf::actor_companion *>(self())->home_system();
    }

    /* These pure virtual methods from QAbstractItemModel need to be overriden*/
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QModelIndex
    index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override;
    [[nodiscard]] QVariant
    data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] bool canFetchMore(const QModelIndex &parent) const override;

    // When roleData is set in QML, this method is invoked
    [[nodiscard]] bool
    setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    int viewMode() const { return view_mode_; }
    void setViewMode(int mode) {
        if (mode != view_mode_) {
            view_mode_ = mode;
            fullRebuild();
            emit viewModeChanged();
        }
    }

    QString searchString() const { return search_string_; }
    void setSearchString(const QString &s);

    QString sortRole() const { 
        auto p = data_model_roles.find(sort_role_);
        if (p != data_model_roles.end()) {
            return ui::qml::QStringFromStd(p->second);
        }
        return QString();
    }

    void setSortRole(const QString &s) {
        auto p = std::find_if(
            data_model_roles.begin(),
            data_model_roles.end(),
            [&](const auto &p) { return p.second == s.toStdString(); });
        if (p != data_model_roles.end()) {
            if (p->first != sort_role_) {
                sort_role_ = p->first;
                doFiltering();
                emit sortRoleChanged();
            }
        }
    }

    bool sortAscending() const { return sort_ascending_; }
    void setSortAscending(bool ascending) {
        if (ascending != sort_ascending_) {
            sort_ascending_ = ascending;
            fullRebuild();
            emit sortAscendingChanged();
        }
    }

    QString currentFilterTime() const { return current_filter_time_; }
    void setCurrentFilterTime(const QString &s);

    QString currentFilterVersion() const { return current_filter_version_; }
    void setCurrentFilterVersion(const QString &s);

    [[nodiscard]] const QModelIndex &rootIndex() const { return root_index_; }

    int numThumbnailCols() const { return numCols_; }
    void setNumThumbnailCols(int num) {
        if (num != numCols_) {
            numCols_ = num;
            emit numThumbnailColsChanged();
        }
    }

    int numMediaFiles() const { return num_media_files_; }
    int numRootFolders() const { return num_root_folders_; }

    Q_INVOKABLE void set(const QModelIndex &index, const QVariant &value, const QString &role_name);
    Q_INVOKABLE QVariant get(const QModelIndex &index, const QString &role_name) const;
    Q_INVOKABLE QModelIndex indexAtVisibleRow(int row) const;

  signals:

    void viewModeChanged();
    void searchStringChanged();
    void numThumbnailColsChanged();
    void rootIndexChanged();
    void sortAscendingChanged();
    void currentFilterTimeChanged();
    void currentFilterVersionChanged();
    void sortRoleChanged();
    void numMediaFilesChanged();
    void numRootFoldersChanged();

  private:

    void connectToPythonPlugin();
    void doFiltering();
    void fullRebuild();
    void addDirectoryToModel(
        const std::string &directory,
        std::shared_ptr<utility::JsonStore> &scan_dir_results,
        const bool is_root,
        const bool full_rebuild = false
    );
    void setEntryData(QModelIndex &index, const std::string &path, const nlohmann::json &data);
    bool filter_check(nlohmann::json &data, const bool setting_filtered_status = false);
    double timestamp_cutoff() const;
    void schedule_sort_and_filter();

    ScanResultsNode *
    createNode(ScanResultsNode *parent);

    const ScanResultsNode *
    get_entry(int row, int column, const ScanResultsNode *parent_entry) const;

    ScanResultsNode * get_entry(const QModelIndex &index) const;

    std::shared_ptr<ScanResultsNode> root_entry_;
    std::shared_ptr<ScanResultsNode> old_root_;

    std::map<int, std::string> data_model_role_id_to_name_;
    std::map<std::string, int> data_model_role_name_to_id_;
    std::map<std::string, std::shared_ptr<utility::JsonStore>> results_per_directory_;
    std::map<std::string, std::shared_ptr<ScanResultsNode>> path_entries_;
    std::map<quintptr, ScanResultsNode * > node_map_;
    quintptr next_node_id_ = 1;
    quintptr root_node_id_ = 1;
    int view_mode_ = 3;
    int numCols_ = 4;
    QString search_string_;
    std::regex filter_regex_;
    QModelIndex root_index_;
    bool sort_ascending_ = true;
    QString current_filter_time_ = "Any";
    double current_filter_time_seconds_ = 0;
    QString current_filter_version_ = "All Versions";
    int current_filter_version_rank_ = -1;
    utility::time_point sort_filter_time_point_;
    int sort_role_ = ScanResultsModel::NAME;
    int num_media_files_ = 0;
    int num_root_folders_ = 0;
};

class ScanResultsFilterModel : public QSortFilterProxyModel {

    Q_OBJECT

    Q_PROPERTY(
        QModelIndex rootIndex READ rootIndex NOTIFY rootIndexChanged)

  public:

    using super = QSortFilterProxyModel;

    ScanResultsFilterModel(QObject *parent = nullptr);

    [[nodiscard]] const QModelIndex &rootIndex() const { return root_index_; }

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override {
        if (!sourceModel())
            return QHash<int, QByteArray>();
        return sourceModel()->roleNames();
    }

    void setSourceModel(QAbstractItemModel *sourceModel) override {
        super::setSourceModel(sourceModel);
        source_model_ = dynamic_cast<ScanResultsModel *>(sourceModel);
        if (source_model_) {
            /*connect(source_model_, &ScanResultsModel::rootIndexChanged, this, &ScanResultsFilterModel::sourceModelReset);
            root_index_ = mapFromSource(source_model_->index(0, 0));
            emit rootIndexChanged();*/
        }
    }

    Q_INVOKABLE void set(const QModelIndex &index, const QVariant &value, const QString &role_name) {
        if (source_model_) {
            source_model_->set(mapToSource(index), value, role_name);
        }
    }
    Q_INVOKABLE QVariant get(const QModelIndex &index, const QString &role_name) const {
        if (source_model_) {
            return source_model_->get(mapToSource(index), role_name);
        }
        return QVariant();
    }
    Q_INVOKABLE QModelIndex indexAtVisibleRow(int row) const {
        if (source_model_) {
            auto source_index = source_model_->indexAtVisibleRow(row);
            if (source_index.isValid()) {
                return mapFromSource(source_index);
            }
        }
        return QModelIndex();
    }

  public slots:

    void sourceModelReset() {
        /*if (source_model_) {
            root_index_ = mapFromSource(source_model_->index(0, 0));
            emit rootIndexChanged();
            sort(0, Qt::DescendingOrder);
        }*/
    }

  protected:
    [[nodiscard]] bool
    filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

  signals:
    void searchStringChanged();
    void rootIndexChanged();
  private:
    QString search_string_;
    ScanResultsModel *source_model_ = nullptr;
    QModelIndex root_index_;
};

// We require this boiler-plate to register our custom class as a QML
// item:

//![plugin]
class DemoPluginQML : public QQmlExtensionPlugin {
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "xstudio-project.demoplugin.ui")
    void registerTypes(const char *uri) override {
        qmlRegisterType<xstudio::file_system_browser::ScanResultsModel>(
            "filesystembrowser.qml", 1, 0, "FileSystemBrowserScanResultsModel");
        qmlRegisterType<xstudio::file_system_browser::ScanResultsFilterModel>(
            "filesystembrowser.qml", 1, 0, "FileSystemBrowserScanResultsFilterModel");
        }
};
//![plugin]

} // namespace xstudio::demo_plugin
