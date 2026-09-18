// SPDX-License-Identifier: Apache-2.0
#include <caf/actor_registry.hpp>
#include <regex>

#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"
#include "scan_results_datamodel_ui.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"

CAF_PUSH_WARNINGS
#include <QQmlExtensionPlugin>
CAF_POP_WARNINGS

using namespace xstudio;
using namespace xstudio::file_system_browser;
using namespace xstudio::ui::qml;
using namespace xstudio::utility;

class ScanResultsMiddleman : public caf::event_based_actor {
  public:

    inline static const utility::Uuid PLUGIN_UUID = utility::Uuid("dad9cfc6-6275-408b-8c96-eee5e1f3a47d");

    ScanResultsMiddleman(caf::actor_config &cfg, const utility::JsonStore &) : caf::event_based_actor(cfg) {

        system().registry().put("ScanResultsMiddleman", caf::actor_cast<caf::actor>(this));

    }

    ~ScanResultsMiddleman() override = default;

    void on_exit() override {
        clients_.clear();
        system().registry().erase("ScanResultsMiddleman");
        caf::event_based_actor::on_exit();
    }

    caf::behavior make_behavior() override {
        return {
            [=](broadcast::join_broadcast_atom atom, caf::actor actor) {

                monitor(actor, [this, addr = actor.address()](const error &err) {
                    auto p = clients_.begin();
                    while (p != clients_.end()) {
                        if (addr == (*p).address()) {
                            p = clients_.erase(p);
                        } else {
                            p++;
                        }
                    }
                });
                clients_.push_back(actor);
                bool root = true;
                for(auto &entry : last_scan_results_) {
                    mail(utility::event_atom_v, entry.first, root, entry.second).send(actor);
                    root = false;
                }

            },
            [=](utility::event_atom, const bool new_scan) {
                for (auto &client : clients_) {
                    mail(utility::event_atom_v, new_scan).send(client);
                }
                last_scan_results_.clear();
            },
            [=](utility::event_atom, const std::string &directory, const bool is_root, const std::string &__scan_dir_results) {
                last_scan_results_[directory] = __scan_dir_results;
                for (auto &client : clients_) {
                    mail(utility::event_atom_v, directory, is_root, __scan_dir_results).send(client);
                }
            },
            [=](caf::message &m) {
                spdlog::warn(
                    "{} : unrecognised message received. Message content: {}",
                    __PRETTY_FUNCTION__,
                    to_string(m));
            }};
    }

    std::vector<caf::actor> clients_;
    std::map<std::string, std::string> last_scan_results_;
};

namespace xstudio::file_system_browser {
    // QAbstractItemModel requires us to back the model with our own
    // internal data. In this example we implement a simple tree structure
    // (with no columns) that maps exactly to the structure of the QAbstractItemModel
    struct ScanResultsNode {

        ScanResultsNode(ScanResultsNode *p, const quintptr id);
        ~ScanResultsNode();

        [[nodiscard]] int row() const { return row_; }
        [[nodiscard]] int num_rows() const { return static_cast<int>(visible_children.size()); }
        [[nodiscard]] bool visible() const { return data.at(ScanResultsModel::IS_VISIBLE).toBool(); }
        [[nodiscard]] int column() const { return 0; }
        [[nodiscard]] int total_visible_rows() const { return total_visible_rows_; }
        [[nodiscard]] int total_file_count() const { return total_file_count_; }
        [[nodiscard]] bool is_expanded() const { return data.at(ScanResultsModel::IS_EXPANDED).toBool(); }
        [[nodiscard]] bool is_folder() const { return data.at(ScanResultsModel::IS_FOLDER).toBool(); }
        [[nodiscard]] bool is_scanned() const { return data.at(ScanResultsModel::IS_SCANNED).toBool(); }
        [[nodiscard]] bool is_deep_scanned() const { return deep_scanned_; }
        [[nodiscard]] ScanResultsNode * entry_at_visible_row(int local_row) const;
        [[nodiscard]] ScanResultsNode * nth_visible_child(int n) const;

        bool filter_and_sort(const QString &filter_string, const double not_older_than, const int current_filter_version_rank, const int sort_role, const bool sort_ascending);
        bool sort(const int sort_role, const bool ascending, const bool recurse = true);
        void set_expanded(bool expanded);
        void add_rows(std::vector<std::shared_ptr<ScanResultsNode>> &nc);
        void add_row(std::shared_ptr<ScanResultsNode> nc);
        void update_total_visible_rows(const bool auto_update_parent = true);
        void set_data(const nlohmann::json &data);
        void set_row(int row) { row_ = row; }

        ScanResultsNode *parent = {nullptr};
        quintptr node_id = 0;
        // The actual data (model values) at this node in the tree
        std::map<int, QVariant> data = {
            {ScanResultsModel::NAME, QString()},
            {ScanResultsModel::NAME_AND_COUNT, QString()},
            {ScanResultsModel::IS_EXPANDED, false},
            {ScanResultsModel::IS_VISIBLE, true},
            {ScanResultsModel::IS_FOLDER, false},
            {ScanResultsModel::IS_SCANNED, false}};

        std::vector<std::shared_ptr<ScanResultsNode>> children;
        std::vector<std::shared_ptr<ScanResultsNode>> visible_children;
        std::vector<int> child_local_row_starts;

        std::string path;
        int total_visible_rows_ = 1;
        int row_ = 0;
        int total_file_count_ = 0;
        int visible_file_count_ = 0;
        bool deep_scanned_ = false;

    };
}

ScanResultsNode::ScanResultsNode(ScanResultsNode *p, const quintptr id) : parent(p), node_id(id) {
    if (!p) {
        // this is the root entry - always expanded
        data[ScanResultsModel::IS_EXPANDED] = true;
    }
}

ScanResultsNode::~ScanResultsNode() {
}

bool ScanResultsNode::sort(const int sort_role, const bool ascending, const bool recurse) {
    if (children.empty()) return false;
    bool changed = false;

    auto sort_func1 = [](const std::shared_ptr<ScanResultsNode> &a, const std::shared_ptr<ScanResultsNode> &b, const int sort_role) -> int {

        const auto &a_p = a->data.find(sort_role);
        const auto &b_p = b->data.find(sort_role);
        if (a_p != a->data.end() && b_p == b->data.end()) return -1;
        else if (a_p == a->data.end() && b_p != b->data.end()) {
            return 1;
        }
        else if (a_p == a->data.end() && b_p == b->data.end()) return 0;
        else {
            const auto r = QVariant::compare(a_p->second, b_p->second);
            return r == QPartialOrdering::Less ? -1 : (r == QPartialOrdering::Greater ? 1 : 0);
        }
        return 0;

    };

    std::sort(children.begin(), children.end(), [sort_role, ascending, &changed, sort_func1](const std::shared_ptr<ScanResultsNode> &a, const std::shared_ptr<ScanResultsNode> &b) {

        auto r = sort_func1(a, b, ScanResultsModel::IS_FOLDER);
        if (r == 0) r = sort_func1(a, b, sort_role);
        if (r == 0) r = sort_func1(a, b, ScanResultsModel::NAME);
        return ascending ? r < 0 : r > 0;

    });
    if (recurse) {
        for (auto &child : children) {
            changed |= child->sort(sort_role, ascending);
        }
    }
    visible_children.clear();
    return changed;
}

bool ScanResultsNode::filter_and_sort(
    const QString &filter_string,
    const double not_older_than,
    const int current_filter_version_rank,
    const int sort_role,
    const bool sort_ascending
    ) {

    auto p = data.find(ScanResultsModel::DATE);
    bool v = true;
    if (p != data.end() && p->second.canConvert<double>()) {
        v = p->second.toDouble() >= not_older_than;
    }

    if (v && !filter_string.isEmpty()) {
        p = data.find(ScanResultsModel::NAME);
        if (p != data.end() && p->second.canConvert<QString>()) {
            v = p->second.toString().contains(filter_string, Qt::CaseInsensitive);
        }
    }

    std::set<int> hidden_version_children;
    if (current_filter_version_rank >= 0) {
        // first, sort by the version key. That way adjacent versions will be next to each other, and we can filter out all but the latest version etc.
        sort(ScanResultsModel::VERSION_STREAM_KEY, false, false);
        QString last_version_stream_key;
        int ct = current_filter_version_rank;
        int ii = -1;
        for (auto &child : children) {
            ii++;
            if (child->is_folder()) continue;
            auto p = child->data.find(ScanResultsModel::VERSION_STREAM_KEY);
            if (p != child->data.end() && p->second.canConvert<QString>()) {
                const auto vrank = p->second.toString();
                if (vrank == last_version_stream_key ) {
                    if (ct) ct--;
                    else {
                        child->data[ScanResultsModel::IS_VISIBLE] = false;
                        hidden_version_children.insert(ii);
                    }
                } else {
                    last_version_stream_key = vrank;
                    ct = current_filter_version_rank;
                }
            }
        }
    }

    int ii = 0;
    for (const auto &child : children) {
        if (hidden_version_children.find(ii) == hidden_version_children.end()) {
            child->filter_and_sort(filter_string, not_older_than, current_filter_version_rank, sort_role, sort_ascending);
        }
        ii++;
    }

    // now sort by the requested sort role and order
    sort(sort_role, sort_ascending, false);

    visible_children.clear();
    int row = 0;
    total_file_count_ = 0;
    visible_file_count_ = 0;
    deep_scanned_ = is_scanned();
    for (const auto &child : children) {
        if (child->visible()) {
            visible_children.push_back(child);
            child->set_row(row++);
            if (!child->is_folder()) {
                visible_file_count_++;
            }
        } else {
            child->set_row(-1);
        }
        if (!child->is_folder()) {
            total_file_count_++;            
        } else {
            total_file_count_ += child->total_file_count();
            deep_scanned_ &= child->is_deep_scanned();
        }

    }    
    v |= !visible_children.empty();

    if (is_folder() && is_scanned() && visible_children.empty()) {
        // hide folders that have been scanned but have no visible children
        data[ScanResultsModel::IS_EMPTY] = true;
    }

    data[ScanResultsModel::IS_VISIBLE] = v;
    data[ScanResultsModel::TOTAL_FILE_COUNT] = total_file_count_;
    data[ScanResultsModel::FILE_COUNT] = visible_file_count_;
    if (is_folder()) {
        if (total_file_count_)
            data[ScanResultsModel::NAME_AND_COUNT] = QString("%1 (%2)").arg(data[ScanResultsModel::NAME].toString()).arg(total_file_count_);
        else if (!deep_scanned_)
            //data[ScanResultsModel::NAME_AND_COUNT] = QString("%1 (not scanned)").arg(data[ScanResultsModel::NAME].toString());
            data[ScanResultsModel::NAME_AND_COUNT] = data[ScanResultsModel::NAME];
        else
            data[ScanResultsModel::NAME_AND_COUNT] = QString("%1 (no media)").arg(data[ScanResultsModel::NAME].toString());

    } else {
        data[ScanResultsModel::NAME_AND_COUNT] = data[ScanResultsModel::NAME];
    }
    update_total_visible_rows(false);
    return v;
}

void ScanResultsNode::add_rows(std::vector<std::shared_ptr<ScanResultsNode>> &nc) {
    children.insert(children.end(), nc.begin(), nc.end());
    visible_children.clear();
    int row = 0;
    for (const auto &child : children) {

        if (child->visible()) {
            visible_children.push_back(child);
            child->set_row(row++);
        } else {
            child->set_row(-1);
        }
    } 
    update_total_visible_rows();
}

void ScanResultsNode::add_row(std::shared_ptr<ScanResultsNode> nc) {
    children.push_back(nc);
    if (nc->visible()) {
        nc->set_row(static_cast<int>(visible_children.size()));
        visible_children.push_back(nc);
    } else {
        nc->set_row(-1);
    }
    update_total_visible_rows();
}

void ScanResultsNode::set_expanded(bool expanded) {
    if (is_expanded() != expanded) {
        data[ScanResultsModel::IS_EXPANDED] = expanded;
        update_total_visible_rows();
    }
}

void ScanResultsNode::update_total_visible_rows(const bool auto_update_parent) {
    const int bf = total_visible_rows_;
    if (!visible()) {
        total_visible_rows_ = 0;
    } else {
        total_visible_rows_ = parent ? 1 : 0;
        if (is_expanded()) {
            child_local_row_starts.clear();
            for (const auto &child : visible_children) {
                child_local_row_starts.push_back(total_visible_rows_);
                total_visible_rows_ += child->total_visible_rows();
            }
        }
    }
    if (total_visible_rows_ != bf && parent && auto_update_parent) {
        parent->update_total_visible_rows();
    }
}

ScanResultsNode * ScanResultsNode::entry_at_visible_row(int local_row) const {
    if (parent && local_row == 0) return const_cast<ScanResultsNode *>(this);
    std::vector<int>::const_iterator it = std::upper_bound(child_local_row_starts.begin(), child_local_row_starts.end(), local_row);
    if (it == child_local_row_starts.begin()) return nullptr;
    auto p = visible_children.begin() + std::distance(child_local_row_starts.begin(), it) - 1;
    return (*p)->entry_at_visible_row(local_row - child_local_row_starts[std::distance(visible_children.begin(), p)]);
}

ScanResultsNode * ScanResultsNode::nth_visible_child(int n) const {
    if (n < 0 || n >= num_rows()) return nullptr;
    return visible_children[n].get();
}

void ScanResultsNode::set_data(const nlohmann::json &d) {

    for (const auto &p : ScanResultsModel::data_model_roles) {
        const auto &role_name = p.second;
        if (d.contains(role_name)) {
            data[p.first] = ui::qml::json_to_qvariant(d[role_name]);
        }
    }
    data[ScanResultsModel::NAME_AND_COUNT] = data[ScanResultsModel::NAME];
}

ScanResultsModel::ScanResultsModel(QObject *parent) : super(parent) {

    root_entry_.reset(createNode(nullptr));

    for (const auto &p : data_model_roles) {
        data_model_role_name_to_id_[p.second] = p.first;
    }
    init(CafSystemObject::get_actor_system());
}

void ScanResultsModel::init(caf::actor_system &_system) {

    super::init(_system);
    connectToPythonPlugin();

    // We can defined custom message handlers here so that messages can be
    // received from other components (actors) in xSTUDIO. In this case we will
    // be interfacing with the plugin backend
    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {
            [=](utility::event_atom, utility::time_point when_to_sort) {

                if (sort_filter_time_point_ == when_to_sort) {
                    doFiltering();
                }

            },
            [=](utility::event_atom, bool reset) {
                // the message from the python plugin tells us to clear the whole model
                // ahead of a re-scan.
                if (reset) {
                    beginResetModel();
                    old_root_ = root_entry_;
                    root_entry_.reset(createNode(nullptr));
                    path_entries_.clear();
                    results_per_directory_.clear();
                    endResetModel();
                    root_index_ = index(0, 0);
                    emit rootIndexChanged();
                }
            },
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](utility::event_atom, const std::string &directory, const bool is_root, const std::string &__scan_dir_results) {

                auto scan_dir_results = std::make_shared<utility::JsonStore>();
                scan_dir_results->parse_string(__scan_dir_results);
                // make a seperate store 
                results_per_directory_[directory] = scan_dir_results;
                addDirectoryToModel(directory, scan_dir_results, is_root, false);

                schedule_sort_and_filter();
                
            },

        [=](caf::message &m) {
            spdlog::warn(
                "{} : unrecognised message received. Message content: {}",
                __PRETTY_FUNCTION__,
                to_string(m));
        }};
    });
}

QHash<int, QByteArray> ScanResultsModel::roleNames() const {

    QHash<int, QByteArray> roles;
    for (const auto &p : data_model_roles) {
        roles[p.first] = p.second.c_str();
    }

    return roles;
}

void ScanResultsModel::schedule_sort_and_filter() {

    // set a time point in the future to do the sort. If more data comes in
    // this gets reset so we're not running sort/filter while the scan data 
    // is still incoming.
    auto when_to_sort = utility::clock::now();
    sort_filter_time_point_ = when_to_sort;
    anon_mail(utility::event_atom_v, when_to_sort).delay(std::chrono::milliseconds(400)).send(
        as_actor());

}


int ScanResultsModel::rowCount(const QModelIndex &parent) const {

    const ScanResultsNode *e = get_entry(parent);
    if (e && e->num_rows() != -1) {
        return e->num_rows();
    }
 
    // we don't know the row count yet!
    return 1;

}

int ScanResultsModel::columnCount(const QModelIndex &parent) const { return 1; }

bool ScanResultsModel::hasChildren(const QModelIndex &parent) const { return rowCount(parent) != 0; }

ScanResultsNode *ScanResultsModel::createNode(ScanResultsNode *parent) {

    if (!parent) {
        // creating a new root node.
        root_node_id_ = next_node_id_;
        node_map_.clear();
    }
    auto n = new ScanResultsNode(parent, next_node_id_);
    node_map_[next_node_id_] = n;
    next_node_id_++;
    return n;
}

const ScanResultsNode *
ScanResultsModel::get_entry(int row, int column, const ScanResultsNode *parent_entry) const {

    const ScanResultsNode *result = nullptr;
    if (parent_entry) {
        if (parent_entry->num_rows() > row) {
            result = parent_entry->nth_visible_child(row);
        } else {
        }
    } else {
        result = root_entry_.get();
    }
    return result;
}


QModelIndex ScanResultsModel::index(int row, int column, const QModelIndex &parent) const {

    if (row < 0) {
        return createIndex(row, column);
    }
    const ScanResultsNode *parent_entry = get_entry(parent);
    const ScanResultsNode *entry = get_entry(row, column, parent_entry);
    return createIndex(row, column, entry ? entry->node_id : 0);
}


QModelIndex ScanResultsModel::parent(const QModelIndex &child) const {

    ScanResultsNode *e = get_entry(const_cast<QModelIndex &>(child));
    if (e && e->parent) {
        return createIndex(e->parent->row(), e->parent->column(), e->parent->node_id);
    }
    return {};
}

void ScanResultsModel::set(const QModelIndex &index, const QVariant &value, const QString &role_name)
{
    auto p = data_model_role_name_to_id_.find(role_name.toStdString());
    if (p != data_model_role_name_to_id_.end()) {
        setData(index, value, p->second);
    }   
}

QVariant ScanResultsModel::get(const QModelIndex &index, const QString &role_name) const {

    auto p = data_model_role_name_to_id_.find(role_name.toStdString());
    if (p != data_model_role_name_to_id_.end()) {
        return data(index, p->second);
    }
    return QVariant();
}

bool ScanResultsModel::setData(const QModelIndex &index, const QVariant &value, int role) {

    ScanResultsNode *e = get_entry(index);

    if (e) {
        const auto p = e->data.find(role);
        if (p == e->data.end() || p->second != value) {
            e->data[role] = value;
            emit dataChanged(index, index, {role});
            if (role == IS_EXPANDED) {
                // if the expanded state has changed, we need to update the row count for this entry and all parents.
                e->update_total_visible_rows();
            }
            return true;
        }
    }
    return false;
}

QVariant ScanResultsModel::data(const QModelIndex &index, int role) const {

    ScanResultsNode *e = get_entry(index);

    if (e) {
        const auto p = e->data.find(role);
        if (p != e->data.end()) {
            return p->second;
        }
    }
    // no data ... YET!
    return QVariant(QString());

}

bool ScanResultsModel::canFetchMore(const QModelIndex &parent) const {
    auto result          = false;
    ScanResultsNode *e = get_entry(parent);
    if (!e || e->num_rows() == -1)
        result = true;
    return result;
}

void ScanResultsModel::addDirectoryToModel(
    const std::string &directory,
    std::shared_ptr<utility::JsonStore> &scan_dir_results,
    const bool is_root,
    const bool full_rebuild) {

    if (view_mode_ == 0) {

        // view mode 0 is a flat list of all scanned files. No folders are shown.
        if (scan_dir_results->is_array()) {
            
            // auto t0 = utility::clock::now();
            // total number of rows - if we're mode 0 (flat list of files) then
            // we skip directories.
            int count = 0;
            for (auto &j: *scan_dir_results) {
                // skip directories in view mode 0
                if ((view_mode_ == 0 && j.value("is_folder", true))) continue;
                count++;
                
            }
            //auto t1 = utility::clock::now();

            if (!count) {
                // no results we want to show, so return early
                return;
            }

            
            int rc = root_entry_->num_rows();
            std::vector<std::shared_ptr<ScanResultsNode>> new_entries(count);
            int i = 0;
            for (auto &j: *scan_dir_results) {
                if (j.value("is_folder", true)) continue;
                new_entries[i].reset(createNode(root_entry_.get()));
                new_entries[i]->set_data(j);
                i++;
            }
            root_entry_->add_rows(new_entries);

            int new_rc = root_entry_->num_rows();
            if (new_rc != rc && !full_rebuild) {
                QModelIndex parent_index = index(0, 0);
                beginInsertRows(parent_index, rc, new_rc-1);
                insertRows(rc, (new_rc-rc), parent_index);
                endInsertRows();
            }

        }

    } else if (view_mode_ == 1) {

        auto parent_entry = root_entry_;
        auto p = path_entries_.find(directory);
        if (p != path_entries_.end()) {
            parent_entry = p->second;
        }

        int count = scan_dir_results->size();
        if (!count) {
            parent_entry->data[IS_SCANNED] = true;
            return;
        }

        utility::JsonStore j;
        j["is_folder"] = true;
        j["is_expanded"] = is_root;
        j["is_scanned"] = true;
        parent_entry->set_data(j);

        int rc = parent_entry->num_rows();
        std::vector<std::shared_ptr<ScanResultsNode>> new_entries(scan_dir_results->size());
        int i = 0;
        for (auto &j: *scan_dir_results) {
            new_entries[i].reset(createNode(parent_entry.get()));
            new_entries[i]->set_data(j);
            if (j.value("is_folder", true) == true) {
                path_entries_[j["path"].get<std::string>()] = new_entries[i];   
            }
            i++;
        }
        parent_entry->add_rows(new_entries);
        parent_entry->filter_and_sort(search_string_, timestamp_cutoff(), current_filter_version_rank_, sort_role_, sort_ascending_);

        int new_rc = parent_entry->num_rows();
        if (new_rc > rc && !full_rebuild) {
            QModelIndex parent_index = createIndex(parent_entry->row(), 0, parent_entry->node_id);
            beginInsertRows(parent_index, rc, new_rc-1);
            insertRows(rc, (new_rc-rc), parent_index);
            endInsertRows();
        }

    } else if (view_mode_ == 3 && scan_dir_results->size()) {

        int count = 0;
        for (auto &j: *scan_dir_results) {
            if (j.value("is_folder", true)) continue;
            count++;
        }
        if (!count) {
            return;
        }

        // add a row to root entry for the directory.
        utility::JsonStore j;
        j["name"] = directory;
        j["is_folder"] = true;
        j["path"] = directory;
        std::shared_ptr<ScanResultsNode> directory_entry(createNode(root_entry_.get()));
        directory_entry->set_data(j);
        int rc0 = root_entry_->num_rows();
        root_entry_->add_row(directory_entry);

        std::vector<std::shared_ptr<ScanResultsNode>> new_entries;
        for (auto &j: *scan_dir_results) {
            if (j.value("is_folder", true)) continue;
            new_entries.emplace_back(createNode(directory_entry.get()));
            new_entries.back()->set_data(j);
        }
        directory_entry->add_rows(new_entries);

        int new_rc = root_entry_->num_rows();
        if (new_rc != rc0 && !full_rebuild) {
            QModelIndex parent_index = index(0, 0);
            beginInsertRows(parent_index, rc0, new_rc-1);
            insertRows(rc0, (new_rc-rc0), parent_index);
            endInsertRows();
        }

    }
}

ScanResultsNode * ScanResultsModel::get_entry(const QModelIndex &index) const {
    if (!index.isValid()) return nullptr;
    if (index.internalId() < root_node_id_) {
        return nullptr;
    }
    const auto p = node_map_.find(index.internalId());
    if (p == node_map_.end()) {
        return nullptr;
    }
    return p->second;
}

void ScanResultsModel::setEntryData(QModelIndex &index, const std::string &path, const nlohmann::json &data) {

    ScanResultsNode *e = get_entry(index);

    if (e) {
        e->path = data.value("path", "");
        for (const auto &p : data_model_roles) {
            const auto &role_name = p.second;
            if (data.contains(role_name)) {
                e->data[p.first] = ui::qml::json_to_qvariant(data[role_name]);
            }
        }
        /*e->data = data;
        if (view_mode_ == 0) {
            e->data["depth"] = 0;
        }*/
    }
}

QModelIndex ScanResultsModel::indexAtVisibleRow(int row) const {

    QModelIndex result;
    if (root_entry_) {
        auto entry = root_entry_->entry_at_visible_row(row);
        if (entry) {
            result = createIndex(entry->row(), 0, entry->node_id);
        }
    }
    return result;
}

void ScanResultsModel::doFiltering() {

    if (view_mode_ == 0) {
        // in view mode 0 we don't have a tree structure, so we can just filter the root entry.
        const int nr = root_entry_->num_rows();
        root_entry_->filter_and_sort(search_string_, timestamp_cutoff(), current_filter_version_rank_, sort_role_, sort_ascending_);

        // assess change in number of rows.
        const int new_nr = root_entry_->num_rows();

        if (new_nr > nr) {
            beginInsertRows(index(0, 0), nr, new_nr-1);
            insertRows(nr, new_nr-nr, index(0, 0));
            endInsertRows();
        } else if (new_nr < nr) {
            beginRemoveRows(index(0, 0), new_nr, nr-1);
            removeRows(new_nr, nr-new_nr, index(0, 0));
            endRemoveRows();
        }

        // brute force update all rows - too hard to work out which rows have changed, so just update all of them.
        emit dataChanged(index(0, 0, index(0, 0)), index(new_nr-1, 0, index(0, 0)), {NAME, NAME_AND_COUNT, DATE, SIZE_STR, OWNER, PATH, VERSION});

    } else {
        // in view mode 1 we have a tree structure, so we need to filter all entries.
        emit layoutAboutToBeChanged();
        beginResetModel();
        root_entry_->filter_and_sort(search_string_, timestamp_cutoff(), current_filter_version_rank_, sort_role_, sort_ascending_);
        if (root_entry_->num_rows() > 0) {
            beginInsertRows(index(0, 0), 0, root_entry_->num_rows()-1);
            insertRows(0, root_entry_->num_rows(), index(0, 0));
            endInsertRows();
        }
        endResetModel();
        emit layoutChanged();
        root_index_ = index(0, 0);
        emit rootIndexChanged();
    }

    int num_media_files = root_entry_->total_file_count();
    if (num_media_files != num_media_files_) {
        num_media_files_ = num_media_files;
        emit numMediaFilesChanged();
    }

    int num_root_folders = 0;
    if (!results_per_directory_.empty()) {
        // scan results for the root folder are always the first entry in the map
        // because the map is keyed by directory path and the root folder is always
        // the first entry in lexicographical order.
        for (const auto & j: *(results_per_directory_.begin()->second)) {
            if (j.value("is_folder", true)) num_root_folders++;
        }
    }
    if (num_root_folders != num_root_folders_) {
        num_root_folders_ = num_root_folders;
        emit numRootFoldersChanged();
    }

}

void ScanResultsModel::setSearchString(const QString &s) {
    if (s != search_string_) {
        search_string_ = s;
        emit searchStringChanged();
        doFiltering();
    }   
}

void ScanResultsModel::setCurrentFilterTime(const QString &s) {
    if (s != current_filter_time_) {
        current_filter_time_ = s;
        static std::map<QString, double> filter_time_map = {
            {"Any", 0},
            {"Last 1 day", 60*60*24},
            {"Last 2 days", 60*60*24*2},
            {"Last 1 week", 60*60*24*7},
            {"Last 1 month", 60*60*24*30}
        };
        auto p = filter_time_map.find(s);
        if (p != filter_time_map.end()) {
            current_filter_time_seconds_ = p->second;
        } else {
            current_filter_time_seconds_ = 0;
        }
        emit currentFilterTimeChanged();
        doFiltering();
    }
}

void ScanResultsModel::setCurrentFilterVersion(const QString &s) {
    if (s != current_filter_version_) {
        current_filter_version_ = s;
        static std::map<QString, int> filter_version_map = {
            {"All Versions", -1},
            {"Latest Version", 0},
            {"Latest 2 Versions", 1}
        };
        auto p = filter_version_map.find(s);
        if (p != filter_version_map.end()) {
            current_filter_version_rank_ = p->second;
        } else {
            current_filter_version_rank_ = -1;
        }
        emit currentFilterVersionChanged();
        doFiltering();
    }
}

void ScanResultsModel::fullRebuild()
{
    beginResetModel();
    emit layoutAboutToBeChanged();
    
    old_root_ = root_entry_;
    root_entry_.reset(createNode(nullptr));

    root_index_ = index(0, 0);
    emit rootIndexChanged();
    path_entries_.clear();
    bool root = true;
    for (auto &entry : results_per_directory_) {
        addDirectoryToModel(entry.first, entry.second, root, true);
        root = false;
    }
    emit layoutChanged();
    doFiltering();
    endResetModel();
    root_index_ = index(0, 0);
    emit rootIndexChanged();

}

double ScanResultsModel::timestamp_cutoff() const {
    if (current_filter_time_seconds_ == 0) return 0;
    const auto tp = std::chrono::system_clock::now().time_since_epoch();
    auto time_since_epoch = static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(tp).count());
    return time_since_epoch - current_filter_time_seconds_;
}

void ScanResultsModel::connectToPythonPlugin() {

    // get to the plugin manager actor through the actor registry.
    auto foodar = system().registry().template get<caf::actor>("ScanResultsMiddleman");
    anon_mail(broadcast::join_broadcast_atom_v, as_actor()).send(foodar);

    /*try {

        // get the python interpreter actor from the global actor
        scoped_actor sys{system()};
        auto python_interp = utility::request_receive<caf::actor>(
            *sys,
            global_actor,
            global::get_python_atom_v);
        if (!python_interp) {
            spdlog::critical("{} Failed to get python interpreter actor", __PRETTY_FUNCTION__);
            return;
        }

        // now we add our actor address to an args dictionary. xSTUDIO python interpreter
        // will convert this back to an actor object (python wrapped object) and then
        // call the method register_scan_results_ui_model on the File Browser python plugin, passing
        // the actor as an argument. This allows the python plugin to send messages to this UI model actor (for example to send scan results back to the UI).
        utility::JsonStore args;
        args["results_model_actor"] = utility::actor_to_string(system(), as_actor());

        auto r = utility::request_receive<utility::JsonStore>(
            *sys,
            python_interp,
            embedded_python::python_exec_atom_v,
                    "File Browser",        // plugin name
                    "register_scan_results_ui_model", // method on plugin
                    args       // arguments for method
            );
        std::cerr << "Got response from python plugin: " << r.dump(4) << std::endl;
    } catch (const std::exception &err) {
        spdlog::critical("{} {}", __PRETTY_FUNCTION__, err.what());
    }*/

}

ScanResultsFilterModel::ScanResultsFilterModel(QObject *parent) : QSortFilterProxyModel(parent) {

}

bool ScanResultsFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    if (!sourceModel()) return false;
    auto idx = sourceModel()->index(source_row, 0, source_parent);
    return sourceModel()->data(idx, ScanResultsModel::IS_VISIBLE).toBool();
}

XSTUDIO_PLUGIN_DECLARE_BEGIN()

XSTUDIO_REGISTER_PLUGIN(
    ScanResultsMiddleman,
    ScanResultsMiddleman::PLUGIN_UUID,
    Scan Results Middleman,
    plugin_manager::PluginFlags::PF_UTILITY,
    true,
    Ted Waine Sam Richardson,
    Plugin actor to receive scan results from Python plugin and forward to UI model,
    1.0.0)

XSTUDIO_PLUGIN_DECLARE_END()