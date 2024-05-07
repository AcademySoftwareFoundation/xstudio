// SPDX-License-Identifier: Apache-2.0
#pragma once

#ifndef HELPER_QML_EXPORT_H
#define HELPER_QML_EXPORT_H

#ifdef HELPER_QML_STATIC_DEFINE
#define HELPER_QML_EXPORT
#define HELPER_QML_NO_EXPORT
#else
#ifndef HELPER_QML_EXPORT
#ifdef helper_qml_EXPORTS
/* We are building this library */
#define HELPER_QML_EXPORT __declspec(dllexport)
#else
/* We are using this library */
#define HELPER_QML_EXPORT __declspec(dllimport)
#endif
#endif

#ifndef HELPER_QML_NO_EXPORT
#define HELPER_QML_NO_EXPORT
#endif
#endif

#ifndef HELPER_QML_DEPRECATED
#define HELPER_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef HELPER_QML_DEPRECATED_EXPORT
#define HELPER_QML_DEPRECATED_EXPORT HELPER_QML_EXPORT HELPER_QML_DEPRECATED
#endif

#ifndef HELPER_QML_DEPRECATED_NO_EXPORT
#define HELPER_QML_DEPRECATED_NO_EXPORT HELPER_QML_NO_EXPORT HELPER_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#ifndef HELPER_QML_NO_DEPRECATED
#define HELPER_QML_NO_DEPRECATED
#endif
#endif

#endif /* HELPER_QML_EXPORT_H */

#include <caf/all.hpp>

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/ui/qml/tag_ui.hpp"


CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
CAF_POP_WARNINGS

namespace xstudio::ui::qml {
using namespace caf;

class HELPER_QML_EXPORT UIModelData : public caf::mixin::actor_object<JSONTreeModel> {

    Q_OBJECT

    Q_PROPERTY(QString modelDataName READ modelDataName WRITE setModelDataName NOTIFY
                   modelDataNameChanged)

  public:
    using super = caf::mixin::actor_object<JSONTreeModel>;

    explicit UIModelData(
        QObject *parent,
        const std::string &model_name,
        const std::string &data_preference_path);

    explicit UIModelData(QObject *parent);

    virtual void init(caf::actor_system &system);

    const QString modelDataName() const { return QStringFromStd(model_name_); }

    Q_INVOKABLE void dump() const {
        spdlog::warn("Banglesod {}", utility::tree_to_json(data_, "children").dump(2));
    }

  signals:

    void modelDataNameChanged();

  public slots:

    void nodeActivated(const QModelIndex &idx);
    void setModelDataName(QString name);

  private slots:

  public:
    caf::actor_system &system() { return self()->home_system(); }

    bool
    setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    Q_INVOKABLE bool
    removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    Q_INVOKABLE bool
    removeRowsSync(int row, int count, const QModelIndex &parent = QModelIndex());

    Q_INVOKABLE bool moveRows(
        const QModelIndex &sourceParent,
        int sourceRow,
        int count,
        const QModelIndex &destinationParent,
        int destinationChild) override;
    Q_INVOKABLE bool
    insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    Q_INVOKABLE bool
    insertRowsSync(int row, int count, const QModelIndex &parent = QModelIndex());

  private:
    caf::actor central_models_data_actor_;
    std::string model_name_;
    std::string data_preference_path_;
    bool foobarred_ = {false};
};

class HELPER_QML_EXPORT MenusModelData : public UIModelData {

    Q_OBJECT

  public:
    explicit MenusModelData(QObject *parent = nullptr);
};

class HELPER_QML_EXPORT ViewsModelData : public UIModelData {

    Q_OBJECT

  public:
    explicit ViewsModelData(QObject *parent = nullptr);

  public slots:

    // call this function to register a widget (or view) that can be used to
    // fill an xSTUDIO panel in the interface. See main.qml for examples.
    void register_view(QString qml_source, QString view_name);

    // call this function to retrieve the QML source (or the path to the
    // source .qml file) for the given view
    QVariant view_qml_source(QString view_name);
};

class HELPER_QML_EXPORT ReskinPanelsModel : public UIModelData {

    Q_OBJECT

  public:
    explicit ReskinPanelsModel(QObject *parent = nullptr);

    Q_INVOKABLE void close_panel(QModelIndex panel_index);
    Q_INVOKABLE void split_panel(QModelIndex panel_index, bool horizontal_split);
    Q_INVOKABLE void duplicate_layout(QModelIndex panel_index);
};

class MediaListColumnsModel : public UIModelData {

    Q_OBJECT

  public:
    explicit MediaListColumnsModel(QObject *parent = nullptr);
};

class HELPER_QML_EXPORT MenuModelItem : public caf::mixin::actor_object<QObject> {

    Q_OBJECT

  public:
    using super = caf::mixin::actor_object<QObject>;

    explicit MenuModelItem(QObject *parent = nullptr);

    ~MenuModelItem() override;

    virtual void init(caf::actor_system &system);

    Q_PROPERTY(QString menuPath READ menuPath WRITE setMenuPath NOTIFY menuPathChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(
        QString menuModelName READ menuModelName WRITE setMenuName NOTIFY menuNameChanged)
    Q_PROPERTY(float menuItemPosition READ menuItemPosition WRITE setMenuItemPosition NOTIFY
                   menuItemPositionChanged)
    Q_PROPERTY(QStringList choices READ choices WRITE setChoices NOTIFY choicesChanged)
    Q_PROPERTY(QString currentChoice READ currentChoice WRITE setCurrentChoice NOTIFY
                   currentChoiceChanged)
    Q_PROPERTY(bool isChecked READ isChecked WRITE setIsChecked NOTIFY isCheckedChanged)
    Q_PROPERTY(QString hotkey READ hotkey WRITE setHotkey NOTIFY hotkeyChanged)
    Q_PROPERTY(
        QString menuItemType READ menuItemType WRITE setMenuItemType NOTIFY menuItemTypeChanged)

    const QString &menuPath() const { return menu_path_; }
    const QString &text() const { return text_; }
    const QString &menuModelName() const { return menu_name_; }
    float menuItemPosition() const { return menu_item_position_; }
    const QStringList &choices() const { return choices_; }
    const QString &currentChoice() const { return current_choice_; }
    bool isChecked() const { return is_checked_; }
    const QString &hotkey() const { return hotkey_; }
    const QString &menuItemType() const { return menu_item_type_; }

  public slots:

    void setMenuPath(const QString &path);
    void setText(const QString &text);
    void setMenuName(const QString &menu_name);
    void setMenuItemPosition(const float position) {
        if (position != menu_item_position_) {
            menu_item_position_ = position;
            emit menuItemPositionChanged();
            insertIntoMenuModel();
        }
    }
    void setChoices(const QStringList &choices) {
        if (choices != choices_) {
            choices_ = choices;
            emit choicesChanged();
            insertIntoMenuModel();
        }
    }
    void setCurrentChoice(const QString &currentChoice) {
        if (currentChoice != current_choice_) {
            current_choice_ = currentChoice;
            emit currentChoiceChanged();
            insertIntoMenuModel();
        }
    }
    void setIsChecked(const bool checked) {
        if (checked != is_checked_) {
            is_checked_ = checked;
            emit isCheckedChanged();
            insertIntoMenuModel();
        }
    }
    void setHotkey(const QString &hotkey) {
        if (hotkey != hotkey_) {
            hotkey_ = hotkey;
            emit hotkeyChanged();
            insertIntoMenuModel();
        }
    }
    void setMenuItemType(const QString &menu_item_type) {
        if (menu_item_type != menu_item_type_) {
            menu_item_type_ = menu_item_type;
            emit menuItemTypeChanged();
            insertIntoMenuModel();
        }
    }

  signals:

    void menuPathChanged();
    void menuNameChanged();
    void textChanged();
    void activated();
    void menuItemPositionChanged();
    void choicesChanged();
    void currentChoiceChanged();
    void isCheckedChanged();
    void hotkeyChanged();
    void menuItemTypeChanged();

  private:
    void insertIntoMenuModel();

    QString menu_name_;
    QString menu_path_ = QString("Undefined");
    QString text_;
    QString hotkey_;
    QString current_choice_;
    QString menu_item_type_ = QString("button");
    QStringList choices_;
    bool is_checked_              = {false};
    float menu_item_position_     = -1.0f;
    utility::Uuid model_entry_id_ = utility::Uuid::generate();
    bool dont_update_model_       = {false};
};


} // namespace xstudio::ui::qml