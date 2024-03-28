// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QSortFilterProxyModel>
#include <QUrl>
#include <QFuture>
#include <QtConcurrent>
// #include <QDateTime>
// #include <QQmlEngine>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"

namespace xstudio::ui::qml {

class BookmarkCategoryModel : public JSONTreeModel {
    Q_OBJECT

    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)

    enum Roles { textRole = JSONTreeModel::Roles::LASTROLE, valueRole, colourRole };

  public:
    explicit BookmarkCategoryModel(QObject *parent = nullptr);

    [[nodiscard]] QVariant value() const { return value_; }
    void setValue(const QVariant &value);

    [[nodiscard]] QVariant
    data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

  signals:
    void valueChanged();

  private:
    QVariant value_;
};


class BookmarkFilterModel : public QSortFilterProxyModel {
    Q_OBJECT

    Q_PROPERTY(
        QVariantMap mediaOrder READ mediaOrder WRITE setMediaOrder NOTIFY mediaOrderChanged)
    Q_PROPERTY(QVariant currentMedia READ currentMedia WRITE setCurrentMedia NOTIFY
                   currentMediaChanged)

    Q_PROPERTY(int depth READ depth WRITE setDepth NOTIFY depthChanged)

  public:
    BookmarkFilterModel(QObject *parent = nullptr);

    [[nodiscard]] QVariantMap mediaOrder() const { return media_order_; }
    [[nodiscard]] int depth() const { return depth_; }
    [[nodiscard]] QVariant currentMedia() const { return QVariant::fromValue(current_media_); }

    void setMediaOrder(const QVariantMap &mo);
    void setDepth(const int value);
    void setCurrentMedia(const QVariant &value);

    Q_INVOKABLE bool
    removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override {
        return QSortFilterProxyModel::removeRows(row, count, parent);
    }
    [[nodiscard]] QModelIndex
    index(int row, int column, const QModelIndex &parent = QModelIndex()) const override {
        return QSortFilterProxyModel::index(row, column, parent);
    }


  protected:
    [[nodiscard]] bool
    filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    [[nodiscard]] bool
    lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

  signals:
    void mediaOrderChanged();
    void depthChanged();
    void currentMediaChanged();

  private:
    QVariantMap media_order_;
    QUuid current_media_;
    int depth_{0};
};


class BookmarkModel : public caf::mixin::actor_object<JSONTreeModel> {
    Q_OBJECT

    Q_PROPERTY(QString bookmarkActorAddr READ bookmarkActorAddr WRITE setBookmarkActorAddr
                   NOTIFY bookmarkActorAddrChanged)
    Q_PROPERTY(int length READ length NOTIFY lengthChanged)

  public:
    enum Roles {
        enabledRole = JSONTreeModel::Roles::LASTROLE,
        focusRole,
        frameRole,
        frameFromTimecodeRole,
        startTimecodeRole,
        endTimecodeRole,
        durationTimecodeRole,
        startFrameRole,
        endFrameRole,
        hasNoteRole,
        subjectRole,
        noteRole,
        authorRole,
        categoryRole,
        colourRole,
        createdRole,
        hasAnnotationRole,
        thumbnailRole,
        ownerRole,
        uuidRole,
        objectRole,
        startRole,
        durationRole,
        durationFrameRole,
        visibleRole
    };

    using super = caf::mixin::actor_object<JSONTreeModel>;
    explicit BookmarkModel(QObject *parent = nullptr);
    virtual void init(caf::actor_system &system);

    [[nodiscard]] QString bookmarkActorAddr() const { return bookmark_actor_addr_; };
    void setBookmarkActorAddr(const QString &addr);

    [[nodiscard]] QVariant
    data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool
    setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    Q_INVOKABLE bool
    removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    Q_INVOKABLE bool
    insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    [[nodiscard]] int length() const { return rowCount(); }

    Q_INVOKABLE QFuture<QString> exportCSVFuture(const QUrl &path);
    Q_INVOKABLE QString exportCSV(const QUrl &path) { return exportCSVFuture(path).result(); }

    Q_INVOKABLE [[nodiscard]] QString
    getJSON(const QModelIndex &index, const QString &path) const {
        return getJSONFuture(index, path).result();
    }
    [[nodiscard]] QFuture<QString>
    getJSONFuture(const QModelIndex &index, const QString &path) const;

  signals:
    void bookmarkActorAddrChanged();
    void lengthChanged();

  public:
    caf::actor_system &system() const { return self()->home_system(); };

  private:
    QVector<int> getRoleChanges(
        const bookmark::BookmarkDetail &ood, const bookmark::BookmarkDetail &nbd) const;
    nlohmann::json jsonFromBookmarks();
    bookmark::BookmarkDetail getDetail(const caf::actor &actor) const;
    nlohmann::json createJsonFromDetail(const bookmark::BookmarkDetail &detail) const;
    void sendDetail(const bookmark::BookmarkDetail &detail) const;

  private:
    QString bookmark_actor_addr_;
    caf::actor bookmark_actor_;
    caf::actor backend_events_;
    std::map<utility::Uuid, bookmark::BookmarkDetail> bookmarks_;
};
} // namespace xstudio::ui::qml
