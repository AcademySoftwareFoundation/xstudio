// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/event_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

#include <QDebug>
#include <QTimer>

using namespace caf;
using namespace xstudio;
using namespace xstudio::event;
using namespace xstudio::ui::qml;

EventUI::EventUI(const event::Event &event, QObject *parent) : QObject(parent) {
    update(event);
}

void EventUI::update(const event::Event &e) {
    auto tmp = event_;
    event_   = e;

    if (tmp.progress() != event_.progress())
        emit progressChanged();
    if (tmp.progress_minimum() != event_.progress_minimum())
        emit progressMinimumChanged();
    if (tmp.progress_maximum() != event_.progress_maximum())
        emit progressMaximumChanged();
    if (tmp.progress_percentage() != event_.progress_percentage())
        emit progressPercentageChanged();
    if (tmp.progress_text() != event_.progress_text())
        emit textChanged();
    if (tmp.progress_text_range() != event_.progress_text_range())
        emit textRangeChanged();
    if (tmp.progress_text_percentage() != event_.progress_text_percentage())
        emit textPercentageChanged();
    if (tmp.event() != event_.event())
        emit eventChanged();
    if (tmp.complete() != event_.complete())
        emit completeChanged();
}


EventAttrs::EventAttrs(QObject *parent) : QQmlPropertyMap(this, parent) {
    new EventManagerUI(this);
}

void EventAttrs::addEvent(const event::Event &e) {
    if (e.targets().empty()) {
        auto uuid = QUuid().toString();
        if (not contains(uuid) or value(uuid).isNull())
            insert(uuid, QVariant::fromValue(new EventUI(e, this)));
        else {
            qobject_cast<EventUI *>(qvariant_cast<QObject *>(value(uuid)))->update(e);
        }
    } else {
        for (const auto &i : e.targets()) {
            auto uuid = QUuidFromUuid(i).toString();

            if (not contains(uuid) or value(uuid).isNull()) {
                auto o = new EventUI(e, this);
                insert(uuid, QVariant::fromValue(o));
            } else {
                qobject_cast<EventUI *>(qvariant_cast<QObject *>(value(uuid)))->update(e);
            }
        }
    }
}

EventManagerUI::EventManagerUI(EventAttrs *attrs_map)
    : QMLActor(static_cast<QObject *>(attrs_map)), attrs_map_(attrs_map) {

    init(CafSystemObject::get_actor_system());
}

void EventManagerUI::init(caf::actor_system &system) {

    QMLActor::init(system);

    spdlog::debug("EventManagerUI init");

    self()->set_default_handler(caf::drop);

    self()->join(system.groups().get_local(global_event_group));

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {
            [=](utility::event_atom, const Event &e) { attrs_map_->addEvent(e); },
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg &g) {
                caf::aout(self())
                    << "EventManagerUI down: " << to_string(g.source) << std::endl;
            }};
    });
}
