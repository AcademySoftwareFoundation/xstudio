// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::utility;

std::string xstudio::utility::to_string(const Uuid &uuid) {
    return uuids::to_string(uuid.uuid_);
}

void xstudio::utility::to_json(nlohmann::json &j, const Uuid &uuid) { j = to_string(uuid); }

void xstudio::utility::from_json(const nlohmann::json &j, Uuid &uuid) { uuid.from_string(j); }


UuidListContainer::UuidListContainer(const std::vector<Uuid> &uuids) {
    uuids_.insert(uuids_.end(), uuids.begin(), uuids.end());
}

UuidListContainer::UuidListContainer(const JsonStore &jsn) {
    for (const auto &i : jsn) {
        uuids_.push_back(i);
    }
}

JsonStore UuidListContainer::serialise() const {
    JsonStore jsn;

    jsn = nlohmann::json::array();
    for (const auto &i : uuids_) {
        jsn.push_back(i);
    }

    return jsn;
}

void UuidListContainer::insert(const utility::Uuid &uuid, const utility::Uuid &uuid_before) {
    uuids_.insert(std::find(uuids_.begin(), uuids_.end(), uuid_before), uuid);
}

bool UuidListContainer::remove(const utility::Uuid &uuid) {
    auto it      = std::find(uuids_.begin(), uuids_.end(), uuid);
    bool removed = false;

    if (it != std::end(uuids_)) {
        uuids_.erase(it);
        removed = true;
    }

    return removed;
}

bool UuidListContainer::swap(const Uuid &from, const Uuid &to) {
    if (contains(from)) {
        insert(to, from);
        return remove(from);
    }
    return false;
}

Uuid UuidListContainer::next_uuid(const Uuid &uuid) const {
    auto result = Uuid();

    auto it = std::find(uuids_.begin(), uuids_.end(), uuid);
    if (it != std::end(uuids_)) {
        it++;
        result = *it;
    }

    return result;
}

UuidVector UuidListContainer::uuid_vector() const {
    UuidVector result(uuids_.size());
    std::copy(uuids_.begin(), uuids_.end(), result.begin());
    return result;
}

bool UuidListContainer::move(const utility::Uuid &uuid, const utility::Uuid &uuid_before) {
    auto it    = std::find(uuids_.begin(), uuids_.end(), uuid);
    bool moved = false;

    if (it != std::end(uuids_)) {
        auto dit = std::find(uuids_.begin(), uuids_.end(), uuid_before);
        if (uuid_before.is_null() or dit != std::end(uuids_)) {
            uuids_.splice(dit, uuids_, it);
            moved = true;
        }
    }

    return moved;
}

size_t UuidListContainer::count(const utility::Uuid &uuid) const {
    size_t cnt = 0;
    for (const auto &i : uuids_) {
        if (i == uuid)
            cnt++;
    }
    return cnt;
}

bool UuidListContainer::contains(const utility::Uuid &uuid) const {
    auto it = std::find(uuids_.begin(), uuids_.end(), uuid);
    return it != std::end(uuids_);
}