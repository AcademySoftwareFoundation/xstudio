// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR
// GPL-3.0-only

#include <math.h>
// #include <QtCore/qstack.h>
// #include <QtCore/qdebug.h>
#include <QStack>

#include "xstudio/ui/qml/QTreeModelToTableModel.hpp"

// QT_BEGIN_NAMESPACE

// //#define QQMLTREEMODELADAPTOR_DEBUG
// #if defined(QQMLTREEMODELADAPTOR_DEBUG) && !defined(QT_TESTLIB_LIB)
// #   define ASSERT_CONSISTENCY() Q_ASSERT_X(testConsistency(true /* dumpOnFail */),
// Q_FUNC_INFO, "Consistency test failed") #else
#define ASSERT_CONSISTENCY qt_noop
// #endif

QTreeModelToTableModel::QTreeModelToTableModel(QObject *parent) : QAbstractItemModel(parent) {}

QAbstractItemModel *QTreeModelToTableModel::model() const { return m_model; }

void QTreeModelToTableModel::setModel(QAbstractItemModel *arg) {
    struct Cx {
        const char *signal;
        const char *slot;
    };
    const Cx connections[] = {
        {SIGNAL(destroyed(QObject *)), SLOT(modelHasBeenDestroyed())},
        {SIGNAL(modelReset()), SLOT(modelHasBeenReset())},
        {SIGNAL(dataChanged(QModelIndex, QModelIndex, QVector<int>)),
         SLOT(modelDataChanged(QModelIndex, QModelIndex, QVector<int>))},

        {SIGNAL(layoutAboutToBeChanged(
             QList<QPersistentModelIndex>, QAbstractItemModel::LayoutChangeHint)),
         SLOT(modelLayoutAboutToBeChanged(
             QList<QPersistentModelIndex>, QAbstractItemModel::LayoutChangeHint))},
        {SIGNAL(
             layoutChanged(QList<QPersistentModelIndex>, QAbstractItemModel::LayoutChangeHint)),
         SLOT(modelLayoutChanged(
             QList<QPersistentModelIndex>, QAbstractItemModel::LayoutChangeHint))},

        {SIGNAL(rowsAboutToBeInserted(QModelIndex, int, int)),
         SLOT(modelRowsAboutToBeInserted(QModelIndex, int, int))},
        {SIGNAL(rowsInserted(QModelIndex, int, int)),
         SLOT(modelRowsInserted(QModelIndex, int, int))},
        {SIGNAL(rowsAboutToBeRemoved(QModelIndex, int, int)),
         SLOT(modelRowsAboutToBeRemoved(QModelIndex, int, int))},
        {SIGNAL(rowsRemoved(QModelIndex, int, int)),
         SLOT(modelRowsRemoved(QModelIndex, int, int))},
        {SIGNAL(rowsAboutToBeMoved(QModelIndex, int, int, QModelIndex, int)),
         SLOT(modelRowsAboutToBeMoved(QModelIndex, int, int, QModelIndex, int))},
        {SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)),
         SLOT(modelRowsMoved(QModelIndex, int, int, QModelIndex, int))},
        {nullptr, nullptr}};

    if (m_model != arg) {
        if (m_model) {
            for (const Cx *c = &connections[0]; c->signal; c++)
                disconnect(m_model, c->signal, this, c->slot);
        }

        clearModelData();
        m_model = arg;

        if (m_rootIndex.isValid() && m_rootIndex.model() != m_model)
            m_rootIndex = QModelIndex();

        if (m_model) {
            for (const Cx *c = &connections[0]; c->signal; c++)
                connect(m_model, c->signal, this, c->slot);

            showModelTopLevelItems();
        }

        emit modelChanged(arg);
    }
}

void QTreeModelToTableModel::clearModelData() {
    beginResetModel();
    m_items.clear();
    m_expandedItems.clear();
    endResetModel();
    emit lengthChanged();
}

QModelIndex QTreeModelToTableModel::rootIndex() const { return m_rootIndex; }

void QTreeModelToTableModel::setRootIndex(const QModelIndex &idx) {
    if (m_rootIndex == idx)
        return;

    if (m_model)
        clearModelData();
    m_rootIndex = idx;
    if (m_model)
        showModelTopLevelItems();
    emit rootIndexChanged();
}

void QTreeModelToTableModel::resetRootIndex() { setRootIndex(QModelIndex()); }

QModelIndex
QTreeModelToTableModel::index(int row, int column, const QModelIndex &parent) const {
    return hasIndex(row, column, parent) ? createIndex(row, column) : QModelIndex();
}

QModelIndex QTreeModelToTableModel::parent(const QModelIndex &child) const {
    Q_UNUSED(child)
    return QModelIndex();
}

QHash<int, QByteArray> QTreeModelToTableModel::roleNames() const {
    if (!m_model)
        return QHash<int, QByteArray>();
    return m_model->roleNames();
}

int QTreeModelToTableModel::rowCount(const QModelIndex &) const {
    if (!m_model)
        return 0;
    return m_items.size();
}

int QTreeModelToTableModel::columnCount(const QModelIndex &parent) const {
    if (!m_model)
        return 0;
    return m_model->columnCount(parent);
}

QVariant QTreeModelToTableModel::data(const QModelIndex &index, int role) const {
    if (!m_model)
        return QVariant();

    return m_model->data(mapToModel(index), role);
}

bool QTreeModelToTableModel::setData(
    const QModelIndex &index, const QVariant &value, int role) {
    if (!m_model)
        return false;

    return m_model->setData(mapToModel(index), value, role);
}

QVariant
QTreeModelToTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    return m_model->headerData(section, orientation, role);
}

Qt::ItemFlags QTreeModelToTableModel::flags(const QModelIndex &index) const {
    return m_model->flags(mapToModel(index));
}

int QTreeModelToTableModel::depthAtRow(int row) const {
    if (row < 0 || row >= m_items.size())
        return 0;
    return m_items.at(row).depth;
}

int QTreeModelToTableModel::itemIndex(const QModelIndex &index) const {
    // This is basically a plagiarism of QTreeViewPrivate::viewIndex()
    if (!index.isValid() || index == m_rootIndex || m_items.isEmpty())
        return -1;

    const int totalCount = m_items.size();

    // We start nearest to the lastViewedItem
    int localCount = qMin(m_lastItemIndex - 1, totalCount - m_lastItemIndex);

    for (int i = 0; i < localCount; ++i) {
        const TreeItem &item1 = m_items.at(m_lastItemIndex + i);
        if (item1.index == index) {
            m_lastItemIndex = m_lastItemIndex + i;
            return m_lastItemIndex;
        }
        const TreeItem &item2 = m_items.at(m_lastItemIndex - i - 1);
        if (item2.index == index) {
            m_lastItemIndex = m_lastItemIndex - i - 1;
            return m_lastItemIndex;
        }
    }

    for (int j = qMax(0, m_lastItemIndex + localCount); j < totalCount; ++j) {
        const TreeItem &item = m_items.at(j);
        if (item.index == index) {
            m_lastItemIndex = j;
            return j;
        }
    }

    for (int j = qMin(totalCount, m_lastItemIndex - localCount) - 1; j >= 0; --j) {
        const TreeItem &item = m_items.at(j);
        if (item.index == index) {
            m_lastItemIndex = j;
            return j;
        }
    }

    // nothing found
    return -1;
}

bool QTreeModelToTableModel::isVisible(const QModelIndex &index) {
    return itemIndex(index) != -1;
}

bool QTreeModelToTableModel::childrenVisible(const QModelIndex &index) {
    return (index == m_rootIndex && !m_items.isEmpty()) ||
           (m_expandedItems.contains(index) && isVisible(index));
}

QModelIndex QTreeModelToTableModel::mapToModel(const QModelIndex &index) const {
    if (!index.isValid())
        return QModelIndex();

    const int row = index.row();
    if (row < 0 || row > m_items.size() - 1)
        return QModelIndex();

    const QModelIndex sourceIndex = m_items.at(row).index;
    return m_model->index(sourceIndex.row(), index.column(), sourceIndex.parent());
}

QModelIndex QTreeModelToTableModel::mapFromModel(const QModelIndex &index) const {
    if (!index.isValid())
        return QModelIndex();

    int row = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        const QModelIndex proxyIndex = m_items[i].index;
        if (proxyIndex.row() == index.row() && proxyIndex.parent() == index.parent()) {
            row = i;
            break;
        }
    }

    if (row == -1)
        return QModelIndex();

    return this->index(row, index.column());
}

QModelIndex QTreeModelToTableModel::mapToModel(int row) const {
    if (row < 0 || row >= m_items.size())
        return QModelIndex();
    return m_items.at(row).index;
}

QItemSelection QTreeModelToTableModel::selectionForRowRange(
    const QModelIndex &fromIndex, const QModelIndex &toIndex) const {
    int from = itemIndex(fromIndex);
    int to   = itemIndex(toIndex);
    if (from == -1) {
        if (to == -1)
            return QItemSelection();
        return QItemSelection(toIndex, toIndex);
    }

    to = qMax(to, 0);
    if (from > to)
        qSwap(from, to);

    typedef QPair<QModelIndex, QModelIndex> MIPair;
    typedef QHash<QModelIndex, MIPair> MI2MIPairHash;
    MI2MIPairHash ranges;
    QModelIndex firstIndex     = m_items.at(from).index;
    QModelIndex lastIndex      = firstIndex;
    QModelIndex previousParent = firstIndex.parent();
    bool selectLastRow         = false;
    for (int i = from + 1; i <= to || (selectLastRow = true); i++) {
        // We run an extra iteration to make sure the last row is
        // added to the selection. (And also to avoid duplicating
        // the insertion code.)
        QModelIndex index;
        QModelIndex parent;
        if (!selectLastRow) {
            index  = m_items.at(i).index;
            parent = index.parent();
        }
        if (selectLastRow || previousParent != parent) {
            const MI2MIPairHash::iterator &it = ranges.find(previousParent);
            if (it == ranges.end())
                ranges.insert(previousParent, MIPair(firstIndex, lastIndex));
            else
                it->second = lastIndex;

            if (selectLastRow)
                break;

            firstIndex     = index;
            previousParent = parent;
        }
        lastIndex = index;
    }

    QItemSelection sel;
    sel.reserve(ranges.size());
    for (const MIPair &pair : std::as_const(ranges))
        sel.append(QItemSelectionRange(pair.first, pair.second));

    return sel;
}

void QTreeModelToTableModel::showModelTopLevelItems(bool doInsertRows) {
    if (!m_model)
        return;

    if (m_model->hasChildren(m_rootIndex) && m_model->canFetchMore(m_rootIndex))
        m_model->fetchMore(m_rootIndex);
    const long topLevelRowCount = m_model->rowCount(m_rootIndex);
    if (topLevelRowCount == 0)
        return;

    showModelChildItems(TreeItem(m_rootIndex), 0, topLevelRowCount - 1, doInsertRows);
}

void QTreeModelToTableModel::showModelChildItems(
    const TreeItem &parentItem,
    int start,
    int end,
    bool doInsertRows,
    bool doExpandPendingRows) {
    const QModelIndex &parentIndex = parentItem.index;
    int rowIdx =
        parentIndex.isValid() && parentIndex != m_rootIndex ? itemIndex(parentIndex) + 1 : 0;
    Q_ASSERT(rowIdx == 0 || parentItem.expanded);
    if (parentIndex.isValid() && parentIndex != m_rootIndex &&
        (rowIdx == 0 || !parentItem.expanded))
        return;

    if (m_model->rowCount(parentIndex) == 0) {
        if (m_model->hasChildren(parentIndex) && m_model->canFetchMore(parentIndex))
            m_model->fetchMore(parentIndex);
        return;
    }

    int insertCount = end - start + 1;
    int startIdx;
    if (start == 0) {
        startIdx = rowIdx;
    } else {
        // Prefer to insert before next sibling instead of after last child of previous, as
        // the latter is potentially buggy, see QTBUG-66062
        const QModelIndex &nextSiblingIdx = m_model->index(end + 1, 0, parentIndex);
        if (nextSiblingIdx.isValid()) {
            startIdx = itemIndex(nextSiblingIdx);
        } else {
            const QModelIndex &prevSiblingIdx = m_model->index(start - 1, 0, parentIndex);
            startIdx                          = lastChildIndex(prevSiblingIdx) + 1;
        }
    }

    int rowDepth = rowIdx == 0 ? 0 : parentItem.depth + 1;
    if (doInsertRows)
        beginInsertRows(QModelIndex(), startIdx, startIdx + insertCount - 1);
    m_items.reserve(m_items.size() + insertCount);

    for (int i = 0; i < insertCount; i++) {
        const QModelIndex &cmi = m_model->index(start + i, 0, parentIndex);
        const bool expanded    = m_expandedItems.contains(cmi);
        const TreeItem treeItem(cmi, rowDepth, expanded);
        m_items.insert(startIdx + i, treeItem);

        if (expanded)
            m_itemsToExpand.append(treeItem);
    }

    if (doInsertRows) {
        endInsertRows();
        emit lengthChanged();
    }

    if (doExpandPendingRows)
        expandPendingRows(doInsertRows);
}


void QTreeModelToTableModel::expand(const QModelIndex &idx) {
    ASSERT_CONSISTENCY();
    if (!m_model)
        return;

    Q_ASSERT(!idx.isValid() || idx.model() == m_model);

    if (!idx.isValid() || !m_model->hasChildren(idx))
        return;
    if (m_expandedItems.contains(idx))
        return;

    int row = itemIndex(idx);
    if (row != -1)
        expandRow(row);
    else
        m_expandedItems.insert(idx);
    ASSERT_CONSISTENCY();

    emit expanded(idx);
}

void QTreeModelToTableModel::collapse(const QModelIndex &idx) {
    ASSERT_CONSISTENCY();
    if (!m_model)
        return;

    Q_ASSERT(!idx.isValid() || idx.model() == m_model);

    if (!idx.isValid() || !m_model->hasChildren(idx))
        return;
    if (!m_expandedItems.contains(idx))
        return;

    int row = itemIndex(idx);
    if (row != -1)
        collapseRow(row);
    else
        m_expandedItems.remove(idx);
    ASSERT_CONSISTENCY();

    emit collapsed(idx);
}

bool QTreeModelToTableModel::isExpanded(const QModelIndex &index) const {
    ASSERT_CONSISTENCY();
    if (!m_model)
        return false;

    Q_ASSERT(!index.isValid() || index.model() == m_model);
    return !index.isValid() || m_expandedItems.contains(index);
}

bool QTreeModelToTableModel::isExpanded(int row) const {
    if (row < 0 || row >= m_items.size())
        return false;
    return m_items.at(row).expanded;
}

bool QTreeModelToTableModel::hasChildren(int row) const {
    if (row < 0 || row >= m_items.size())
        return false;
    return m_model->hasChildren(m_items[row].index);
}

bool QTreeModelToTableModel::hasSiblings(int row) const {
    const QModelIndex &index = mapToModel(row);
    return index.row() != m_model->rowCount(index.parent()) - 1;
}

void QTreeModelToTableModel::expandRow(int n) {
    if (!m_model || isExpanded(n))
        return;

    TreeItem &item = m_items[n];

    // Ted - commenting the below test out, because hasChildren can return false
    // since the session model might not yet be filled out for the given index -
    // assumption is if we explicitly called 'expandRow' we want it expanded
    // regarless of whether it has children yet or now.

    /*if ((item.index.flags() & Qt::ItemNeverHasChildren) || !m_model->hasChildren(item.index))
        return;*/

    item.expanded = true;
    m_expandedItems.insert(item.index);
    QVector<int> changedRole(1, ExpandedRole);
    emit dataChanged(index(n, m_column), index(n, m_column), changedRole);

    m_itemsToExpand.append(item);
    expandPendingRows();
}

void QTreeModelToTableModel::expandRecursively(int row, int depth) {
    Q_ASSERT(depth == -1 || depth > 0);
    const int startDepth = depthAtRow(row);

    auto expandHelp =
        [this, depth, startDepth](const auto expandHelp, const QModelIndex &index) -> void {
        const int rowToExpand = itemIndex(index);
        if (!m_expandedItems.contains(index))
            expandRow(rowToExpand);

        if (depth != -1 && depthAtRow(rowToExpand) == startDepth + depth - 1)
            return;

        const int childCount = m_model->rowCount(index);
        for (int childRow = 0; childRow < childCount; ++childRow) {
            const QModelIndex childIndex = m_model->index(childRow, 0, index);
            if (m_model->hasChildren(childIndex))
                expandHelp(expandHelp, childIndex);
        }
    };

    const QModelIndex index = m_items[row].index;
    if (index.isValid())
        expandHelp(expandHelp, index);
}

void QTreeModelToTableModel::expandPendingRows(bool doInsertRows) {
    while (!m_itemsToExpand.isEmpty()) {
        const TreeItem item = m_itemsToExpand.takeFirst();
        Q_ASSERT(item.expanded);
        const QModelIndex &index = item.index;
        int childrenCount        = m_model->rowCount(index);
        if (childrenCount == 0) {
            if (m_model->hasChildren(index) && m_model->canFetchMore(index))
                m_model->fetchMore(index);
            continue;
        }

        // TODO Pre-compute the total number of items made visible
        // so that we only call a single beginInsertRows()/endInsertRows()
        // pair per expansion (same as we do for collapsing).
        showModelChildItems(item, 0, childrenCount - 1, doInsertRows, false);
    }
}

void QTreeModelToTableModel::collapseRecursively(int row) {
    auto collapseHelp = [this](const auto collapseHelp, const QModelIndex &index) -> void {
        if (m_expandedItems.contains(index)) {
            const int rowToCollapse = itemIndex(index);
            if (rowToCollapse != -1)
                collapseRow(rowToCollapse);
            else
                m_expandedItems.remove(index);
        }

        const int childCount = m_model->rowCount(index);
        for (int childRow = 0; childRow < childCount; ++childRow) {
            const QModelIndex childIndex = m_model->index(childRow, 0, index);
            if (m_model->hasChildren(childIndex))
                collapseHelp(collapseHelp, childIndex);
        }
    };

    const QModelIndex index = m_items[row].index;
    if (index.isValid())
        collapseHelp(collapseHelp, index);
}

#include "xstudio/utility/logging.hpp"

void QTreeModelToTableModel::collapseRow(int n) {
    if (!m_model || !isExpanded(n))
        return;

    SignalFreezer aggregator(this);

    TreeItem &item = m_items[n];
    item.expanded  = false;
    m_expandedItems.remove(item.index);
    QVector<int> changedRole(1, ExpandedRole);
    queueDataChanged(index(n, m_column), index(n, m_column), changedRole);
    int childrenCount = m_model->rowCount(item.index);
    if ((item.index.flags() & Qt::ItemNeverHasChildren) || !m_model->hasChildren(item.index) ||
        childrenCount == 0)
        return;

    const QModelIndex &emi = m_model->index(childrenCount - 1, 0, item.index);
    int lastIndex          = lastChildIndex(emi);
    removeVisibleRows(n + 1, lastIndex);
}

int QTreeModelToTableModel::lastChildIndex(const QModelIndex &index) const {
    // The purpose of this function is to return the row of the last decendant of a node N.
    // But note: index should point to the last child of N, and not N itself!
    // This means that if index is not expanded, the last child will simply be index itself.
    // Otherwise, since the tree underneath index can be of any depth, it will instead find
    // the first sibling of N, get its table row, and simply return the row above.

    // index is last direct child of item being collapsed

    if (!m_expandedItems.contains(index))
        return itemIndex(index);

    // last child is expanded, we need to find last child of it..
    // this is recursive..

    // item being collapsed
    QModelIndex parent = index;
    QModelIndex nextSiblingIndex;

    int firstIndex;

    while (parent.isValid() and m_expandedItems.contains(parent)) {
        // last child of parent
        nextSiblingIndex = m_model->index(m_model->rowCount(parent) - 1, 0, parent);
        firstIndex       = itemIndex(nextSiblingIndex);
        parent           = nextSiblingIndex;
    }

    return firstIndex;
}

void QTreeModelToTableModel::removeVisibleRows(
    int startIndex, int endIndex, bool doRemoveRows) {
    if (startIndex < 0 || endIndex < 0 || startIndex > endIndex)
        return;

    if (doRemoveRows)
        beginRemoveRows(QModelIndex(), startIndex, endIndex);
    m_items.erase(m_items.begin() + startIndex, m_items.begin() + endIndex + 1);
    if (doRemoveRows) {
        endRemoveRows();
        emit lengthChanged();


        /* We need to update the model index for all the items below the removed ones */
        int lastIndex = m_items.size() - 1;
        if (startIndex <= lastIndex) {
            const QModelIndex &topLeft     = index(startIndex, 0, QModelIndex());
            const QModelIndex &bottomRight = index(lastIndex, 0, QModelIndex());
            const QVector<int> changedRole(1, ModelIndexRole);
            queueDataChanged(topLeft, bottomRight, changedRole);
        }
    }
}

void QTreeModelToTableModel::modelHasBeenDestroyed() {
    // The model has been deleted. This should behave as if no model was set
    clearModelData();
    emit modelChanged(nullptr);
}

void QTreeModelToTableModel::modelHasBeenReset() {
    clearModelData();
    emit lengthChanged();

    showModelTopLevelItems();
    ASSERT_CONSISTENCY();
}

void QTreeModelToTableModel::modelDataChanged(
    const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
    Q_ASSERT(topLeft.parent() == bottomRight.parent());
    const QModelIndex &parent = topLeft.parent();
    if (parent.isValid() && !childrenVisible(parent)) {
        ASSERT_CONSISTENCY();
        return;
    }

    int topIndex = itemIndex(topLeft.siblingAtColumn(0));
    if (topIndex == -1) // 'parent' is not visible anymore, though it's been expanded previously
        return;
    for (int i = topLeft.row(); i <= bottomRight.row(); i++) {
        // Group items with same parent to minize the number of 'dataChanged()' emits
        int bottomIndex = topIndex;
        while (bottomIndex < m_items.size()) {
            const QModelIndex &idx = m_items.at(bottomIndex).index;
            if (idx.parent() != parent) {
                --bottomIndex;
                break;
            }
            if (idx.row() == bottomRight.row())
                break;
            ++bottomIndex;
        }
        emit dataChanged(
            index(topIndex, topLeft.column()), index(bottomIndex, bottomRight.column()), roles);

        i += bottomIndex - topIndex;
        if (i == bottomRight.row())
            break;
        topIndex = bottomIndex + 1;
        while (topIndex < m_items.size() && m_items.at(topIndex).index.parent() != parent)
            topIndex++;
    }
    ASSERT_CONSISTENCY();
}

void QTreeModelToTableModel::modelLayoutAboutToBeChanged(
    const QList<QPersistentModelIndex> &parents, QAbstractItemModel::LayoutChangeHint hint) {
    Q_UNUSED(hint)

    // Since the m_items is a list of TreeItems that contains QPersistentModelIndexes, we
    // cannot wait until we get a modelLayoutChanged() before we remove the affected rows
    // from that list. After the layout has changed, the list (or, the persistent indexes
    // that it contains) is no longer in sync with the model (after all, that is what we're
    // supposed to correct in modelLayoutChanged()).
    // This means that vital functions, like itemIndex(index), cannot be trusted at that point.
    // Therefore we need to do the update in two steps; First remove all the affected rows
    // from here (while we're still in sync with the model), and then add back the
    // affected rows, and notify about it, from modelLayoutChanged().
    m_modelLayoutChanged = false;

    if (parents.isEmpty() || !parents[0].isValid()) {
        // Update entire model
        emit layoutAboutToBeChanged();
        m_modelLayoutChanged = true;
        m_items.clear();
        return;
    }

    for (const QPersistentModelIndex &pmi : parents) {
        for (const auto &i : m_items) {
            if (i.index == pmi) {
                // Update entire model
                emit layoutAboutToBeChanged();
                m_modelLayoutChanged = true;
                m_items.clear();
                return;
            }
        }
    }

    for (const QPersistentModelIndex &pmi : parents) {
        if (!m_expandedItems.contains(pmi))
            continue;
        const int row = itemIndex(pmi);
        if (row == -1)
            continue;
        const int rowCount = m_model->rowCount(pmi);
        if (rowCount == 0)
            continue;

        if (!m_modelLayoutChanged) {
            emit layoutAboutToBeChanged();
            m_modelLayoutChanged = true;
        }

        const QModelIndex &lmi = m_model->index(rowCount - 1, 0, pmi);
        const int lastRow      = lastChildIndex(lmi);
        removeVisibleRows(row + 1, lastRow, false /*doRemoveRows*/);
    }

    ASSERT_CONSISTENCY();
}

void QTreeModelToTableModel::modelLayoutChanged(
    const QList<QPersistentModelIndex> &parents, QAbstractItemModel::LayoutChangeHint hint) {
    Q_UNUSED(hint)

    if (!m_modelLayoutChanged) {
        // No relevant changes done from modelLayoutAboutToBeChanged()
        return;
    }

    if (m_items.isEmpty()) {
        // Entire model has changed. Add back all rows.
        showModelTopLevelItems(false /*doInsertRows*/);
        const QModelIndex &mi = m_model->index(0, 0);
        const int columnCount = m_model->columnCount(mi);
        emit dataChanged(index(0, 0), index(m_items.size() - 1, columnCount - 1));
        emit layoutChanged();
        return;
    }

    for (const QPersistentModelIndex &pmi : parents) {
        if (!m_expandedItems.contains(pmi))
            continue;
        const int row = itemIndex(pmi);
        if (row == -1)
            continue;
        const int rowCount = m_model->rowCount(pmi);
        if (rowCount == 0)
            continue;

        const QModelIndex &lmi = m_model->index(rowCount - 1, 0, pmi);
        const int columnCount  = m_model->columnCount(lmi);
        showModelChildItems(m_items.at(row), 0, rowCount - 1, false /*doInsertRows*/);
        const int lastRow = lastChildIndex(lmi);
        emit dataChanged(index(row + 1, 0), index(lastRow, columnCount - 1));
    }

    emit layoutChanged();

    ASSERT_CONSISTENCY();
}

void QTreeModelToTableModel::modelRowsAboutToBeInserted(
    const QModelIndex &parent, int start, int end) {
    Q_UNUSED(parent)
    Q_UNUSED(start)
    Q_UNUSED(end)
    ASSERT_CONSISTENCY();
}

void QTreeModelToTableModel::modelRowsInserted(const QModelIndex &parent, int start, int end) {
    TreeItem item;
    int parentRow = itemIndex(parent);
    if (parentRow >= 0) {
        const QModelIndex &parentIndex = index(parentRow, m_column);
        QVector<int> changedRole(1, HasChildrenRole);
        queueDataChanged(parentIndex, parentIndex, changedRole);
        item = m_items.at(parentRow);
        if (!item.expanded) {
            ASSERT_CONSISTENCY();
            return;
        }
    } else if (parent == m_rootIndex) {
        item = TreeItem(parent);
    } else {
        ASSERT_CONSISTENCY();
        return;
    }
    showModelChildItems(item, start, end);
    ASSERT_CONSISTENCY();
}

void QTreeModelToTableModel::modelRowsAboutToBeRemoved(
    const QModelIndex &parent, int start, int end) {
    ASSERT_CONSISTENCY();
    enableSignalAggregation();
    if (parent == m_rootIndex || childrenVisible(parent)) {
        const QModelIndex &smi = m_model->index(start, 0, parent);
        int startIndex         = itemIndex(smi);
        const QModelIndex &emi = m_model->index(end, 0, parent);
        int endIndex           = -1;
        if (isExpanded(emi)) {
            int rowCount = m_model->rowCount(emi);
            if (rowCount > 0) {
                const QModelIndex &idx = m_model->index(rowCount - 1, 0, emi);
                endIndex               = lastChildIndex(idx);
            }
        }
        if (endIndex == -1)
            endIndex = itemIndex(emi);

        removeVisibleRows(startIndex, endIndex);
    }

    for (int r = start; r <= end; r++) {
        const QModelIndex &cmi = m_model->index(r, 0, parent);
        m_expandedItems.remove(cmi);
    }
}

void QTreeModelToTableModel::modelRowsRemoved(const QModelIndex &parent, int start, int end) {
    Q_UNUSED(start)
    Q_UNUSED(end)
    int parentRow = itemIndex(parent);
    if (parentRow >= 0) {
        const QModelIndex &parentIndex = index(parentRow, m_column);
        QVector<int> changedRole(1, HasChildrenRole);
        queueDataChanged(parentIndex, parentIndex, changedRole);
    }
    disableSignalAggregation();
    ASSERT_CONSISTENCY();
}

void QTreeModelToTableModel::modelRowsAboutToBeMoved(
    const QModelIndex &sourceParent,
    int sourceStart,
    int sourceEnd,
    const QModelIndex &destinationParent,
    int destinationRow) {
    ASSERT_CONSISTENCY();
    enableSignalAggregation();
    m_visibleRowsMoved = false;
    if (!childrenVisible(sourceParent))
        return; // Do nothing now. See modelRowsMoved() below.

    if (!childrenVisible(destinationParent)) {
        modelRowsAboutToBeRemoved(sourceParent, sourceStart, sourceEnd);
        /* If the destination parent has no children, we'll need to
         * report a change on the HasChildrenRole */
        if (isVisible(destinationParent) && m_model->rowCount(destinationParent) == 0) {
            const QModelIndex &topLeft = index(itemIndex(destinationParent), 0, QModelIndex());
            const QModelIndex &bottomRight = topLeft;
            const QVector<int> changedRole(1, HasChildrenRole);
            queueDataChanged(topLeft, bottomRight, changedRole);
        }
    } else {
        int depthDifference = -1;
        if (destinationParent.isValid()) {
            int destParentIndex = itemIndex(destinationParent);
            if (destParentIndex >= 0)
                depthDifference = m_items.at(destParentIndex).depth;
            else
                depthDifference = 0;
        }
        if (sourceParent.isValid()) {
            int sourceParentIndex = itemIndex(sourceParent);
            if (sourceParentIndex >= 0)
                depthDifference -= m_items.at(sourceParentIndex).depth;
            else
                depthDifference = 0;
        } else {
            depthDifference++;
        }

        int startIndex         = itemIndex(m_model->index(sourceStart, 0, sourceParent));
        const QModelIndex &emi = m_model->index(sourceEnd, 0, sourceParent);
        int endIndex           = -1;
        if (isExpanded(emi)) {
            int rowCount = m_model->rowCount(emi);
            if (rowCount > 0)
                endIndex = lastChildIndex(m_model->index(rowCount - 1, 0, emi));
        }
        if (endIndex == -1)
            endIndex = itemIndex(emi);

        int destIndex = -1;
        if (destinationRow == m_model->rowCount(destinationParent)) {
            const QModelIndex &emi = m_model->index(destinationRow - 1, 0, destinationParent);
            destIndex              = lastChildIndex(emi) + 1;
        } else {
            destIndex = itemIndex(m_model->index(destinationRow, 0, destinationParent));
        }

        int totalMovedCount = endIndex - startIndex + 1;

        /* This beginMoveRows() is matched by a endMoveRows() in the
         * modelRowsMoved() method below. */
        m_visibleRowsMoved =
            startIndex != destIndex &&
            beginMoveRows(QModelIndex(), startIndex, endIndex, QModelIndex(), destIndex);

        const QList<TreeItem> &buffer = m_items.mid(startIndex, totalMovedCount);
        int bufferCopyOffset;
        if (destIndex > endIndex) {
            for (int i = endIndex + 1; i < destIndex; i++) {
                m_items.swapItemsAt(
                    i, i - totalMovedCount); // Fast move from 1st to 2nd position
            }
            bufferCopyOffset = destIndex - totalMovedCount;
        } else {
            // NOTE: we will not enter this loop if startIndex == destIndex
            for (int i = startIndex - 1; i >= destIndex; i--) {
                m_items.swapItemsAt(
                    i, i + totalMovedCount); // Fast move from 1st to 2nd position
            }
            bufferCopyOffset = destIndex;
        }
        for (int i = 0; i < buffer.size(); i++) {
            TreeItem item = buffer.at(i);
            item.depth += depthDifference;
            m_items.replace(bufferCopyOffset + i, item);
        }

        /* If both source and destination items are visible, the indexes of
         * all the items in between will change. If they share the same
         * parent, then this is all; however, if they belong to different
         * parents, their bottom siblings will also get displaced, so their
         * index also needs to be updated.
         * Given that the bottom siblings of the top moved elements are
         * already included in the update (since they lie between the
         * source and the dest elements), we only need to worry about the
         * siblings of the bottom moved element.
         */
        const int top = qMin(startIndex, bufferCopyOffset);
        int bottom    = qMax(endIndex, bufferCopyOffset + totalMovedCount - 1);
        if (sourceParent != destinationParent) {
            const QModelIndex &bottomParent =
                bottom == endIndex ? sourceParent : destinationParent;

            const int rowCount = m_model->rowCount(bottomParent);
            if (rowCount > 0)
                bottom =
                    qMax(bottom, lastChildIndex(m_model->index(rowCount - 1, 0, bottomParent)));
        }
        const QModelIndex &topLeft     = index(top, 0, QModelIndex());
        const QModelIndex &bottomRight = index(bottom, 0, QModelIndex());
        const QVector<int> changedRole(1, ModelIndexRole);
        queueDataChanged(topLeft, bottomRight, changedRole);

        if (depthDifference != 0) {
            const QModelIndex &topLeft = index(bufferCopyOffset, 0, QModelIndex());
            const QModelIndex &bottomRight =
                index(bufferCopyOffset + totalMovedCount - 1, 0, QModelIndex());
            const QVector<int> changedRole(1, DepthRole);
            queueDataChanged(topLeft, bottomRight, changedRole);
        }
    }
}

void QTreeModelToTableModel::modelRowsMoved(
    const QModelIndex &sourceParent,
    int sourceStart,
    int sourceEnd,
    const QModelIndex &destinationParent,
    int destinationRow) {
    if (!childrenVisible(sourceParent)) {
        modelRowsInserted(
            destinationParent, destinationRow, destinationRow + sourceEnd - sourceStart);
    } else if (!childrenVisible(destinationParent)) {
        modelRowsRemoved(sourceParent, sourceStart, sourceEnd);
    }

    if (m_visibleRowsMoved)
        endMoveRows();

    if (isVisible(sourceParent) && m_model->rowCount(sourceParent) == 0) {
        int parentRow = itemIndex(sourceParent);
        collapseRow(parentRow);
        const QModelIndex &topLeft     = index(parentRow, 0, QModelIndex());
        const QModelIndex &bottomRight = topLeft;
        const QVector<int> changedRole{ExpandedRole, HasChildrenRole};
        queueDataChanged(topLeft, bottomRight, changedRole);
    }

    disableSignalAggregation();

    ASSERT_CONSISTENCY();
}

void QTreeModelToTableModel::dump() const {
    if (!m_model)
        return;
    int count = m_items.size();
    if (count == 0)
        return;
    int countWidth = floor(log10(double(count))) + 1;
    // qInfo() << "Dumping" << this;
    for (int i = 0; i < count; i++) {
        const TreeItem &item = m_items.at(i);
        bool hasChildren     = m_model->hasChildren(item.index);
        int children         = m_model->rowCount(item.index);
        // qInfo().noquote().nospace()
        //         << QStringLiteral("%1 ").arg(i, countWidth) << QString(4 * item.depth,
        //         QChar::fromLatin1('.'))
        //         << QLatin1String(!hasChildren ? ".. " : item.expanded ? " v " : " > ")
        //         << item.index << children;
    }
}

bool QTreeModelToTableModel::testConsistency(bool dumpOnFail) const {
    if (!m_model) {
        if (!m_items.isEmpty()) {
            // qWarning() << "Model inconsistency: No model but stored visible items";
            return false;
        }
        if (!m_expandedItems.isEmpty()) {
            // qWarning() << "Model inconsistency: No model but stored expanded items";
            return false;
        }
        return true;
    }
    QModelIndex parent = m_rootIndex;
    QStack<QModelIndex> ancestors;
    QModelIndex idx = m_model->index(0, 0, parent);
    for (int i = 0; i < m_items.size(); i++) {
        bool isConsistent    = true;
        const TreeItem &item = m_items.at(i);
        if (item.index != idx) {
            // qWarning() << "QModelIndex inconsistency" << i << item.index;
            // qWarning() << "    expected" << idx;
            isConsistent = false;
        }
        if (item.index.parent() != parent) {
            // qWarning() << "Parent inconsistency" << i << item.index;
            // qWarning() << "    stored index parent" << item.index.parent() << "model parent"
            // << parent;
            isConsistent = false;
        }
        if (item.depth != ancestors.size()) {
            // qWarning() << "Depth inconsistency" << i << item.index;
            // qWarning() << "    item depth" << item.depth << "ancestors stack" <<
            // ancestors.size();
            isConsistent = false;
        }
        if (item.expanded && !m_expandedItems.contains(item.index)) {
            // qWarning() << "Expanded inconsistency" << i << item.index;
            // qWarning() << "    set" << m_expandedItems.contains(item.index) << "item" <<
            // item.expanded;
            isConsistent = false;
        }
        if (!isConsistent) {
            if (dumpOnFail)
                dump();
            return false;
        }
        QModelIndex firstChildIndex;
        if (item.expanded)
            firstChildIndex = m_model->index(0, 0, idx);
        if (firstChildIndex.isValid()) {
            ancestors.push(parent);
            parent = idx;
            idx    = m_model->index(0, 0, parent);
        } else {
            while (idx.row() == m_model->rowCount(parent) - 1) {
                if (ancestors.isEmpty())
                    break;
                idx    = parent;
                parent = ancestors.pop();
            }
            idx = m_model->index(idx.row() + 1, 0, parent);
        }
    }

    return true;
}

void QTreeModelToTableModel::enableSignalAggregation() { m_signalAggregatorStack++; }

void QTreeModelToTableModel::disableSignalAggregation() {
    m_signalAggregatorStack--;
    Q_ASSERT(m_signalAggregatorStack >= 0);
    if (m_signalAggregatorStack == 0) {
        emitQueuedSignals();
    }
}

void QTreeModelToTableModel::queueDataChanged(
    const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
    if (isAggregatingSignals()) {
        m_queuedDataChanged.append(DataChangedParams{topLeft, bottomRight, roles});
    } else {
        emit dataChanged(topLeft, bottomRight, roles);
    }
}

void QTreeModelToTableModel::emitQueuedSignals() {
    QVector<DataChangedParams> combinedUpdates;
    /* First, iterate through the queued updates and merge the overlapping ones
     * to reduce the number of updates.
     * We don't merge adjacent updates, because they are typically filed with a
     * different role (a parent row is next to its children).
     */
    for (const DataChangedParams &dataChange : std::as_const(m_queuedDataChanged)) {
        int startRow = dataChange.topLeft.row();
        int endRow   = dataChange.bottomRight.row();
        bool merged  = false;
        for (DataChangedParams &combined : combinedUpdates) {
            int combinedStartRow = combined.topLeft.row();
            int combinedEndRow   = combined.bottomRight.row();
            if ((startRow <= combinedStartRow && endRow >= combinedStartRow) ||
                (startRow <= combinedEndRow && endRow >= combinedEndRow)) {
                if (startRow < combinedStartRow) {
                    combined.topLeft = dataChange.topLeft;
                }
                if (endRow > combinedEndRow) {
                    combined.bottomRight = dataChange.bottomRight;
                }
                for (int role : dataChange.roles) {
                    if (!combined.roles.contains(role))
                        combined.roles.append(role);
                }
                merged = true;
                break;
            }
        }
        if (!merged) {
            combinedUpdates.append(dataChange);
        }
    }

    /* Finally, emit the dataChanged signals */
    for (const DataChangedParams &dataChange : combinedUpdates) {
        emit dataChanged(dataChange.topLeft, dataChange.bottomRight, dataChange.roles);
    }
    m_queuedDataChanged.clear();
}

// QT_END_NAMESPACE

// #include "moc_QTreeModelToTableModel_p_p.cpp"
