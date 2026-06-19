// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include <caf/actor_registry.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/hotkey_ui.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;
using namespace xstudio::ui;

namespace {

nlohmann::json regular_keys(const Hotkey &hk, const bool _default) {
    auto r = nlohmann::json::array();
    auto p = Hotkey::key_names.find(_default ? hk.default_key_id() : hk.key_id());
    if (p != Hotkey::key_names.cend()) {
        r.push_back(p->second);
    } else {
        r.push_back(hk.key());
    }
    return r;
}

nlohmann::json modifier_keys(const Hotkey &hk, const bool _default) {
    auto r              = nlohmann::json::array();
    const int modifiers = _default ? hk.default_modifiers() : hk.modifiers();
    if ((modifiers & ShiftModifier) == ShiftModifier) {
        r.push_back(Hotkey::key_names.at(Hotkey::modifier_to_key.at(ShiftModifier)));
    }
    if ((modifiers & MetaModifier) == MetaModifier) {
        r.push_back(Hotkey::key_names.at(Hotkey::modifier_to_key.at(MetaModifier)));
    }
    if ((modifiers & AltModifier) == AltModifier) {
        r.push_back(Hotkey::key_names.at(Hotkey::modifier_to_key.at(AltModifier)));
    }
    if ((modifiers & ControlModifier) == ControlModifier) {
        r.push_back(Hotkey::key_names.at(Hotkey::modifier_to_key.at(ControlModifier)));
    }
    return r;
}
} // namespace


HotkeysUI::HotkeysUI(QObject *parent) : super(parent) {

    setRoleNames(
        std::vector<std::string>(
            {"clashWarning",
             "defaultKeyboardKey",
             "defaultKeyModifiers",
             "keyboardKey",
             "hotkeyUuid",
             "keyModifiers",
             "hotkeyName",
             "hotkeyCategory",
             "hotkeyDescription",
             "hotkeySequence",
             "hotkeyErrorMessage"}));

    init(CafSystemObject::get_actor_system());

    try {

        scoped_actor sys{system()};

        keyboard_manager_ = system().registry().template get<caf::actor>(keyboard_events);

        auto hotkeys_config_events_group = utility::request_receive<caf::actor>(
            *sys,
            keyboard_manager_,
            utility::get_event_group_atom_v,
            keypress_monitor::hotkey_event_atom_v);

        request_receive<bool>(
            *sys, hotkeys_config_events_group, broadcast::join_broadcast_atom_v, as_actor());

        hotkeys_data_ = request_receive<std::vector<Hotkey>>(
            *sys, keyboard_manager_, keypress_monitor::register_hotkey_atom_v);

        update_hotkeys_model_data();

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](keypress_monitor::hotkey_event_atom, const std::vector<Hotkey> &hotkeys) {
                // update_hotkeys_model_data(hotkeys);
            },
            [=](keypress_monitor::hotkey_event_atom,
                const std::vector<std::string> &pressed_keys,
                const std::vector<std::string> &modifiers) {},
            [=](keypress_monitor::hotkey_event_atom, Hotkey &hotkey) {
                bool match = false;
                for (auto &hk : hotkeys_data_) {
                    if (hk.uuid() == hotkey.uuid()) {
                        hk    = hotkey;
                        match = true;
                        break;
                    }
                }
                if (!match) {
                    hotkeys_data_.push_back(hotkey);
                }
                update_hotkeys_model_data();
            },
            [=](keypress_monitor::hotkey_event_atom,
                const utility::Uuid kotkey_uuid,
                const bool pressed,
                const std::string &context,
                const std::string &window,
                const bool due_to_focus_change) {
                // actual hotkey pressed or release ... we ignore
            }};
    });
}

void HotkeysUI::update_hotkeys_model_data() { makeFilteredHotkeysData(); }

void HotkeysUI::makeFilteredHotkeysData() {

    filtered_hotkeys_data_.clear();

    for (const auto &hk : hotkeys_data_) {
        if (search_string_.isEmpty() ||
            QStringFromStd(hk.hotkey_name()).contains(search_string_, Qt::CaseInsensitive) ||
            QStringFromStd(hk.hotkey_description())
                .contains(search_string_, Qt::CaseInsensitive) ||
            QStringFromStd(hk.hotkey_origin()).contains(search_string_, Qt::CaseInsensitive) ||
            QStringFromStd(hk.hotkey_sequence())
                .contains(search_string_, Qt::CaseInsensitive)) {
            filtered_hotkeys_data_.push_back(hk);
        }
    }

    // sort by the name of the hotkey
    std::sort(
        filtered_hotkeys_data_.begin(),
        filtered_hotkeys_data_.end(),
        [](const Hotkey &a, const Hotkey &b) -> bool {
            if (a.hotkey_origin() == b.hotkey_origin()) {
                return a.hotkey_name() < b.hotkey_name();
            }
            return a.hotkey_origin() < b.hotkey_origin();
        });

    // Build a json dict according to the requirements of JsonTreeModel
    utility::JsonStore model(nlohmann::json::parse(R"({"children":[]})"));
    auto &children = model["children"];

    for (const auto &hk : filtered_hotkeys_data_) {

        std::string clash_warning;
        for (const auto &other_hk : hotkeys_data_) {
            if (other_hk.key_id() == hk.key_id() && other_hk.modifiers() == hk.modifiers() &&
                other_hk.uuid() != hk.uuid()) {
                clash_warning = fmt::format(
                    "clashes with hotkey '{}'",
                    other_hk.hotkey_name(),
                    other_hk.hotkey_sequence());
            }
        }

        const auto _regular_keys         = regular_keys(hk, false);
        const auto _modifier_keys        = modifier_keys(hk, false);
        const auto default_keys          = regular_keys(hk, true);
        const auto default_modifier_keys = modifier_keys(hk, true);

        nlohmann::json hk_data;
        hk_data["keyboardKey"]         = _regular_keys;
        hk_data["keyModifiers"]        = _modifier_keys;
        hk_data["hotkeyName"]          = hk.hotkey_name();
        hk_data["hotkeyCategory"]      = hk.hotkey_origin();
        hk_data["hotkeyDescription"]   = hk.hotkey_description();
        hk_data["hotkeySequence"]      = hk.hotkey_sequence();
        hk_data["hotkeyUuid"]          = hk.uuid();
        hk_data["defaultKeyboardKey"]  = default_keys;
        hk_data["defaultKeyModifiers"] = default_modifier_keys;
        hk_data["clashWarning"]        = clash_warning;

        bool found = false;
        for (auto &c : children) {
            if (c["hotkeyCategory"] == hk.hotkey_origin()) {
                c["children"].push_back(hk_data);
                found = true;
                break;
            }
        }
        if (!found) {
            nlohmann::json cat_data;
            cat_data["hotkeyCategory"] = hk.hotkey_origin();
            cat_data["children"]       = nlohmann::json::array({hk_data});
            children.push_back(cat_data);
        }
    }
    setModelData(model);
    checkCategories();
}

void HotkeysUI::checkCategories() {
    QSet<QString> cats;
    for (const auto &hk : filtered_hotkeys_data_) {
        cats.insert(QStringFromStd(hk.hotkey_origin()));
    }
    if (cats.values() != categories_) {
        categories_ = cats.values();
        categories_.sort();
        emit categoriesChanged();
    }
}

QString HotkeysUI::hotkey_sequence(const QVariant &hotkey_uuid) {
    QString result;
    utility::Uuid hk_uuid;
    if (hotkey_uuid.canConvert<QUuid>()) {
        hk_uuid = UuidFromQUuid(hotkey_uuid.value<QUuid>());
    } else {
        hk_uuid.from_string(StdFromQString(hotkey_uuid.toString()));
    }
    for (const auto &hk : hotkeys_data_) {
        if (hk.uuid() == hk_uuid) {
            result = QStringFromStd(hk.hotkey_sequence());
            break;
        }
    }
    return result;
}

QString HotkeysUI::hotkey_sequence_from_hotkey_name(const QString &hotkey_name) {
    QString result;
    const std::string nm(StdFromQString(hotkey_name));
    for (const auto &hk : hotkeys_data_) {
        if (hk.hotkey_name() == nm) {
            result = QStringFromStd(hk.hotkey_sequence());
            break;
        }
    }
    return result;
}

void HotkeysUI::setTestHotkeyID(const QString &test_hotkey_id) {
    if (test_hotkey_id != test_hotkey_id_) {
        test_hotkey_id_ = test_hotkey_id;
        emit testHotkeyIDChanged();
    }
}

QVariant HotkeysUI::assignTestSequence() {

    // Assign the currenbt test_sequence_ to the hotkey with uuid test_hotkey_id_ and return
    // any success/error message.
    QVariant result;
    try {

        std::vector<std::string> sequence;
        if (test_sequence_.canConvert<QStringList>()) {
            QStringList seq = test_sequence_.value<QStringList>();
            for (const auto &s : seq) {
                sequence.push_back(StdFromQString(s));
            }
        } else {
            qDebug() << "Invalid test sequence variant passed to assignTestSequence: "
                     << test_sequence_;
            throw std::runtime_error("Invalid test sequence");
        }

        auto hotkey_to_be_changed_id = UuidFromQUuid(QUuid(test_hotkey_id_));

        int keycode  = 0;
        int modifier = 0;
        Hotkey::sequence_to_key_and_modifier(sequence, keycode, modifier);
        caf::scoped_actor sys{system()};
        utility::request_receive<Hotkey>(
            *sys,
            keyboard_manager_,
            keypress_monitor::register_hotkey_atom_v,
            hotkey_to_be_changed_id,
            keycode,
            modifier);

        result = true;

    } catch (const std::exception &err) {
        result = QString("Error assigning hotkey sequence: " + QStringFromStd(err.what()));
    }

    return result;
}

void HotkeysUI::setTestSequence(const QVariant &test_sequence) {

    if (test_sequence_ != test_sequence) {
        test_sequence_ = test_sequence;
        emit testSequenceChanged();
    }

    // test if a key sequence doesn't clash with an existing hotkey.
    try {
        QString status;
        if (test_sequence.canConvert<QStringList>()) {
            QStringList seq = test_sequence.value<QStringList>();
            std::vector<std::string> sequence;
            for (const auto &s : seq) {
                sequence.push_back(StdFromQString(s));
            }
            if (sequence.empty() || (sequence.size() == 1 && sequence[0] == "")) {
                if (test_sequence_status_ != "OK") {
                    test_sequence_status_ = "OK";
                    emit testSequenceStatusChanged();
                }
                return;
            }

            caf::scoped_actor sys{system()};

            utility::Uuid oi(StdFromQString(test_hotkey_id_));

            sys->mail(
                   keypress_monitor::key_down_atom_v,
                   sequence,
                   utility::Uuid(StdFromQString(test_hotkey_id_)))
                .request(keyboard_manager_, std::chrono::seconds(1))
                .receive(
                    [=](const std::string &clashing_hotkey_name) {
                        const QString old_status = test_sequence_status_;
                        if (clashing_hotkey_name == "self") {
                            test_sequence_status_ = QStringFromStd("No Change");
                        } else if (!clashing_hotkey_name.empty()) {
                            test_sequence_status_ = QStringFromStd(
                                fmt::format(
                                    "Clashes with hotkey '{}', which will be unbound if you "
                                    "hit "
                                    "assign.",
                                    clashing_hotkey_name));
                        } else {
                            test_sequence_status_ = "OK";
                        }
                        if (test_sequence_status_ != old_status) {
                            emit testSequenceStatusChanged();
                        }
                    },
                    [=](const error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

HotkeyUI::HotkeyUI(QQuickItem *parent) : QQuickItemActor(parent) {
    init(CafSystemObject::get_actor_system());
}

void HotkeyUI::init(actor_system &system_) {

    QQuickItemActor::init(system_);

    scoped_actor sys{system()};

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](keypress_monitor::hotkey_event_atom,
                const utility::Uuid &uuid,
                const bool hotkey_pressed,
                const std::string &context,
                const std::string &window,
                const bool due_to_focus_change) {
                // if the hotkey was pressed in a different parent window to
                // the parent of this XsHotkey item, we don't activate

                if (hotkey_pressed && StdFromQString(window_name_) != window)
                    return;

                if (hotkey_uuid_ == QUuidFromUuid(uuid)) {
                    if (context_.isNull() || context_ == QString("any") ||
                        StdFromQString(context_) == context) {

                        if (hotkey_pressed)
                            emit activated(QStringFromStd(context));
                        else
                            emit released(QStringFromStd(context));
                    }
                }
            }

        };
    });

    emit componentNameChanged(); // for default componentName ('xStudio')
    emit autoRepeatChanged();    // for default componentName ('xStudio')
    emit uuidChanged();
}

void HotkeyUI::componentComplete() {
    QQuickItem::componentComplete();

    registerHotkey();

    // not sure if this is needed.
    QObject::connect(this, &HotkeyUI::sequenceChanged, this, &HotkeyUI::registerHotkey);
    QObject::connect(this, &HotkeyUI::nameChanged, this, &HotkeyUI::registerHotkey);
    QObject::connect(this, &HotkeyUI::componentNameChanged, this, &HotkeyUI::registerHotkey);
    QObject::connect(this, &HotkeyUI::descriptionChanged, this, &HotkeyUI::registerHotkey);
    QObject::connect(this, &HotkeyUI::contextChanged, this, &HotkeyUI::registerHotkey);
}


void HotkeyUI::registerHotkey() {

    // if (sequence_.isNull() || name_.isNull() || component_name_.isNull() ||
    // context_.isNull()) {
    //     // not ready, some properties not set (yet)
    //     return;
    // }
    int key = 0;
    int mod = 0;

    if (QKeySequence seq(sequence_); seq.count() == 1) {
        mod = 0;
        if ((seq[0].toCombined() & Qt::ShiftModifier) == Qt::ShiftModifier) {
            mod |= KeyboardModifier::ShiftModifier;
        }
        if ((seq[0].toCombined() & Qt::MetaModifier) == Qt::MetaModifier) {
            mod |= KeyboardModifier::MetaModifier;
        }
        if ((seq[0].toCombined() & Qt::ControlModifier) == Qt::ControlModifier) {
            mod |= KeyboardModifier::ControlModifier;
        }
        if ((seq[0].toCombined() & Qt::AltModifier) == Qt::AltModifier) {
            mod |= KeyboardModifier::AltModifier;
        }

        key = seq[0].toCombined() & 0x01ffffff;
    }

    auto keypress_event_manager =
        self()->home_system().registry().template get<caf::actor>(keyboard_events);

    window_name_ = item_window_name(parent());

    Hotkey hk(
        key,
        mod,
        StdFromQString(name_),
        StdFromQString(component_name_),
        StdFromQString(description_),
        StdFromQString(window_name_),
        autorepeat_,
        caf::actor_cast<caf::actor_addr>(as_actor()));

    hotkey_uuid_ = QUuidFromUuid(hk.uuid());

    emit uuidChanged();

    anon_mail(ui::keypress_monitor::register_hotkey_atom_v, hk).send(keypress_event_manager);
}

HotkeyReferenceUI::HotkeyReferenceUI(QObject *parent) : QMLActor(parent) {
    init(CafSystemObject::get_actor_system());
}

HotkeyReferenceUI::~HotkeyReferenceUI() {

    if (exclusive_) {
        exclusive_ = false;
        notifyExclusiveChanged();
    }
}

void HotkeyReferenceUI::init(caf::actor_system &system_) {

    QMLActor::init(system_);
    scoped_actor sys{system()};

    try {

        auto keyboard_manager = system().registry().template get<caf::actor>(keyboard_events);

        auto hotkeys_config_events_group = utility::request_receive<caf::actor>(
            *sys,
            keyboard_manager,
            utility::get_event_group_atom_v,
            keypress_monitor::hotkey_event_atom_v);

        anon_mail(broadcast::join_broadcast_atom_v, as_actor())
            .send(hotkeys_config_events_group);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](keypress_monitor::hotkey_event_atom, const std::vector<Hotkey> &hotkeys) {
                // hotkeys have been updated
                for (const auto &hk : hotkeys) {
                    if (hk.uuid() == hotkey_.uuid()) {
                        if (hk.hotkey_sequence() != hotkey_.hotkey_sequence()) {
                            hotkey_ = hk;
                            Q_EMIT sequenceChanged();
                            Q_EMIT keyChanged();
                            Q_EMIT modifiersChanged();
                        }
                    }
                }
            },
            [=](keypress_monitor::hotkey_event_atom, Hotkey &hotkey) {
                // a hotkey has changed
                if (hotkey.uuid() == hotkey_.uuid()) {
                    if (hotkey.hotkey_sequence() != hotkey_.hotkey_sequence()) {
                        hotkey_ = hotkey;
                        Q_EMIT sequenceChanged();
                        Q_EMIT keyChanged();
                        Q_EMIT modifiersChanged();
                    }
                }
            },
            [=](keypress_monitor::hotkey_event_atom,
                const std::vector<std::string> & /*pressed_keys*/,
                const std::vector<std::string> & /*modifiers*/) {},
            [=](keypress_monitor::hotkey_event_atom,
                const utility::Uuid hotkey_uuid,
                const bool pressed,
                const std::string &context,
                const std::string &window,
                const bool due_to_focus_change) {
                // actual hotkey pressed or release ... we ignore
                if (pressed && hotkey_uuid == hotkey_.uuid() &&
                    (exclusive_ || hotkey_.hotkey_origin() == "any" ||
                     hotkey_.hotkey_origin() == context)) {
                    activated(QStringFromStd(context));
                }
            }};
    });
}

void HotkeyReferenceUI::setHotkeyName(const QString &name) {

    if (hotkeyName() == name)
        return;

    try {

        scoped_actor sys{system()};

        auto keyboard_manager = system().registry().template get<caf::actor>(keyboard_events);

        hotkey_ = request_receive<Hotkey>(
            *sys, keyboard_manager, ui::keypress_monitor::hotkey_atom_v, StdFromQString(name));

        emit sequenceChanged();
        emit hotkeyNameChanged();
        emit uuidChanged();
        emit contextChanged();
        emit exclusiveChanged();
        emit descriptionChanged();
        emit keyChanged();
        emit modifiersChanged();
        emit defaultKeyChanged();
        emit defaultModifiersChanged();

    } catch (const std::exception &err) {
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void HotkeyReferenceUI::setHotkeyUUID(const QUuid &uuid) {

    if (QUuidFromUuid(hotkey_.uuid()) == uuid)
        return;

    try {

        scoped_actor sys{system()};

        auto keyboard_manager = system().registry().template get<caf::actor>(keyboard_events);

        hotkey_ = request_receive<Hotkey>(
            *sys, keyboard_manager, ui::keypress_monitor::hotkey_atom_v, UuidFromQUuid(uuid));

        emit sequenceChanged();
        emit hotkeyNameChanged();
        emit uuidChanged();
        emit contextChanged();
        emit exclusiveChanged();
        emit descriptionChanged();
        emit keyChanged();
        emit modifiersChanged();
        emit defaultKeyChanged();
        emit defaultModifiersChanged();

    } catch (const std::exception &err) {
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}


void HotkeyReferenceUI::setExclusive(const bool exclusive) {

    if (exclusive == exclusive_)
        return;
    exclusive_ = exclusive;
    emit exclusiveChanged();
    notifyExclusiveChanged();
}

void HotkeyReferenceUI::notifyExclusiveChanged() {

    auto keyboard_manager = system().registry().template get<caf::actor>(keyboard_events);
    anon_mail(keypress_monitor::watch_hotkey_atom_v, hotkey_.uuid(), as_actor(), exclusive_)
        .send(keyboard_manager);
}

QVariant HotkeyReferenceUI::key() const {
    return json_to_qvariant(regular_keys(hotkey_, false));
}

QVariant HotkeyReferenceUI::modifiers() const {
    return json_to_qvariant(modifier_keys(hotkey_, false));
}

QVariant HotkeyReferenceUI::defaultKey() const {
    return json_to_qvariant(regular_keys(hotkey_, true));
}

QVariant HotkeyReferenceUI::defaultModifiers() const {
    return json_to_qvariant(modifier_keys(hotkey_, true));
}

PressedKeysMonitor::PressedKeysMonitor(QObject *parent) : QMLActor(parent) {
    init(CafSystemObject::get_actor_system());
}

void PressedKeysMonitor::init(caf::actor_system &system_) {

    QMLActor::init(system_);
    scoped_actor sys{system()};

    try {

        auto keyboard_manager = system().registry().template get<caf::actor>(keyboard_events);

        auto hotkeys_config_events_group = utility::request_receive<caf::actor>(
            *sys,
            keyboard_manager,
            utility::get_event_group_atom_v,
            keypress_monitor::hotkey_event_atom_v);

        anon_mail(broadcast::join_broadcast_atom_v, as_actor())
            .send(hotkeys_config_events_group);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](keypress_monitor::hotkey_event_atom, const std::vector<Hotkey> &hotkeys) {},
            [=](keypress_monitor::hotkey_event_atom, Hotkey &hotkey) {},
            [=](keypress_monitor::hotkey_event_atom,
                const std::vector<std::string> &pressed_keys,
                const std::vector<std::string> &modifiers) {
                QStringList new_pressed_keys;
                QStringList new_modifiers;
                for (const auto &k : pressed_keys)
                    new_pressed_keys.append(QStringFromStd(k));
                for (const auto &k : modifiers)
                    new_modifiers.append(QStringFromStd(k));
                if (pressed_keys_ != new_pressed_keys) {
                    pressed_keys_ = new_pressed_keys;
                    emit pressedKeysChanged();
                }
                if (modifiers_ != new_modifiers) {
                    modifiers_ = new_modifiers;
                    emit modifiersChanged();
                }
            },
            [=](keypress_monitor::hotkey_event_atom,
                const utility::Uuid kotkey_uuid,
                const bool pressed,
                const std::string &context,
                const std::string &window,
                const bool due_to_focus_change) {}};
    });
}
