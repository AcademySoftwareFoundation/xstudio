// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"


CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QSortFilterProxyModel>
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
        const std::string &data_preference_path,
        const std::vector<std::string> &role_names = {});

    explicit UIModelData(QObject *parent);

    virtual void init(caf::actor_system &system);

    const QString modelDataName() const { return QStringFromStd(model_name_); }

    Q_INVOKABLE void dump() const {
        spdlog::warn("UIModelData dump {}", utility::tree_to_json(data_, "children").dump(2));
    }

    static void add_context_object_lookup(QObject *obj);
    static QObject *context_object_lookup(const QString &obj_str);


  signals:

    void modelDataNameChanged();

  public slots:

    void nodeActivated(const QModelIndex &idx, const QString &data, QObject *panel);
    void setModelDataName(QString name);
    void restoreDefaults();

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

  protected:
    std::string apply_filter(const std::string &path) const;
    std::string reverse_apply_filter(const std::string &path) const;

    caf::actor central_models_data_actor_;
    std::string model_name_;
    std::string data_preference_path_;
    int filtered_child_idx_ = {-1};
    bool debug_             = {false};
    static std::mutex mutex_;
    static QMap<QString, QObject *> context_object_lookup_map_;
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
    void register_view(QString qml_source, QString view_name, float position);

    // call this function to retrieve the QML source (or the path to the
    // source .qml file) for the given view
    QVariant view_qml_source(QString view_name);
};

class HELPER_QML_EXPORT PopoutWindowsData : public UIModelData {

    Q_OBJECT

  public:
    explicit PopoutWindowsData(QObject *parent = nullptr);

  public slots:

    // call this function to register a widget (or view) that can be shown in
    // a pop-out window via a button in the tool shelf at the top-left of the
    // viewport window.
    void register_popout_window(
        QString name,
        QString qml_source,
        QString icon_path,
        float button_position,
        const QUuid hotkey = QUuid());
};

class HELPER_QML_EXPORT SingletonsModelData : public UIModelData {

    Q_OBJECT

  public:
    explicit SingletonsModelData(QObject *parent = nullptr);

  public slots:

    void register_singleton_qml(const QString &qml_code);
};

class HELPER_QML_EXPORT PanelsModel : public UIModelData {

    Q_OBJECT

  public:
    explicit PanelsModel(QObject *parent = nullptr);

    Q_INVOKABLE void close_panel(QModelIndex panel_index);
    Q_INVOKABLE void split_panel(QModelIndex panel_index, bool horizontal_split);
    Q_INVOKABLE int add_layout(QString layout_name, QModelIndex root, QString layoutType);
    Q_INVOKABLE QModelIndex duplicate_layout(QModelIndex panel_index);
    Q_INVOKABLE void storeFloatingWindowData(QString window_name, QVariant data);
    Q_INVOKABLE QVariant retrieveFloatingWindowData(QString window_name);
};

class HELPER_QML_EXPORT MediaListColumnsModel : public UIModelData {

    Q_OBJECT

  public:
    explicit MediaListColumnsModel(QObject *parent = nullptr);
};

class HELPER_QML_EXPORT MediaListFilterModel : public QSortFilterProxyModel {

    Q_OBJECT

    Q_PROPERTY(
        QString searchString READ searchString WRITE setSearchString NOTIFY searchStringChanged)

  public:
    using super = QSortFilterProxyModel;

    MediaListFilterModel(QObject *parent = nullptr);

    const QString &searchString() const { return search_string_; }

    void setSearchString(const QString &ss) {
        if (ss != search_string_) {
            search_string_ = ss;
            emit searchStringChanged();
            invalidateFilter();
        }
    }

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override {
        if (!sourceModel())
            return QHash<int, QByteArray>();
        return sourceModel()->roleNames();
    }

    Q_INVOKABLE [[nodiscard]] QModelIndex rowToSourceIndex(const int row) const;

    Q_INVOKABLE [[nodiscard]] int sourceIndexToRow(const QModelIndex &) const;

    Q_INVOKABLE [[nodiscard]] int
    getRowWithMatchingRoleData(const QVariant &searchValue, const QString &searchRole) const;

  protected:
    [[nodiscard]] bool
    filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

  signals:
    void searchStringChanged();

  private:
    QString search_string_;
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
    Q_PROPERTY(QUuid hotkeyUuid READ hotkeyUuid WRITE setHotkeyUuid NOTIFY hotkeyUuidChanged)
    Q_PROPERTY(
        QString menuItemType READ menuItemType WRITE setMenuItemType NOTIFY menuItemTypeChanged)
    Q_PROPERTY(QString menuCustomIcon READ menuCustomIcon WRITE setMenuCustomIcon NOTIFY
                   menuCustomIconChanged)
    Q_PROPERTY(QString customMenuQml READ customMenuQml WRITE setCustomMenuQml NOTIFY
                   customMenuQmlChanged)
    Q_PROPERTY(QVariant userData READ userData WRITE setUserData NOTIFY userDataChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QObject *panelContext READ panelContext WRITE setPanelContext NOTIFY
                   panelContextChanged)

    const QString &menuPath() const { return menu_path_; }
    const QString &text() const { return text_; }
    const QString &menuModelName() const { return menu_name_; }
    float menuItemPosition() const { return menu_item_position_; }
    const QStringList &choices() const { return choices_; }
    const QString &currentChoice() const { return current_choice_; }
    bool isChecked() const { return is_checked_; }
    const QString &hotkey() const { return hotkey_; }
    const QString &menuItemType() const { return menu_item_type_; }
    const QUuid &hotkeyUuid() const { return hotkey_uuid_; }
    const QString &menuCustomIcon() const { return menu_custom_icon_; }
    const QString &customMenuQml() const { return custom_menu_qml_; }
    const QVariant &userData() const { return user_data_; }
    bool enabled() const { return enabled_; }
    QObject *panelContext() const { return panel_context_; }

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
    void setHotkeyUuid(const QUuid &hotkey_uuid) {
        if (hotkey_uuid != hotkey_uuid_) {
            hotkey_uuid_ = hotkey_uuid;
            emit hotkeyUuidChanged();
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
    void setMenuCustomIcon(const QString &menu_custom_icon) {
        if (menu_custom_icon != menu_custom_icon_) {
            menu_custom_icon_ = menu_custom_icon;
            emit menuCustomIconChanged();
            insertIntoMenuModel();
        }
    }
    void setCustomMenuQml(const QString &custom_menu_qml) {
        if (custom_menu_qml != custom_menu_qml_) {
            custom_menu_qml_ = custom_menu_qml;
            emit customMenuQmlChanged();
            insertIntoMenuModel();
        }
    }
    void setUserData(const QVariant &user_data) {
        if (user_data != user_data_) {
            user_data_ = user_data;
            emit userDataChanged();
            insertIntoMenuModel();
        }
    }

    void setEnabled(const bool e) {
        if (enabled_ != e) {
            enabled_ = e;
            emit enabledChanged();
            insertIntoMenuModel();
        }
    }

    void setPanelContext(QObject *obj) {
        if (panel_context_ != obj) {
            panel_context_ = obj;
            emit panelContextChanged();
            insertIntoMenuModel();
        }
    }

    void setMenuPathPosition(const QString &menu_path, const QVariant position);

  signals:

    void menuPathChanged();
    void menuNameChanged();
    void textChanged();
    void activated(QObject *menuContext);
    void hotkeyActivated();
    void menuItemPositionChanged();
    void choicesChanged();
    void currentChoiceChanged();
    void isCheckedChanged();
    void menuItemTypeChanged();
    void hotkeyUuidChanged();
    void menuCustomIconChanged();
    void customMenuQmlChanged();
    void userDataChanged();
    void enabledChanged();
    void panelContextChanged();

  private:
    void insertIntoMenuModel();
    QObject *contextPanel();

    QString menu_name_;
    QString menu_path_ = QString("Undefined");
    QString text_;
    QString hotkey_;
    QString current_choice_;
    QString menu_item_type_ = QString("button");
    QString menu_custom_icon_;
    QString custom_menu_qml_;
    QStringList choices_;
    QUuid hotkey_uuid_;
    QVariant user_data_;
    QObject *panel_context_       = {nullptr};
    bool is_checked_              = {false};
    bool enabled_                 = {true};
    float menu_item_position_     = -1.0f;
    utility::Uuid model_entry_id_ = utility::Uuid::generate();
    bool dont_update_model_       = {false};
};

/* In the xSTUDIO interface, we can have multiple instances of the same type
of panel. For example, the media list view might be instanced several times.
Each instance will declare its own 'MenuModelItems' to insert items into the
right click menu model. However, there is only one 'model' that describes what's
in the right click menu... we do it this way because there are other plugins
that might want to insert items into the media list menu and they only want to
deal with one menu model, not separate menu models for each media list view.

Our solution is that a MenuModelItem can declare which panel instance it was
created in. Then, on the creation of the menu itself, we use this filter
to only include menu items that originated from the panel that has created the
menu. The MenuModelItem has a string property panelContext - we set this to
the stringified address of the parent panel. When the corresponding entry is
made in the central menu model this is recorded in the role 'menu_item_context'.
Then when the pop-up menu is built from the central menu model, we check if the
menu item has 'menu_item_context' and only add an entry on the menu *IF* the
menu_item_context matches the stringified address of the parent panel that is
creating the pop-up. simples ;-)*/
class HELPER_QML_EXPORT PanelMenuModelFilter : public QSortFilterProxyModel {

    Q_OBJECT

    Q_PROPERTY(
        QString panelAddress READ panelAddress WRITE setPanelAddress NOTIFY panelAddressChanged)

  public:
    PanelMenuModelFilter(QObject *parent = nullptr);

    const QString &panelAddress() const { return panel_address_; }

    Q_INVOKABLE [[nodiscard]] QVariant
    get(const QModelIndex &item, const QString &role = "display") const {
        if (source_model_) {
            return source_model_->get(mapToSource(item), role);
        }
        return QVariant();
    }

  public slots:

    void setPanelAddress(const QString &panelAddress) {
        if (panelAddress != panel_address_) {
            panel_address_ = panelAddress;
            emit panelAddressChanged();
            invalidateFilter();
        }
    }

    void nodeActivated(const QModelIndex &idx, const QString &data, QObject *panel) {
        source_model_->nodeActivated(mapToSource(idx), data, panel);
    }

  protected:
    [[nodiscard]] bool
    filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

  signals:
    void panelAddressChanged();

  private:
    QString panel_address_;
    int menu_item_context_role_id_ = {0};
    UIModelData *source_model_     = nullptr;
};

} // namespace xstudio::ui::qml