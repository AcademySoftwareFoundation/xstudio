// SPDX-License-Identifier: Apache-2.0

#include "xstudio/utility/json_store_sync.hpp"

using namespace nlohmann;
using namespace xstudio::utility;

JsonStoreSync::JsonStoreSync(nlohmann::json json) : JsonStore(std::move(json)) {}

void JsonStoreSync::bind_send_event_func(SendEventFunc fs) {
    event_send_func_ = [fs](auto &&PH1, auto &&PH2) {
        return fs(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
    };
}

void JsonStoreSync::process_event(
    const nlohmann::json &event_pair, const bool redo, const bool undo_redo, const bool local) {

    const auto &event = (redo ? event_pair.at("redo") : event_pair.at("undo"));

    auto type = event.value("type", "");
    auto id   = event.value("id", Uuid());

    // spdlog::warn("JsonStoreSync::process_event {}", event.dump(2));

    // ignore reflected events
    if (not redo or id != id_) {
        last_event_ = event_pair;
        if (type == "insert_rows") {
            insert_rows_base(
                event.value("row", 0),
                event.value("count", 0),
                event.value("data", nlohmann::json::array()),
                event.value("parent", ""),
                local,
                local ? id_ : id,
                undo_redo);
        } else if (type == "insert") {
            insert_base(
                event.value("key", ""),
                event.value("data", nlohmann::json()),
                event.value("parent", ""),
                local,
                local ? id_ : id,
                undo_redo);
        } else if (type == "remove_rows") {
            remove_rows_base(
                event.value("row", 0),
                event.value("count", 0),
                event.value("parent", ""),
                local,
                local ? id_ : id,
                undo_redo);
        } else if (type == "remove") {
            remove_base(
                event.value("key", ""),
                event.value("parent", ""),
                local,
                local ? id_ : id,
                undo_redo);
        } else if (type == "set") {
            set_base(
                event.value("row", 0),
                event.value("data", nlohmann::json::object()),
                event.value("parent", ""),
                local,
                local ? id_ : id,
                undo_redo);
        } else if (type == "move") {
            move_rows_base(
                event.value("src_parent", ""),
                event.value("src_row", 0),
                event.value("count", 0),
                event.value("dst_parent", ""),
                event.value("dst_row", 0),
                local,
                local ? id_ : id,
                undo_redo);
        } else if (type == "reset") {
            reset_data_base(
                event.value("data", nlohmann::json::object()),
                local,
                local ? id_ : id,
                undo_redo);
        }
    }
}

void JsonStoreSync::insert_rows(
    const int row, const int count, const nlohmann::json &data, const std::string &parent) {
    insert_rows_base(row, count, data, parent, true, id_, false);
}


void JsonStoreSync::insert(
    const std::string &key, const nlohmann::json &data, const std::string &parent) {
    insert_base(key, data, parent, true, id_, false);
}

void JsonStoreSync::remove(const std::string &key, const std::string &parent) {
    remove_base(key, parent, true, id_, false);
}

void JsonStoreSync::reset_data(const nlohmann::json &data) {
    reset_data_base(data, true, id_, false);
}

void JsonStoreSync::remove_base(
    const std::string &key,
    const std::string &parent,
    const bool local,
    const utility::Uuid &id,
    const bool undo_redo) {

    // try doing it..
    auto ptr = json::json_pointer(parent);
    auto &p  = (*(this))[ptr];

    // if local broadcast
    if (local or origin_) {
        auto event              = xstudio::utility::SyncEvent;
        event["redo"]           = xstudio::utility::RemoveEvent;
        event["redo"]["id"]     = id;
        event["redo"]["parent"] = parent;
        event["redo"]["key"]    = key;

        event["undo"]           = xstudio::utility::InsertEvent;
        event["undo"]["id"]     = id;
        event["undo"]["parent"] = parent;
        event["undo"]["key"]    = key;
        event["undo"]["data"]   = p.at(key);

        last_event_ = event;
        if (event_send_func_)
            event_send_func_(event, undo_redo);
    }

    p.erase(key);
}

void JsonStoreSync::insert_base(
    const std::string &key,
    const nlohmann::json &data,
    const std::string &parent,
    const bool local,
    const utility::Uuid &id,
    const bool undo_redo) {

    // try doing it..
    auto ptr = json::json_pointer(parent);
    auto &p  = (*(this))[ptr];

    // if local broadcast
    if (local or origin_) {
        auto event              = xstudio::utility::SyncEvent;
        event["redo"]           = xstudio::utility::InsertEvent;
        event["redo"]["id"]     = id_;
        event["redo"]["parent"] = parent;
        event["redo"]["key"]    = key;
        event["redo"]["data"]   = data;

        if (p.count(key)) {
            event["undo"]           = xstudio::utility::InsertEvent;
            event["undo"]["id"]     = id;
            event["undo"]["parent"] = parent;
            event["undo"]["key"]    = key;
            event["undo"]["data"]   = p.at(key);
        } else {
            event["undo"]           = xstudio::utility::RemoveEvent;
            event["undo"]["id"]     = id;
            event["undo"]["parent"] = parent;
            event["undo"]["key"]    = key;
        }

        last_event_ = event;
        if (event_send_func_)
            event_send_func_(event, undo_redo);
    }

    p[key] = data;
}

void JsonStoreSync::insert_rows_base(
    const int row,
    const int count,
    const nlohmann::json &data,
    const std::string &parent,
    const bool local,
    const utility::Uuid &id,
    const bool undo_redo) {

    // try doing it..
    auto ptr = json::json_pointer(parent);
    auto &p  = (*(this))[ptr];

    if (p.at(children_).is_array()) {
        if (data.empty()) {
            p[children_].insert(std::next(p[children_].begin(), row), count, R"({})"_json);
        } else {
            p[children_].insert(std::next(p[children_].begin(), row), data.begin(), data.end());
        }

        // if local broadcast
        if (local or origin_) {
            auto event              = xstudio::utility::SyncEvent;
            event["redo"]           = xstudio::utility::InsertRowsEvent;
            event["redo"]["id"]     = id;
            event["redo"]["parent"] = parent;
            event["redo"]["row"]    = row;
            event["redo"]["count"]  = count;
            event["redo"]["data"]   = data;

            event["undo"]           = xstudio::utility::RemoveRowsEvent;
            event["undo"]["id"]     = id;
            event["undo"]["parent"] = parent;
            event["undo"]["row"]    = row;
            event["undo"]["count"]  = count;

            last_event_ = event;
            if (event_send_func_)
                event_send_func_(event, undo_redo);
        }
    }
}

void JsonStoreSync::remove_rows(const int row, const int count, const std::string &parent) {
    if (count)
        remove_rows_base(row, count, parent, true, id_, false);
}

void JsonStoreSync::remove_rows_base(
    const int row,
    const int count,
    const std::string &parent,
    const bool local,
    const utility::Uuid &id,
    const bool undo_redo) {
    // try doing it..
    auto ptr = json::json_pointer(parent);
    auto &p  = (*(this))[ptr];

    if (p.at(children_).is_array()) {

        if (local or origin_) {
            auto event              = xstudio::utility::SyncEvent;
            event["redo"]           = xstudio::utility::RemoveRowsEvent;
            event["redo"]["id"]     = id;
            event["redo"]["parent"] = parent;
            event["redo"]["row"]    = row;
            event["redo"]["count"]  = count;

            event["undo"]           = xstudio::utility::InsertRowsEvent;
            event["undo"]["id"]     = id;
            event["undo"]["parent"] = parent;
            event["undo"]["row"]    = row;
            event["undo"]["count"]  = count;

            auto undo_data = R"([])"_json;
            for (auto i = 0; i < count; i++)
                undo_data.push_back(p.at(children_).at(row + i));
            event["undo"]["data"] = undo_data;

            last_event_ = event;
            if (event_send_func_)
                event_send_func_(event, undo_redo);
        }

        p[children_].erase(
            std::next(p[children_].begin(), row), std::next(p[children_].begin(), row + count));
    }
}

void JsonStoreSync::set(
    const int row,
    const std::string &key,
    const nlohmann::json &data,
    const std::string &parent) {
    auto tmp = R"({})"_json;
    tmp[key] = data;
    set(row, tmp, parent);
}

void JsonStoreSync::set(const int row, const nlohmann::json &data, const std::string &parent) {
    set_base(row, data, parent, true, id_, false);
}

void JsonStoreSync::set_base(
    const int row,
    const nlohmann::json &data,
    const std::string &parent,
    const bool local,
    const utility::Uuid &id,
    const bool undo_redo) {

    // try doing it..
    auto ptr = json::json_pointer(parent);
    auto &p  = (*(this))[ptr];

    if (p.at(children_).is_array()) {

        if (local or origin_) {
            auto event              = xstudio::utility::SyncEvent;
            event["redo"]           = xstudio::utility::SetEvent;
            event["redo"]["id"]     = id;
            event["redo"]["parent"] = parent;
            event["redo"]["row"]    = row;
            event["redo"]["data"]   = data;

            event["undo"]           = xstudio::utility::SetEvent;
            event["undo"]["id"]     = id;
            event["undo"]["parent"] = parent;
            event["undo"]["row"]    = row;

            auto undo_data = R"({})"_json;
            for (const auto &i : data.items())
                undo_data[i.key()] = p.at(children_).at(row).at(i.key());

            event["undo"]["data"] = undo_data;

            last_event_ = event;
            if (event_send_func_)
                event_send_func_(event, undo_redo);
        }

        p[children_][row].update(data);
    }
}

void JsonStoreSync::move_rows(
    const std::string &src_parent,
    const int src_row,
    const int count,
    const std::string &dst_parent,
    const int dst_row) {
    move_rows_base(src_parent, src_row, count, dst_parent, dst_row, true, id_, false);
}

void JsonStoreSync::move_rows_base(
    const std::string &src_parent,
    const int src_row,
    const int count,
    const std::string &dst_parent,
    const int dst_row,
    const bool local,
    const utility::Uuid &id,
    const bool undo_redo) {

    auto sptr = json::json_pointer(src_parent);
    auto &sp  = (*(this))[sptr];
    auto tmp  = json::array();

    // entries are moved in to tmp, this make life much eaiser
    // but dst locations need to take this into account.

    // spdlog::warn("{}", sp.dump(2));


    if (sp.at(children_).is_array()) {
        // move entries to tmp buffer.
        auto e = std::next(sp.at(children_).begin(), src_row);

        for (auto i = 0; i < count; i++) {
            tmp.push_back(*e);
            e = sp[children_].erase(e);
        }
        // now inject into dst.

        auto dptr = json::json_pointer(dst_parent);
        auto &dp  = (*(this))[dptr];

        if (dp.at(children_).is_array()) {
            dp[children_].insert(
                std::next(dp.at(children_).begin(), dst_row), tmp.begin(), tmp.end());

            // if local broadcast
            if (local or origin_) {
                auto event = SyncEvent;

                event["redo"]               = xstudio::utility::MoveEvent;
                event["redo"]["id"]         = id;
                event["redo"]["src_parent"] = src_parent;
                event["redo"]["src_row"]    = src_row;
                event["redo"]["count"]      = count;
                event["redo"]["dst_parent"] = dst_parent;
                event["redo"]["dst_row"]    = dst_row;

                event["undo"]               = xstudio::utility::MoveEvent;
                event["undo"]["id"]         = id;
                event["undo"]["src_parent"] = dst_parent;
                event["undo"]["src_row"]    = dst_row;
                event["undo"]["count"]      = count;
                event["undo"]["dst_parent"] = src_parent;
                event["undo"]["dst_row"]    = src_row;

                last_event_ = event;
                if (event_send_func_)
                    event_send_func_(event, undo_redo);
            }
        }
    }
    // spdlog::warn("{}", sp.dump(2));
}

void JsonStoreSync::reset_data_base(
    const nlohmann::json &data,
    const bool local,
    const utility::Uuid &id,
    const bool undo_redo) {
    auto &tmp = JsonStore::at(json::json_pointer(""));

    if (local or origin_) {
        auto event = SyncEvent;

        event["redo"]         = xstudio::utility::ResetEvent;
        event["redo"]["id"]   = id;
        event["redo"]["data"] = data;

        event["undo"]         = xstudio::utility::ResetEvent;
        event["undo"]["id"]   = id;
        event["undo"]["data"] = tmp;

        last_event_ = event;
        if (event_send_func_)
            event_send_func_(event, undo_redo);
    }

    tmp = data;
}

const nlohmann::json &JsonStoreSync::as_json() const { return *this; }

nlohmann::json::const_reference JsonStoreSync::at(size_type idx) const {
    return JsonStore::at(idx);
}

nlohmann::json::const_reference
JsonStoreSync::at(const nlohmann::json::json_pointer &ptr) const {
    return JsonStore::at(ptr);
}

nlohmann::json::const_reference
JsonStoreSync::at(const typename nlohmann::json::object_t::key_type &key) const {
    return JsonStore::at(key);
}

std::optional<nlohmann::json::json_pointer> JsonStoreSync::find_first(
    const std::string &key,
    const std::optional<nlohmann::json> value,
    const int prune_depth,
    const nlohmann::json::json_pointer &parent,
    const int row) const {
    auto result = find(key, value, 1, prune_depth, parent, row);
    if (result.empty())
        return {};

    return result[0];
}


std::vector<nlohmann::json::json_pointer> JsonStoreSync::find(
    const std::string &key,
    const std::optional<nlohmann::json> value,
    const int max_find_count,
    const int prune_depth,
    const nlohmann::json::json_pointer &parent,
    const int row) const {
    std::vector<nlohmann::json::json_pointer> result;

    auto cptr = parent / children_;

    if (contains(cptr) and at(cptr).is_array()) {
        const auto &children = at(cptr);
        // check for matches
        for (size_t i = row; i < children.size(); i++) {
            if (children.at(i).is_object()) {
                if (children.at(i).count(key) and
                    (not value or (*value) == children.at(i).at(key))) {

                    result.emplace_back(cptr / std::to_string(i));
                    if (max_find_count > 0 and
                        static_cast<int>(result.size()) >= max_find_count)
                        break;
                }

                if (prune_depth - 1) {
                    auto child_results = find(
                        key,
                        value,
                        max_find_count - result.size(),
                        prune_depth - 1,
                        cptr / std::to_string(i),
                        0);
                    result.insert(result.end(), child_results.begin(), child_results.end());
                    if (max_find_count > 0 and
                        static_cast<int>(result.size()) >= max_find_count)
                        break;
                }
            }
        }
    }

    return result;
}
