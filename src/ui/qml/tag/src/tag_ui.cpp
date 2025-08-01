// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/tag_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/container.hpp"

#include <QDebug>
#include <QTimer>

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::tag;
using namespace xstudio::ui::qml;

TagUI::TagUI(const tag::Tag tag, QObject *parent) : QObject(parent), tag_(std::move(tag)) {}

TagAttrs::TagAttrs(QObject *parent) : QQmlPropertyMap(this, parent) {}

void TagAttrs::reset() {
    for (const auto &i : keys())
        this->clear(i);
}


void TagListUI::addTag(TagUI *tag) {
    tags_.append(tag);
    emit tagsChanged();
}

bool TagListUI::removeTag(const QUuid &id) {
    for (auto i = 0; i < tags_.size(); i++) {
        if (qobject_cast<TagUI *>(tags_[i])->id() == id) {
            // spdlog::warn("TAG REMOVED {}", to_string(UuidFromQUuid(id)));
            tags_.removeAt(i);
            emit tagsChanged();
            return true;
        }
    }
    return false;
}


TagManagerUI::TagManagerUI(QObject *parent) : QMLActor(parent), attrs_map_(new TagAttrs(this)) {

    init(CafSystemObject::get_actor_system());
}

void TagManagerUI::set_backend(caf::actor backend) {
    backend_ = backend;
    attrs_map_->reset();

    scoped_actor sys{system()};

    if (backend_events_) {
        try {
            request_receive<bool>(
                *sys, backend_events_, broadcast::leave_broadcast_atom_v, as_actor());
        } catch ([[maybe_unused]] const std::exception &e) {
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
        backend_events_ = caf::actor();
    }


    if (backend_) {
        try {
            backend_events_ =
                request_receive<caf::actor>(*sys, backend_, get_event_group_atom_v);
            request_receive<bool>(
                *sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());
        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }

        // get current tags.
        try {
            auto tags = request_receive<std::vector<Tag>>(*sys, backend_, get_tags_atom_v);

            for (const auto &i : tags)
                add_tag(i);

        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
}

void TagManagerUI::add_tag(const tag::Tag &tag) {
    // does link exist..

    // spdlog::warn("add_tag {} {} {} {}", tag.unique(), tag.type(), to_string(tag.link()),
    // to_string(tag.id()));

    auto uuid = QUuidFromUuid(tag.link()).toString();
    if (not attrs_map_->contains(uuid) or attrs_map_->value(uuid).isNull()) {
        attrs_map_->insert(uuid, QVariant::fromValue(new QQmlPropertyMap(this)));
    }

    auto type_map =
        qobject_cast<QQmlPropertyMap *>(qvariant_cast<QObject *>(attrs_map_->value(uuid)));

    // check for type..
    auto type = QStringFromStd(tag.type());
    if (not type_map->contains(type) or type_map->value(type).isNull()) {
        type_map->insert(type, QVariant::fromValue(new TagListUI(this)));
    }

    auto tags = qobject_cast<TagListUI *>(qvariant_cast<QObject *>(type_map->value(type)));
    tags->addTag(new TagUI(tag, this));
}

void TagManagerUI::remove_tag(const utility::Uuid &id) {
    // trickier.. need to find it..
    // spdlog::warn("remove_tag {}", to_string(id));

    auto qid = QUuidFromUuid(id);

    for (const auto &i : attrs_map_->keys()) {
        auto attrs =
            qobject_cast<QQmlPropertyMap *>(qvariant_cast<QObject *>(attrs_map_->value(i)));
        if (attrs) {
            for (const auto &j : attrs->keys()) {
                auto tags =
                    qobject_cast<TagListUI *>(qvariant_cast<QObject *>(attrs->value(j)));
                if (tags && tags->removeTag(qid))
                    return;
            }
        }
    }
}

void TagManagerUI::init(caf::actor_system &system) {

    QMLActor::init(system);

    spdlog::debug("TagManagerUI init");

    self()->set_default_handler(caf::drop);

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {
            [=](utility::event_atom, tag::remove_tag_atom, const utility::Uuid &id) {
                remove_tag(id);
            },
            [=](utility::event_atom, tag::add_tag_atom, const Tag &tag) { add_tag(tag); },
            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {},
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](const group_down_msg &) {
                // caf::aout(self()) << "TagManagerUI down: " << to_string(g.source) <<
                // std::endl;
            }};
    });
}
