// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include <fstream>
#include <filesystem>
#include <caf/actor_registry.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/hotkey_ui.hpp"
#include "xstudio/utility/json_store.hpp"

#ifdef _WIN32
#include <ShlObj.h>
#endif

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;
using namespace xstudio::ui;

HotkeysUI::HotkeysUI(QObject *parent) : super(parent) {
    init(CafSystemObject::get_actor_system());

    try {

        scoped_actor sys{system()};

        auto keyboard_manager = system().registry().template get<caf::actor>(keyboard_events);

        auto hotkeys_config_events_group = utility::request_receive<caf::actor>(
            *sys,
            keyboard_manager,
            utility::get_event_group_atom_v,
            keypress_monitor::hotkey_event_atom_v);

        request_receive<bool>(
            *sys, hotkeys_config_events_group, broadcast::join_broadcast_atom_v, as_actor());

        auto hotkeys = request_receive<std::vector<Hotkey>>(
            *sys, keyboard_manager, keypress_monitor::register_hotkey_atom_v);

        update_hotkeys_model_data(hotkeys);

        // Capture defaults before applying overrides
        for (const auto &hk : hotkeys_data_) {
            default_sequences_[hk.hotkey_name()] = hk.hotkey_sequence();
        }
        defaults_captured_ = true;

        loadHotkeyOverrides();

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](keypress_monitor::hotkey_event_atom, const std::vector<Hotkey> &hotkeys) {
                update_hotkeys_model_data(hotkeys);
            },
            [=](keypress_monitor::hotkey_event_atom, const std::string & /*pressed_keys*/) {},
            [=](keypress_monitor::hotkey_event_atom, Hotkey & /*hotkey*/) {
                // update_hotkeys_model_data(hotkeys);
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

void HotkeysUI::update_hotkeys_model_data(const std::vector<Hotkey> &new_hotkeys_data) {

    hotkeys_data_ = new_hotkeys_data;

    // sort by the name of the hotkey
    std::sort(
        hotkeys_data_.begin(),
        hotkeys_data_.end(),
        [](const Hotkey &a, const Hotkey &b) -> bool {
            return a.hotkey_name() < b.hotkey_name();
        });

    beginResetModel();
    endResetModel();
    emit rowCountChanged();
    checkCategories();
}

void HotkeysUI::checkCategories() {
    QSet<QString> cats;
    for (const auto &hk : hotkeys_data_) {
        cats.insert(QStringFromStd(hk.hotkey_origin()));
    }
    if (cats.values() != categories_) {
        categories_ = cats.values();
        emit categoriesChanged();
    }
}

QHash<int, QByteArray> HotkeysUI::roleNames() const {
    QHash<int, QByteArray> roles;
    for (auto i = hotkeyRoleNames.keyValueBegin(); i != hotkeyRoleNames.keyValueEnd(); ++i) {
        roles[i->first] = i->second;
    }
    return roles;
}

int HotkeysUI::rowCount(const QModelIndex &) const {
    int ct = 0;
    const std::string curr_cat(StdFromQString(current_category_));
    for (const auto &hk : hotkeys_data_) {
        if (hk.hotkey_origin() == curr_cat) {
            ct++;
        }
    }
    return ct;
}

QVariant HotkeysUI::data(const QModelIndex &index, int role) const {

    QVariant rt;

    try {
        int ct = 0;
        const std::string curr_cat(StdFromQString(current_category_));
        for (const auto &hk : hotkeys_data_) {
            if (hk.hotkey_origin() == curr_cat) {
                if (ct == index.row()) {

                    if (role == keyboardKey)
                        rt = QStringFromStd(hk.key());
                    if (role == keyModifiers)
                        rt = hk.modifiers();
                    if (role == hotkeyName)
                        rt = QStringFromStd(hk.hotkey_name());
                    if (role == hotkeyCategory)
                        rt = QStringFromStd(hk.hotkey_origin());
                    if (role == hotkeyDescription)
                        rt = QStringFromStd(hk.hotkey_description());
                    if (role == hotkeySequence)
                        rt = QStringFromStd(hk.hotkey_sequence());
                    break;
                }
                ct++;
            }
        }
    } catch (const std::exception &) {
    }

    return rt;
}

bool HotkeysUI::setData(const QModelIndex &index, const QVariant &value, int role) {

    if (role == hotkeySequence) {
        return rebindHotkey(index.row(), value.toString());
    }

    return false;
}

const Hotkey *HotkeysUI::hotkeyAtRow(int row) const {
    int ct = 0;
    const std::string curr_cat(StdFromQString(current_category_));
    for (const auto &hk : hotkeys_data_) {
        if (hk.hotkey_origin() == curr_cat) {
            if (ct == row)
                return &hk;
            ct++;
        }
    }
    return nullptr;
}

Hotkey *HotkeysUI::hotkeyAtRow(int row) {
    int ct = 0;
    const std::string curr_cat(StdFromQString(current_category_));
    for (auto &hk : hotkeys_data_) {
        if (hk.hotkey_origin() == curr_cat) {
            if (ct == row)
                return &hk;
            ct++;
        }
    }
    return nullptr;
}

QString HotkeysUI::hotkeyNameAtRow(int model_row) const {
    const auto *hk = hotkeyAtRow(model_row);
    return hk ? QStringFromStd(hk->hotkey_name()) : QString();
}

QString HotkeysUI::hotkeySequenceAtRow(int model_row) const {
    const auto *hk = hotkeyAtRow(model_row);
    return hk ? QStringFromStd(hk->hotkey_sequence()) : QString();
}

QString HotkeysUI::hotkeyDescriptionAtRow(int model_row) const {
    const auto *hk = hotkeyAtRow(model_row);
    return hk ? QStringFromStd(hk->hotkey_description()) : QString();
}

QString HotkeysUI::checkConflict(int model_row, const QString &new_sequence) {
    int new_key = 0, new_mod = 0;
    Hotkey::sequence_to_key_and_modifier(StdFromQString(new_sequence), new_key, new_mod);
    if (new_key == 0)
        return QString();

    const auto *target = hotkeyAtRow(model_row);
    if (!target)
        return QString();

    for (const auto &hk : hotkeys_data_) {
        if (hk.uuid() == target->uuid())
            continue;
        if (hk.modifiers() == new_mod) {
            // Compare key codes - need to get the key code from the hotkey
            int hk_key = 0, hk_mod = 0;
            Hotkey::sequence_to_key_and_modifier(hk.hotkey_sequence(), hk_key, hk_mod);
            if (hk_key == new_key) {
                return QStringFromStd(hk.hotkey_name());
            }
        }
    }
    return QString();
}

bool HotkeysUI::rebindHotkey(int model_row, const QString &new_sequence) {
    auto *hk = hotkeyAtRow(model_row);
    if (!hk)
        return false;

    int new_key = 0, new_mod = 0;
    Hotkey::sequence_to_key_and_modifier(StdFromQString(new_sequence), new_key, new_mod);
    if (new_key == 0)
        return false;

    // Capture defaults on first rebind
    if (!defaults_captured_) {
        for (const auto &h : hotkeys_data_) {
            default_sequences_[h.hotkey_name()] = h.hotkey_sequence();
        }
        defaults_captured_ = true;
    }

    try {
        auto keyboard_manager = system().registry().template get<caf::actor>(keyboard_events);

        // Create a new Hotkey with the updated key/modifiers
        Hotkey new_hk(
            new_key,
            new_mod,
            hk->hotkey_name(),
            hk->hotkey_origin(),
            hk->hotkey_description(),
            "",     // window_name
            false,  // auto_repeat
            caf::actor_addr(),  // no watcher - existing watchers preserved by KeypressMonitor
            hk->uuid());

        // Send to KeypressMonitor which will call update() on existing hotkey
        anon_mail(keypress_monitor::register_hotkey_atom_v, new_hk)
            .send(keyboard_manager);

        // Record the override immediately so saveHotkeyOverrides() can see
        // it - the async broadcast from KeypressMonitor hasn't updated
        // hotkeys_data_ yet at this point.
        pending_overrides_[hk->hotkey_name()] = StdFromQString(new_sequence);

        saveHotkeyOverrides();

        return true;
    } catch (const std::exception &err) {
        spdlog::warn("rebindHotkey failed: {}", err.what());
    }
    return false;
}

void HotkeysUI::resetHotkey(int model_row) {
    const auto *hk = hotkeyAtRow(model_row);
    if (!hk)
        return;

    auto it = default_sequences_.find(hk->hotkey_name());
    if (it != default_sequences_.end()) {
        rebindHotkey(model_row, QStringFromStd(it->second));
    }
}

void HotkeysUI::resetAllHotkeys() {
    for (auto &[name, seq] : default_sequences_) {
        // Find this hotkey in the current data and rebind
        int ct = 0;
        const std::string curr_cat(StdFromQString(current_category_));
        for (auto &hk : hotkeys_data_) {
            if (hk.hotkey_origin() == curr_cat) {
                if (hk.hotkey_name() == name) {
                    rebindHotkey(ct, QStringFromStd(seq));
                }
                ct++;
            }
        }
    }
}

std::string HotkeysUI::hotkey_overrides_path() {
    std::string dir;
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        dir = std::string(path) + "\\xstudio";
    } else {
        dir = ".";
    }
#else
    const char *home = getenv("HOME");
    dir = home ? std::string(home) + "/.config/xstudio" : ".";
#endif
    std::filesystem::create_directories(dir);
    return dir + "/hotkey_overrides.json";
}

void HotkeysUI::saveHotkeyOverrides() {
    try {
        nlohmann::json overrides = nlohmann::json::object();

        // Build overrides from pending_overrides_ which tracks rebinds
        // immediately, rather than from hotkeys_data_ which is only updated
        // asynchronously after the KeypressMonitor broadcast.
        for (const auto &[name, seq] : pending_overrides_) {
            auto it = default_sequences_.find(name);
            if (it != default_sequences_.end() && it->second != seq) {
                overrides[name] = seq;
            }
        }

        std::ofstream ofs(hotkey_overrides_path());
        if (ofs.is_open()) {
            ofs << overrides.dump(2);
            spdlog::info("Saved hotkey overrides to {}", hotkey_overrides_path());
        }
    } catch (const std::exception &err) {
        spdlog::warn("Failed to save hotkey overrides: {}", err.what());
    }
}

void HotkeysUI::loadHotkeyOverrides() {
    try {
        std::ifstream ifs(hotkey_overrides_path());
        if (!ifs.is_open())
            return;

        nlohmann::json overrides;
        ifs >> overrides;

        auto keyboard_manager = system().registry().template get<caf::actor>(keyboard_events);

        for (auto it = overrides.begin(); it != overrides.end(); ++it) {
            const std::string &name = it.key();
            const std::string &seq = it.value().get<std::string>();

            // Find this hotkey
            for (auto &hk : hotkeys_data_) {
                if (hk.hotkey_name() == name) {
                    int new_key = 0, new_mod = 0;
                    Hotkey::sequence_to_key_and_modifier(seq, new_key, new_mod);
                    if (new_key != 0) {
                        Hotkey new_hk(
                            new_key, new_mod,
                            hk.hotkey_name(), hk.hotkey_origin(), hk.hotkey_description(),
                            "", false, caf::actor_addr(), hk.uuid());
                        anon_mail(keypress_monitor::register_hotkey_atom_v, new_hk)
                            .send(keyboard_manager);
                    }
                    break;
                }
            }
        }

        // Populate pending_overrides_ so subsequent saves include loaded
        // overrides that haven't been touched this session.
        for (auto it = overrides.begin(); it != overrides.end(); ++it) {
            pending_overrides_[it.key()] = it.value().get<std::string>();
        }

        spdlog::info("Loaded {} hotkey overrides from {}", overrides.size(), hotkey_overrides_path());
    } catch (const std::exception &err) {
        spdlog::debug("No hotkey overrides loaded: {}", err.what());
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

HotkeyUI::HotkeyUI(QObject *parent) : QMLActor(parent) {
    init(CafSystemObject::get_actor_system());
}

void HotkeyUI::init(actor_system &system_) {

    QMLActor::init(system_);

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
                            emit activated();
                        else
                            emit released();
                    }
                }
            }

        };
    });

    QObject::connect(this, &HotkeyUI::sequenceChanged, this, &HotkeyUI::registerHotkey);
    QObject::connect(this, &HotkeyUI::nameChanged, this, &HotkeyUI::registerHotkey);
    QObject::connect(this, &HotkeyUI::componentNameChanged, this, &HotkeyUI::registerHotkey);
    QObject::connect(this, &HotkeyUI::descriptionChanged, this, &HotkeyUI::registerHotkey);
    QObject::connect(this, &HotkeyUI::contextChanged, this, &HotkeyUI::registerHotkey);

    emit componentNameChanged(); // for default componentName ('xStudio')
    emit autoRepeatChanged();    // for default componentName ('xStudio')
    emit uuidChanged();
}

void HotkeyUI::registerHotkey() {

    if (sequence_.isNull() || name_.isNull() || component_name_.isNull() || context_.isNull()) {
        // not ready, some properties not set (yet)
        return;
    }

    QKeySequence seq(sequence_);

    if (seq.count() == 1) {
        int mod = 0;
        if ((seq[0] & Qt::ShiftModifier) == Qt::ShiftModifier) {
            mod |= KeyboardModifier::ShiftModifier;
        }
        if ((seq[0] & Qt::MetaModifier) == Qt::MetaModifier) {
            mod |= KeyboardModifier::MetaModifier;
        }
        if ((seq[0] & Qt::ControlModifier) == Qt::ControlModifier) {
            mod |= KeyboardModifier::ControlModifier;
        }
        if ((seq[0] & Qt::AltModifier) == Qt::AltModifier) {
            mod |= KeyboardModifier::AltModifier;
        }

        int key = seq[0] & 0x01ffffff;

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

        anon_mail(ui::keypress_monitor::register_hotkey_atom_v, hk)
            .send(keypress_event_manager);

    } else {
    }
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
                    if (hk.hotkey_name() == StdFromQString(hotkey_name_)) {
                        if (QStringFromStd(hk.hotkey_sequence()) != sequence_) {
                            sequence_ = QStringFromStd(hk.hotkey_sequence());
                            Q_EMIT sequenceChanged();
                        }
                        QUuid uuid = QUuidFromUuid(hk.uuid());
                        if (uuid != hotkey_uuid_) {
                            hotkey_uuid_ = uuid;
                            Q_EMIT uuidChanged();
                            if (exclusive_)
                                notifyExclusiveChanged();
                        }
                    }
                }
            },
            [=](keypress_monitor::hotkey_event_atom, Hotkey &hotkey) {
                // a hotkey has changed
                if (hotkey.hotkey_name() == StdFromQString(hotkey_name_)) {
                    if (QStringFromStd(hotkey.hotkey_sequence()) != sequence_) {
                        sequence_ = QStringFromStd(hotkey.hotkey_sequence());
                        Q_EMIT sequenceChanged();
                    }
                    QUuid uuid = QUuidFromUuid(hotkey.uuid());
                    if (uuid != hotkey_uuid_) {
                        hotkey_uuid_ = uuid;
                        Q_EMIT uuidChanged();
                        if (exclusive_)
                            notifyExclusiveChanged();
                    }
                }
            },
            [=](keypress_monitor::hotkey_event_atom, const std::string & /*pressed_keys*/) {},
            [=](keypress_monitor::hotkey_event_atom,
                const utility::Uuid kotkey_uuid,
                const bool pressed,
                const std::string &context,
                const std::string &window,
                const bool due_to_focus_change) {
                // actual hotkey pressed or release ... we ignore
                if (pressed && QUuidFromUuid(kotkey_uuid) == hotkey_uuid_ &&
                    (context_.empty() || context_ == context)) {
                    activated(QStringFromStd(context));
                }
            }};
    });
}

void HotkeyReferenceUI::setHotkeyName(const QString &name) {

    if (hotkey_name_ == name)
        return;
    hotkey_name_ = name;
    Q_EMIT hotkeyNameChanged();

    try {

        scoped_actor sys{system()};

        auto keyboard_manager = system().registry().template get<caf::actor>(keyboard_events);

        auto hotkeys_config_events_group = utility::request_receive<caf::actor>(
            *sys,
            keyboard_manager,
            utility::get_event_group_atom_v,
            keypress_monitor::hotkey_event_atom_v);

        const auto hk = request_receive<Hotkey>(
            *sys, keyboard_manager, ui::keypress_monitor::hotkey_atom_v, StdFromQString(name));


        QString seq = QStringFromStd(hk.hotkey_sequence());

        if (seq != sequence_) {
            sequence_ = seq;
            Q_EMIT sequenceChanged();
        }

        QUuid uuid = QUuidFromUuid(hk.uuid());
        if (uuid != hotkey_uuid_) {
            hotkey_uuid_ = uuid;
            Q_EMIT uuidChanged();
            if (exclusive_)
                notifyExclusiveChanged();
        }


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
    anon_mail(
        keypress_monitor::watch_hotkey_atom_v,
        UuidFromQUuid(hotkey_uuid_),
        as_actor(),
        exclusive_)
        .send(keyboard_manager);
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
            [=](keypress_monitor::hotkey_event_atom, const std::string &pressed_keys) {
                pressed_keys_ = QStringFromStd(pressed_keys);
                emit pressedKeysChanged();
            },
            [=](keypress_monitor::hotkey_event_atom,
                const utility::Uuid kotkey_uuid,
                const bool pressed,
                const std::string &context,
                const std::string &window,
                const bool due_to_focus_change) {}};
    });
}
