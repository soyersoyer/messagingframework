/****************************************************************************
**
** This file is part of the $PACKAGE_NAME$.
**
** Copyright (C) $THISYEAR$ $COMPANY_NAME$.
**
** $QT_EXTENDED_DUAL_LICENSE$
**
****************************************************************************/

#include "qmailmessagethreadedmodel.h"
#include "qmailstore.h"
#include "qmailnamespace.h"
#include <QCache>
#include <QtAlgorithms>

static const int fullRefreshCutoff = 10;

class QMailMessageThreadedModelItem
{
public:
    explicit QMailMessageThreadedModelItem(const QMailMessageId& id, QMailMessageThreadedModelItem *parent = 0) : _id(id), _parent(parent) {}
    ~QMailMessageThreadedModelItem() {}

    QMailMessageId id() const { return _id; }

    QMailMessageThreadedModelItem &parent() const { return *_parent; }

    QList<QMailMessageThreadedModelItem> &children() { return _children; }
    const QList<QMailMessageThreadedModelItem> &children() const { return _children; }

    int rowInParent() const { return _parent->_children.indexOf(*this); }

    QMailMessageIdList childrenIds() const 
    { 
        QMailMessageIdList ids; 
        foreach (const QMailMessageThreadedModelItem &item, _children) {
            ids.append(item._id);
        }
        
        return ids;
    }

    bool operator== (const QMailMessageThreadedModelItem& other) { return (_id == other._id); }

    QMailMessageId _id;
    QMailMessageThreadedModelItem *_parent;
    QList<QMailMessageThreadedModelItem> _children;
};

// Messages must be deleted in the order: deep to shallow, and then high index to low index
// This comparator must yield less than to facilitate deletion in reverse sorted order
struct LessThanForRemoval
{
    bool operator()(const QPair<QModelIndex, int> &lhs, const QPair<QModelIndex, int> &rhs) const
    {
        if (!lhs.first.isValid() && !rhs.first.isValid()) {
            return (lhs.second < rhs.second);
        } 
        if (!lhs.first.isValid()) {
            return true;
        } 
        if (!rhs.first.isValid()) {
            return false;
        } 

        int lhsDepth = 0;
        int rhsDepth = 0;

        QMailMessageThreadedModelItem *lhsItem = static_cast<QMailMessageThreadedModelItem *>(lhs.first.internalPointer());
        while (lhsItem) {
            lhsItem = lhsItem->_parent;
            ++lhsDepth;
        }

        QMailMessageThreadedModelItem *rhsItem = static_cast<QMailMessageThreadedModelItem *>(rhs.first.internalPointer());
        while (rhsItem) {
            rhsItem = rhsItem->_parent;
            ++rhsDepth;
        }

        if (lhsDepth < rhsDepth) {
            return true;
        }
        if (rhsDepth < lhsDepth) {
            return false;
        }

        return (lhs.second < rhs.second);
    }
};

class QMailMessageThreadedModelPrivate : public QMailMessageModelImplementation
{
public:
    QMailMessageThreadedModelPrivate(QMailMessageThreadedModel& model, const QMailMessageKey& key, const QMailMessageSortKey& sortKey, bool sychronizeEnabled);
    ~QMailMessageThreadedModelPrivate();

    QMailMessageKey key() const;
    void setKey(const QMailMessageKey& key);

    QMailMessageSortKey sortKey() const;
    void setSortKey(const QMailMessageSortKey& sortKey);

    bool isEmpty() const;

    int rowCount(const QModelIndex& idx) const;
    int columnCount(const QModelIndex& idx) const;

    QMailMessageId idFromIndex(const QModelIndex& index) const;
    QModelIndex indexFromId(const QMailMessageId& id) const;

    Qt::CheckState checkState(const QModelIndex &idx) const;
    void setCheckState(const QModelIndex &idx, Qt::CheckState state);

    int rootRow(const QModelIndex& idx) const;

    void reset();

    bool ignoreMailStoreUpdates() const;
    bool setIgnoreMailStoreUpdates(bool ignore);

    bool additionLocations(const QMailMessageIdList &ids,
                           QList<LocationSequence> *locations, 
                           QMailMessageIdList *insertIds);

    bool updateLocations(const QMailMessageIdList &ids, 
                         QList<LocationSequence> *additions, 
                         QList<LocationSequence> *deletions, 
                         QList<LocationSequence> *updates,
                         QMailMessageIdList *insertIds);

    bool removalLocations(const QMailMessageIdList &ids, 
                          QList<LocationSequence> *locations);

    void insertItemAt(int row, const QModelIndex &parentIndex, const QMailMessageId &id);
    void removeItemAt(int row, const QModelIndex &parentIndex);

    QModelIndex index(int row, int column, const QModelIndex &parentIndex) const;
    QModelIndex parent(const QModelIndex &idx) const;

    QModelIndex updateIndex(const QModelIndex &idx) const;

private:
    void init() const;

    QModelIndex index(const QMailMessageThreadedModelItem *item, int column) const;
    QModelIndex parentIndex(const QMailMessageThreadedModelItem *item, int column) const;

    QMailMessageThreadedModelItem *itemFromIndex(const QModelIndex &index) const;
    QModelIndex indexFromItem(const QMailMessageThreadedModelItem *item) const;

    void additionLocations(const QMailMessageIdList &ids, QList<LocationSequence> *locations, QMailMessageIdList *insertIds, const QMailMessageIdList &removedIds);
    void removalLocations(const QMailMessageIdList &ids, QList<QPair<QModelIndex, int> > *indices, QMailMessageIdList *readditions);

    QMailMessageThreadedModel &_model;
    QMailMessageKey _key;
    QMailMessageSortKey _sortKey;
    bool _ignoreUpdates;
    mutable QMailMessageThreadedModelItem _root;
    mutable QMap<QMailMessageId, QMailMessageThreadedModelItem*> _messageItem;
    mutable QSet<QMailMessageId> _checkedIds;
    mutable QList<QMailMessageId> _currentIds;
    mutable QMap<QMailMessageId, QMailMessageThreadedModelItem> _pendingItem;
    mutable bool _initialised;
    mutable bool _needSynchronize;
};


QMailMessageThreadedModelPrivate::QMailMessageThreadedModelPrivate(QMailMessageThreadedModel& model,
                                                                   const QMailMessageKey& key, 
                                                                   const QMailMessageSortKey& sortKey, 
                                                                   bool ignoreUpdates)
:
    _model(model),
    _key(key),
    _sortKey(sortKey),
    _ignoreUpdates(ignoreUpdates),
    _root(QMailMessageId()),
    _initialised(false),
    _needSynchronize(true)
{
}

QMailMessageThreadedModelPrivate::~QMailMessageThreadedModelPrivate()
{
}

QMailMessageKey QMailMessageThreadedModelPrivate::key() const
{
    return _key; 
}

void QMailMessageThreadedModelPrivate::setKey(const QMailMessageKey& key) 
{
    _key = key;
}

QMailMessageSortKey QMailMessageThreadedModelPrivate::sortKey() const
{
   return _sortKey;
}

void QMailMessageThreadedModelPrivate::setSortKey(const QMailMessageSortKey& sortKey) 
{
    _sortKey = sortKey;
}

bool QMailMessageThreadedModelPrivate::isEmpty() const
{
    init();

    return _root.children().isEmpty();
}

int QMailMessageThreadedModelPrivate::rowCount(const QModelIndex &idx) const
{
    init();

    if (idx.isValid()) {
        if (QMailMessageThreadedModelItem *item = itemFromIndex(idx)) {
            return item->children().count();
        }
    } else {
        return _root.children().count();
    }

    return -1;
}

int QMailMessageThreadedModelPrivate::columnCount(const QModelIndex &idx) const
{
    init();

    return 1;
    
    Q_UNUSED(idx)
}

QMailMessageId QMailMessageThreadedModelPrivate::idFromIndex(const QModelIndex& idx) const
{
    init();

    if (idx.isValid()) {
        if (QMailMessageThreadedModelItem *item = itemFromIndex(idx)) {
            return item->_id;
        }
    }

    return QMailMessageId();
}

QModelIndex QMailMessageThreadedModelPrivate::indexFromId(const QMailMessageId& id) const
{
    init();

    if (id.isValid()) {
        QMap<QMailMessageId, QMailMessageThreadedModelItem*>::const_iterator it = _messageItem.find(id);
        if (it != _messageItem.end()) {
            return indexFromItem(it.value());
        }
    }

    return QModelIndex();
}

Qt::CheckState QMailMessageThreadedModelPrivate::checkState(const QModelIndex &idx) const
{
    init();

    if (idx.isValid()) {
        if (QMailMessageThreadedModelItem *item = itemFromIndex(idx)) {
            return (_checkedIds.contains(item->_id) ? Qt::Checked : Qt::Unchecked);
        }
    }

    return Qt::Unchecked;
}

void QMailMessageThreadedModelPrivate::setCheckState(const QModelIndex &idx, Qt::CheckState state)
{
    if (idx.isValid()) {
        if (QMailMessageThreadedModelItem *item = itemFromIndex(idx)) {
            // No support for partial checking in this model...
            if (state == Qt::Checked) {
                _checkedIds.insert(item->_id);
            } else {
                _checkedIds.remove(item->_id);
            }
        }
    }
}

int QMailMessageThreadedModelPrivate::rootRow(const QModelIndex& idx) const
{
    if (idx.isValid()) {
        QMailMessageThreadedModelItem *item = itemFromIndex(idx);
        while (item->_parent != &_root) {
            item = item->_parent;
        }

        return item->rowInParent();
    }

    return -1;
}

void QMailMessageThreadedModelPrivate::reset()
{
    _initialised = false;
}

bool QMailMessageThreadedModelPrivate::ignoreMailStoreUpdates() const
{
    return _ignoreUpdates;
}

bool QMailMessageThreadedModelPrivate::setIgnoreMailStoreUpdates(bool ignore)
{
    _ignoreUpdates = ignore;
    return (!_ignoreUpdates && _needSynchronize);
}

bool QMailMessageThreadedModelPrivate::additionLocations(const QMailMessageIdList &ids,
                                                         QList<LocationSequence> *locations, 
                                                         QMailMessageIdList *insertIds)
{
    if (!_initialised) {
        // Nothing to do yet
        return true;
    }
    
    if (_ignoreUpdates) {
        // Defer until resynchronised
        _needSynchronize = true;
        return true;
    }

    additionLocations(ids, locations, insertIds, QMailMessageIdList());
    return true;
}

void QMailMessageThreadedModelPrivate::additionLocations(const QMailMessageIdList &ids,
                                                         QList<LocationSequence> *locations, 
                                                         QMailMessageIdList *insertIds,
                                                         const QMailMessageIdList &absentIds)
{
    // Are any of these messages members of our display set?
    // Note - we must only consider messages in the set given by (those we currently know +
    // those we have now been informed of) because the database content may have changed between
    // when this event was recorded and when we're processing the signal.
    
    QMailMessageKey idKey(QMailMessageKey::id(_currentIds + ids));
    QMailMessageIdList newIds(QMailStore::instance()->queryMessages(_key & idKey, _sortKey));

    // Find which of the messages we must add (in ascending insertion order)
    QList<int> validIndices;
    foreach (const QMailMessageId &id, ids) {
        int index = newIds.indexOf(id);
        if (index != -1) {
            validIndices.append(index);
        }
    }

    if (validIndices.isEmpty())
        return;

    qSort(validIndices);

    QMailMessageIdList additionIds;
    foreach (int index, validIndices) {
        additionIds.append(newIds.at(index));
    }

    // Find all messages involved in conversations with the new messages, along with their predecessor ID
    QMailMessageKey conversationKey(QMailMessageKey::conversation(QMailMessageKey::id(additionIds)));
    QMailMessageKey::Properties props(QMailMessageKey::Id | QMailMessageKey::InResponseTo);

    QMap<QMailMessageId, QMailMessageId> predecessor;
    foreach (const QMailMessageMetaData &metaData, QMailStore::instance()->messagesMetaData(conversationKey, props)) {
        predecessor.insert(metaData.id(), metaData.inResponseTo());
    }

    // Any previous additions should have been finalised by now
    _pendingItem.clear();

    QList<QPair<QModelIndex, int> > insertIndices;
    QMap<QMailMessageThreadedModelItem*, int> addedChildren;

    // Process the messages to insert into the tree
    foreach (const QMailMessageId& id, additionIds) {
        // See if we have already added this message
        if (insertIds->contains(id))
            continue;

        QMailMessageId messageId(id);
        QList<QMailMessageId> descendants;

        // Find the first message ancestor that is in our display set
        QMailMessageId predecessorId(predecessor[messageId]);
        while (predecessorId.isValid() && !newIds.contains(predecessorId)) {
            predecessorId = predecessor[predecessorId];
        }

        do {
            int messagePos = newIds.indexOf(messageId);

            QMailMessageThreadedModelItem *insertParent = 0;

            if (!predecessorId.isValid()) {
                // This message is a root node - we need to find where it fits in
                insertParent = &_root;
            } else {
                // We can only insert into an absent item if we have logically reinserted it
                if (!absentIds.contains(predecessorId) || insertIds->contains(predecessorId)) {
                    // Find the predecessor and add to the tree
                    QMap<QMailMessageId, QMailMessageThreadedModelItem>::iterator it = _pendingItem.find(predecessorId);
                    if (it != _pendingItem.end()) {
                        insertParent = &(it.value());
                    } else {
                        QMap<QMailMessageId, QMailMessageThreadedModelItem*>::iterator it = _messageItem.find(predecessorId);
                        if (it != _messageItem.end()) {
                            insertParent = it.value();
                        }
                    }
                }
            }

            if (insertParent != 0) {
                int insertIndex = 0;

                // Find the insert location within the parent
                foreach (const QMailMessageId &id, insertParent->childrenIds()) {
                    // If we're being re-added, ignore our own previous position
                    if ((id != messageId) && (!absentIds.contains(id))) {
                        int childPos = newIds.indexOf(id);
                        if ((childPos != -1) && (childPos < messagePos)) {
                            // Ths child precedes us in the sort order
                            ++insertIndex;
                        }
                    }
                }

                // If we have added any children to this parent previously, they precede this one
                QMap<QMailMessageThreadedModelItem*, int>::const_iterator ait = addedChildren.find(insertParent);
                if (ait != addedChildren.end()) {
                    insertIndex += ait.value();
                }

                // This message becomes a new child of the parent
                // Add to the pending list, because we can't actually change the model data here
                QMap<QMailMessageId, QMailMessageThreadedModelItem>::iterator pit = _pendingItem.insert(messageId, QMailMessageThreadedModelItem(messageId, insertParent));
                QMailMessageThreadedModelItem *item = &(pit.value());

                addedChildren[insertParent] += 1;

                QModelIndex parentIndex;
                if (insertParent == &_root) {
                    parentIndex = indexFromItem(insertParent);
                } else {
                    if (_pendingItem.contains(insertParent->_id)) {
                        // The item we're inserting into is a pending item
                        parentIndex = _model.generateIndex(0, 0, static_cast<void*>(insertParent));
                    } else {
                        // The parent item is already existing in the model
                        parentIndex = indexFromItem(insertParent);
                    }
                }

                insertIndices.append(qMakePair(parentIndex, insertIndex));
                insertIds->append(messageId);

                if (descendants.isEmpty()) {
                    messageId = QMailMessageId();
                } else {
                    // Process the descendants of the added node
                    predecessorId = messageId;
                    messageId = descendants.takeLast();
                }
            } else {
                // We need to add the parent before we can be added to it
                descendants.append(messageId);

                messageId = predecessorId;
                predecessorId = predecessor[messageId];
                while (predecessorId.isValid() && !newIds.contains(predecessorId)) {
                    predecessorId = predecessor[predecessorId];
                }
            }
        } while (messageId.isValid());
    }

    *locations = indicesToLocationSequence(insertIndices);
}

bool QMailMessageThreadedModelPrivate::updateLocations(const QMailMessageIdList &ids, 
                                                       QList<LocationSequence> *additions, 
                                                       QList<LocationSequence> *deletions, 
                                                       QList<LocationSequence> *updates,
                                                       QMailMessageIdList *insertIds)
{
    if (!_initialised) {
        // Nothing to do yet
        return true;
    }
    
    if (_ignoreUpdates) {
        // Defer until resynchronised
        _needSynchronize = true;
        return true;
    }

    QSet<QMailMessageId> existingIds(_currentIds.toSet());

    QMailMessageKey idKey(QMailMessageKey::id((existingIds + ids.toSet()).toList()));
    QMailMessageIdList newIds(QMailStore::instance()->queryMessages(_key & idKey, _sortKey));

    QSet<QMailMessageId> currentIds(newIds.toSet());

    // Find which of the messages we must add and remove
    QMailMessageIdList additionIds;
    QMailMessageIdList removalIds;
    QMailMessageIdList temporaryRemovalIds;
    QMailMessageIdList updateIds;

    foreach (const QMailMessageId &id, ids) {
        bool existingMember(existingIds.contains(id));
        bool currentMember(currentIds.contains(id));

        if (!existingMember && currentMember) {
            additionIds.append(id);
        } else if (existingMember && !currentMember) {
            removalIds.append(id);
        } else if (currentMember) {
            updateIds.append(id);
        }
    }

    if (additionIds.isEmpty() && removalIds.isEmpty() && updateIds.isEmpty()) {
        return true;
    }

    // For the updated messages, find if they have a changed predecessor

    // Find all messages involved in conversations with the updated messages, along with their predecessor ID
    QMailMessageKey conversationKey(QMailMessageKey::conversation(QMailMessageKey::id(updateIds)));
    QMailMessageKey::Properties props(QMailMessageKey::Id | QMailMessageKey::InResponseTo);

    QMap<QMailMessageId, QMailMessageId> predecessor;
    foreach (const QMailMessageMetaData &metaData, QMailStore::instance()->messagesMetaData(conversationKey, props)) {
        predecessor.insert(metaData.id(), metaData.inResponseTo());
    }

    QList<QPair<QModelIndex, int> > updateIndices;

    foreach (const QMailMessageId &messageId, updateIds) {
        // Find the first message ancestor that is in our display set
        QMailMessageId predecessorId(predecessor[messageId]);
        while (predecessorId.isValid() && !currentIds.contains(predecessorId)) {
            predecessorId = predecessor[predecessorId];
        }

        bool reinsert(false);

        QMailMessageThreadedModelItem *item = _messageItem[messageId];
        if (item->_parent == &_root) {
            // This is a root item
            if (predecessorId.isValid()) {
                // This item now has a predecessor
                reinsert = true;
            }
        } else {
            if (item->_parent->_id != predecessorId) {
                // This item has a changed predecessor
                reinsert = true;
            }
        }

        if (!reinsert) {
            // We need to see if this item has changed in the sort order
            int row = item->rowInParent();
            int messagePos = newIds.indexOf(messageId);

            QList<QMailMessageThreadedModelItem> &container(item->_parent->_children);
            if (row > 0) {
                // Ensure that we still sort after our immediate predecessor
                if (newIds.indexOf(container.at(row - 1).id()) > messagePos) {
                    reinsert = true;
                }
            }
            if (row < (container.count() - 1)) {
                // Ensure that we still sort before our immediate successor
                if (newIds.indexOf(container.at(row + 1).id()) < messagePos) {
                    reinsert = true;
                }
            }
        }

        if (reinsert) {
            // Remove and re-add this item
            temporaryRemovalIds.append(messageId);
        } else {
            // Find the location for this updated item
            QModelIndex idx(indexFromItem(item));
            updateIndices.append(qMakePair(idx.parent(), item->rowInParent()));
        }
    }

    // Sorting is irrelevant to updates
    *updates = indicesToLocationSequence(updateIndices);

    // Find the locations for removals
    QList<QPair<QModelIndex, int> > removeIndices;
    removalLocations(removalIds, &removeIndices, 0);

    // Find the locations for those to be removed and any children IDs that need to be reinserted after removal
    QMailMessageIdList readditionIds;
    removalLocations(temporaryRemovalIds, &removeIndices, &readditionIds);

    // Sort the removal indices to yield ascending order (they must be deleted in descending order!)
    qSort(removeIndices.begin(), removeIndices.end(), LessThanForRemoval());
    *deletions = indicesToLocationSequence(removeIndices);

    // Find the locations for the added and reinserted messages
    QMailMessageIdList absentIds((removalIds.toSet() + temporaryRemovalIds.toSet() + readditionIds.toSet()).toList());
    additionLocations((additionIds.toSet() + readditionIds.toSet() + temporaryRemovalIds.toSet()).toList(), additions, insertIds, absentIds);

    return true;
}

bool QMailMessageThreadedModelPrivate::removalLocations(const QMailMessageIdList &ids, QList<LocationSequence> *locations)
{
    if (!_initialised) {
        // Nothing to do yet
        return true;
    }
    
    if (_ignoreUpdates) {
        // Defer until resynchronised
        _needSynchronize = true;
        return true;
    }
    
    QList<QPair<QModelIndex, int> > removeIndices;
    removalLocations(ids, &removeIndices, 0);

    // Sort the indices to yield ascending order (they must be deleted in descending order!)
    qSort(removeIndices.begin(), removeIndices.end(), LessThanForRemoval());
    *locations = indicesToLocationSequence(removeIndices);
    return true;
}

void QMailMessageThreadedModelPrivate::removalLocations(const QMailMessageIdList &ids, QList<QPair<QModelIndex, int> > *indices, QMailMessageIdList *readditions)
{
    QSet<QMailMessageId> removedIds;
    QSet<QMailMessageId> childIds;

    foreach (const QMailMessageId &id, ids) {
        if (!removedIds.contains(id)) {
            QMap<QMailMessageId, QMailMessageThreadedModelItem*>::iterator it = _messageItem.find(id);
            if (it != _messageItem.end()) {
                QMailMessageThreadedModelItem *item = it.value();
                QModelIndex idx(indexFromItem(item));

                indices->append(qMakePair(idx.parent(), item->rowInParent()));
                removedIds.insert(id);

                // Find all the children of this item that need to be removed/re-added
                QList<const QMailMessageThreadedModelItem*> items;
                items.append(item);

                while (!items.isEmpty()) {
                    const QMailMessageThreadedModelItem *parent = items.takeFirst();
                    foreach (const QMailMessageThreadedModelItem &child, parent->_children) {
                        if (!removedIds.contains(child._id)) {
                            removedIds.insert(child._id);
                            if (readditions) {
                                childIds.insert(child._id);
                            }

                            items.append(&child);
                        }
                    }
                }
            }
        }
    }
    
    if (readditions) {
        *readditions = childIds.toList();
    }
}

void QMailMessageThreadedModelPrivate::insertItemAt(int row, const QModelIndex &parentIndex, const QMailMessageId &id)
{
    QMailMessageThreadedModelItem *parent;
    if (parentIndex.isValid()) {
        parent = itemFromIndex(parentIndex);
    } else {
        parent = &_root;
    }

    QList<QMailMessageThreadedModelItem> &container(parent->_children);

    container.insert(row, QMailMessageThreadedModelItem(id, parent));
    _messageItem[id] = &(container[row]);
    _currentIds.append(id);
}

void QMailMessageThreadedModelPrivate::removeItemAt(int row, const QModelIndex &parentIndex)
{
    QMailMessageThreadedModelItem *parent;
    if (parentIndex.isValid()) {
        parent = itemFromIndex(parentIndex);
    } else {
        parent = &_root;
    }

    QList<QMailMessageThreadedModelItem> &container(parent->children());

    if (container.count() > row) {
        QMailMessageThreadedModelItem *item = &(container[row]);

        // Find all the descendants of this item that no longer exist
        QList<const QMailMessageThreadedModelItem*> items;
        items.append(item);

        while (!items.isEmpty()) {
            const QMailMessageThreadedModelItem *parent = items.takeFirst();
            foreach (const QMailMessageThreadedModelItem &child, parent->_children) {
                QMailMessageId childId(child.id());
                if (_messageItem.contains(childId)) {
                    _checkedIds.remove(childId);
                    _currentIds.removeAll(childId);
                    _messageItem.remove(childId);
                }

                items.append(&child);
            }
        }

        QMailMessageId id(item->id());

        _checkedIds.remove(id);
        _currentIds.removeAll(id);
        _messageItem.remove(id);
        container.removeAt(row);
    }
}

QModelIndex QMailMessageThreadedModelPrivate::index(int row, int column, const QModelIndex &parentIndex) const
{
    init();

    QMailMessageThreadedModelItem *parent;
    if (parentIndex.isValid()) {
        parent = itemFromIndex(parentIndex);
    } else {
        parent = &_root;
    }

    if (parent && (parent->children().count() > row))
        return _model.generateIndex(row, column, static_cast<void*>(const_cast<QMailMessageThreadedModelItem*>(&(parent->children().at(row)))));

    return QModelIndex();
}

QModelIndex QMailMessageThreadedModelPrivate::parent(const QModelIndex &idx) const
{
    init();

    if (QMailMessageThreadedModelItem *item = itemFromIndex(idx))
        return parentIndex(item, idx.column());

    return QModelIndex();
}

QModelIndex QMailMessageThreadedModelPrivate::updateIndex(const QModelIndex &idx) const
{
    if (idx.isValid()) {
        if (QMailMessageThreadedModelItem *item = static_cast<QMailMessageThreadedModelItem*>(idx.internalPointer())) {
            // This index may refer to a pending item - use the in-model location is there is one
            QMap<QMailMessageId, QMailMessageThreadedModelItem*>::const_iterator it = _messageItem.find(item->id());
            if (it != _messageItem.end()) {
                QMailMessageThreadedModelItem *modelItem = it.value();
                if (modelItem != item) {
                    // Create a new index pointing to the real location
                    return _model.generateIndex(modelItem->rowInParent(), idx.column(), static_cast<void*>(modelItem));
                }
            }
        }
    }

    return idx;
}

void QMailMessageThreadedModelPrivate::init() const
{
    if (!_initialised) {
        _root.children().clear();
        _messageItem.clear();
        _currentIds.clear();

        // Find all messages involved in conversations with the messages to show, along with their predecessor ID
        QMailMessageKey conversationKey(QMailMessageKey::conversation(_key));
        QMailMessageKey::Properties props(QMailMessageKey::Id | QMailMessageKey::InResponseTo);

        QMap<QMailMessageId, QMailMessageId> predecessor;
        foreach (const QMailMessageMetaData &metaData, QMailStore::instance()->messagesMetaData(conversationKey, props)) {
            predecessor.insert(metaData.id(), metaData.inResponseTo());
        }

        // Now find all the messages we're going to show, in order
        QMailMessageIdList ids = QMailStore::instance()->queryMessages(_key, _sortKey);

        // Process the messages to build a tree
        foreach (const QMailMessageId& id, ids) {
            // See if we have already added this message
            QMap<QMailMessageId, QMailMessageThreadedModelItem*>::iterator it = _messageItem.find(id);
            if (it != _messageItem.end())
                continue;

            QMailMessageId messageId(id);
            QList<QMailMessageId> descendants;

            // Find the first message ancestor that is in our display set
            QMailMessageId predecessorId(predecessor[messageId]);
            while (predecessorId.isValid() && !ids.contains(predecessorId)) {
                predecessorId = predecessor[predecessorId];
            }

            do {
                QMailMessageThreadedModelItem *insertParent = 0;

                if (!predecessorId.isValid()) {
                    // This message is a root node
                    insertParent = &_root;
                } else {
                    // Find the predecessor and add to the tree
                    it = _messageItem.find(predecessorId);
                    if (it != _messageItem.end()) {
                        // Add this message to its parent
                        insertParent = it.value();
                    }
                }

                if (insertParent != 0) {
                    // Append the message to the existing children of the parent
                    QList<QMailMessageThreadedModelItem> &container(insertParent->children());

                    container.append(QMailMessageThreadedModelItem(messageId, insertParent));
                    _messageItem[messageId] = &(container.last());
                    _currentIds.append(messageId);
                    
                    if (descendants.isEmpty()) {
                        messageId = QMailMessageId();
                    } else {
                        // Process the descendants of the added node
                        predecessorId = messageId;
                        messageId = descendants.takeLast();
                    }
                } else {
                    // We need to add the parent before we can be added to it
                    descendants.append(messageId);

                    messageId = predecessorId;
                    predecessorId = predecessor[messageId];
                    while (predecessorId.isValid() && !ids.contains(predecessorId)) {
                        predecessorId = predecessor[predecessorId];
                    }
                }
            } while (messageId.isValid());
        }

        _initialised = true;
        _needSynchronize = false;
    }
}

QModelIndex QMailMessageThreadedModelPrivate::parentIndex(const QMailMessageThreadedModelItem *item, int column) const
{
    if (const QMailMessageThreadedModelItem *parent = item->_parent)
        if (parent->_parent != 0)
            return index(parent, column);

    return QModelIndex();
}

QModelIndex QMailMessageThreadedModelPrivate::index(const QMailMessageThreadedModelItem *item, int column) const
{
    if (item->_parent)
        return _model.generateIndex(item->rowInParent(), column, const_cast<void*>(static_cast<const void*>(item)));

    return QModelIndex();
}

QMailMessageThreadedModelItem *QMailMessageThreadedModelPrivate::itemFromIndex(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<QMailMessageThreadedModelItem*>(index.internalPointer());

    return 0;
}

QModelIndex QMailMessageThreadedModelPrivate::indexFromItem(const QMailMessageThreadedModelItem *item) const
{
    return index(item, 0);
}


/*!
    Constructs a QMailMessageThreadedModel with a parent \a parent.

    By default, the model will match all messages in the database, and display them in
    the order they were submitted, and mail store updates are not ignored.

    \sa setKey(), setSortKey(), setIgnoreMailStoreUpdates()
*/
QMailMessageThreadedModel::QMailMessageThreadedModel(QObject* parent)
    : QMailMessageModelBase(parent),
      d(new QMailMessageThreadedModelPrivate(*this, QMailMessageKey::nonMatchingKey(), QMailMessageSortKey::id(), false))
{
}

/*!
    Deletes the QMailMessageThreadedModel object.
*/
QMailMessageThreadedModel::~QMailMessageThreadedModel()
{
    delete d; d = 0;
}

// TODO - is this useful?  Maybe "QMailMessageId rootAncestor()" instead?
int QMailMessageThreadedModel::rootRow(const QModelIndex& idx) const
{
    return d->rootRow(idx);
}

/*! \reimp */
QModelIndex QMailMessageThreadedModel::index(int row, int column, const QModelIndex &parentIndex) const
{
    return d->index(row, column, parentIndex);
}

/*! \reimp */
QModelIndex QMailMessageThreadedModel::parent(const QModelIndex &idx) const
{
    return d->parent(idx);
}

/*! \reimp */
QModelIndex QMailMessageThreadedModel::generateIndex(int row, int column, void *ptr)
{
    return createIndex(row, column, ptr);
}

/*! \reimp */
QMailMessageModelImplementation *QMailMessageThreadedModel::impl()
{
    return d;
}

/*! \reimp */
const QMailMessageModelImplementation *QMailMessageThreadedModel::impl() const
{
    return d;
}
