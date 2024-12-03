// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

// special class for handing synced json stores, we restrict access so we don't miss changes.

namespace xstudio::utility {
const auto SyncEvent =
    R"({
	        "undo": {},
	        "redo": {}
	    })"_json;

const auto RemoveRowsEvent =
    R"({
	        "type": "remove_rows",
	        "parent": "",
	        "row": -1,
	        "count": 0,
	        "id": null
	    })"_json;

const auto InsertRowsEvent =
    R"({
	        "type": "insert_rows",
	        "parent": "",
	        "row": -1,
	        "count": 0,
	        "id": null,
	        "data": []
	    })"_json;

const auto InsertEvent =
    R"({
	        "type": "insert",
	        "parent": "",
	        "key": "",
	        "data": null
	    })"_json;

const auto RemoveEvent =
    R"({
	        "type": "remove",
	        "parent": "",
	        "key": ""
	    })"_json;

const auto SetEvent =
    R"({
	        "type": "set",
	        "parent": "",
	        "row": -1,
	        "id": null,
	        "data": {}
	    })"_json;

const auto MoveEvent =
    R"({
	        "type": "move",
	        "src_parent": "",
	        "src_row": -1,
	        "count": 0,
	        "id": null,
	        "dst_parent": "",
	        "dst_row": -1
	    })"_json;

const auto ResetEvent =
    R"({
	        "type": "reset",
	        "id": null,
	        "data": null
	    })"_json;

class JsonStoreSync : protected JsonStore {
  public:
    typedef std::function<void(const nlohmann::json &event, const bool undo_redo)>
        SendEventFunc;

    JsonStoreSync(nlohmann::json json = R"({"children":[]})"_json);
    virtual ~JsonStoreSync() = default;

    void bind_send_event_func(SendEventFunc fs);

    void apply_event(const nlohmann::json &event) { process_event(event, true, true, true); }
    void unapply_event(const nlohmann::json &event) { process_event(event, false, true, true); }
    void process_event(
        const nlohmann::json &event,
        const bool redo      = true,
        const bool undo_redo = false,
        const bool local     = false);

    void insert(
        const std::string &key,
        const nlohmann::json &data = nlohmann::json(),
        const std::string &parent  = "");

    void insert_rows(
        const int row,
        const int count,
        const nlohmann::json &data = nlohmann::json::array(),
        const std::string &parent  = "");

    void remove(const std::string &key, const std::string &parent = "");

    void remove_rows(const int row, const int count, const std::string &parent = "");

    void
    set(const int row,
        const std::string &key,
        const nlohmann::json &data = nlohmann::json(),
        const std::string &parent  = "");
    void
    set(const int row,
        const nlohmann::json &data = nlohmann::json::object(),
        const std::string &parent  = "");

    void move_rows(
        const std::string &src_parent,
        const int src_row,
        const int count,
        const std::string &dst_parent,
        const int dst_row);

    void reset_data(const nlohmann::json &data);

    std::vector<nlohmann::json::json_pointer> find(
        const std::string &key,
        const std::optional<nlohmann::json> value  = {},
        const int max_find_count                   = -1,
        const int prune_depth                      = -1,
        const nlohmann::json::json_pointer &parent = nlohmann::json::json_pointer(),
        const int row                              = 0) const;

    std::optional<nlohmann::json::json_pointer> find_first(
        const std::string &key,
        const std::optional<nlohmann::json> value  = {},
        const int prune_depth                      = -1,
        const nlohmann::json::json_pointer &parent = nlohmann::json::json_pointer(),
        const int row                              = 0) const;

    using JsonStore::dump;

    nlohmann::json::const_reference at(size_type idx) const;
    nlohmann::json::const_reference at(const nlohmann::json::json_pointer &ptr) const;
    nlohmann::json::const_reference
    at(const typename nlohmann::json::object_t::key_type &key) const;

    // template<typename KeyType>
    // const_reference at(KeyType&& key) const;
    const nlohmann::json &as_json() const;

    [[nodiscard]] bool origin() const { return origin_; }
    void set_origin(const bool origin) { origin_ = origin; }

    [[nodiscard]] utility::Uuid id() const { return id_; }
    void set_id(const utility::Uuid &id) { id_ = id; }

    [[nodiscard]] nlohmann::json last_event() const { return last_event_; }

  private:
    void reset_data_base(
        const nlohmann::json &data,
        const bool local,
        const utility::Uuid &id,
        const bool undo_redo);

    void insert_base(
        const std::string &key,
        const nlohmann::json &data,
        const std::string &parent,
        const bool local,
        const utility::Uuid &id,
        const bool undo_redo);

    void insert_rows_base(
        const int row,
        const int count,
        const nlohmann::json &data,
        const std::string &parent,
        const bool local,
        const utility::Uuid &id,
        const bool undo_redo);

    void remove_base(
        const std::string &key,
        const std::string &parent,
        const bool local,
        const utility::Uuid &id,
        const bool undo_redo);

    void remove_rows_base(
        const int row,
        const int count,
        const std::string &parent,
        const bool local,
        const utility::Uuid &id,
        const bool undo_redo);

    void set_base(
        const int row,
        const nlohmann::json &data,
        const std::string &parent,
        const bool local,
        const utility::Uuid &id,
        const bool undo_redo);

    void move_rows_base(
        const std::string &src_parent,
        const int src_row,
        const int count,
        const std::string &dst_parent,
        const int dst_row,
        const bool local,
        const utility::Uuid &id,
        const bool undo_redo);

    bool origin_{false};
    SendEventFunc event_send_func_{nullptr};
    utility::Uuid id_ = Uuid::generate();
    nlohmann::json last_event_;
    std::string children_{"children"};
};
} // namespace xstudio::utility
