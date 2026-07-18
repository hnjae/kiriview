// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "thumbnailimagestore.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"
#include "cache/imagecachepolicy.h"
#include "session/thumbnaillogging.h"

#include <QDebug>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <iterator>
#include <list>
#include <memory>
#include <set>
#include <utility>

namespace kiriview {
namespace {
    int priorityRank(ThumbnailImageRetentionPriority priority)
    {
        switch (priority) {
        case ThumbnailImageRetentionPriority::Background:
            return 0;
        case ThumbnailImageRetentionPriority::Nearby:
            return 1;
        case ThumbnailImageRetentionPriority::Visible:
            return 2;
        }

        return 0;
    }

    qsizetype defaultThumbnailStoreByteBudget() { return thumbnailCachePreferredByteBudget(); }
}

class ThumbnailImageStore::Private
{
public:
    struct Entry
    {
        QString id;
        QImage image;
        qsizetype byteCost = 0;
        quint64 lastUse = 0;
        ThumbnailImageRetentionPriority priority = ThumbnailImageRetentionPriority::Nearby;
    };

    struct EvictionRecord
    {
        int priority = 0;
        quint64 lastUse = 0;
        QString id;
    };

    struct EvictionRecordLess
    {
        bool operator()(const EvictionRecord& left, const EvictionRecord& right) const
        {
            if (left.priority != right.priority) {
                return left.priority < right.priority;
            }
            if (left.lastUse != right.lastUse) {
                return left.lastUse < right.lastUse;
            }
            return left.id < right.id;
        }
    };

    using EntryList = std::list<Entry>;
    using EntryIterator = EntryList::iterator;

    mutable QMutex mutex;
    EntryList images;
    QHash<QString, EntryIterator> entriesById;
    std::set<EvictionRecord, EvictionRecordLess> evictionOrder;
    qsizetype byteBudget = 0;
    qsizetype byteCost = 0;
    mutable quint64 useClock = 0;
    quint64 nextId = 1;

    EvictionRecord evictionRecord(const Entry& entry) const
    {
        return EvictionRecord {
            priorityRank(entry.priority),
            entry.lastUse,
            entry.id,
        };
    }

    EntryIterator findEntry(const QString& id)
    {
        auto indexed = entriesById.constFind(id);
        return indexed == entriesById.cend() ? images.end() : indexed.value();
    }

    void insertIndexes(EntryIterator entry)
    {
        entriesById.insert(entry->id, entry);
        evictionOrder.insert(evictionRecord(*entry));
    }

    void eraseEntry(EntryIterator entry)
    {
        evictionOrder.erase(evictionRecord(*entry));
        entriesById.remove(entry->id);
        byteCost -= entry->byteCost;
        images.erase(entry);
    }

    void refreshEntry(EntryIterator entry, ThumbnailImageRetentionPriority priority)
    {
        evictionOrder.erase(evictionRecord(*entry));
        entry->priority = priority;
        entry->lastUse = ++useClock;
        evictionOrder.insert(evictionRecord(*entry));
    }

    void trimToBudget()
    {
        while (byteCost > byteBudget && !images.empty()) {
            if (evictionOrder.empty()) {
                return;
            }

            const QString removableId = evictionOrder.cbegin()->id;
            EntryIterator removable = findEntry(removableId);
            if (removable == images.end()) {
                evictionOrder.erase(evictionOrder.cbegin());
                continue;
            }

            qCDebug(kiriviewThumbnailLog)
                << "Evicting thumbnail image from store" << removable->id << "priority"
                << priorityRank(removable->priority) << "bytes" << removable->byteCost;
            eraseEntry(removable);
        }
    }
};

ThumbnailImageStore::ThumbnailImageStore(qsizetype byteBudget)
    : d(std::make_unique<Private>())
{
    d->byteBudget = byteBudget > 0 ? byteBudget : defaultThumbnailStoreByteBudget();
}

ThumbnailImageStore::~ThumbnailImageStore() = default;

QString ThumbnailImageStore::insert(QImage image, ThumbnailImageRetentionPriority priority)
{
    if (image.isNull()) {
        return {};
    }

    const qsizetype byteCost = imageByteCost(image);
    QMutexLocker locker(&d->mutex);
    if (byteCost <= 0 || byteCost > d->byteBudget) {
        return {};
    }

    QString id;
    do {
        id = QStringLiteral("thumbnail-%1").arg(d->nextId++);
        if (d->nextId == 0) {
            ++d->nextId;
        }
    } while (d->entriesById.contains(id));

    d->images.push_back(Private::Entry {
        id,
        std::move(image),
        byteCost,
        ++d->useClock,
        priority,
    });
    d->insertIndexes(std::prev(d->images.end()));
    d->byteCost = saturatedQtByteSum(d->byteCost, byteCost);
    d->trimToBudget();
    return d->entriesById.contains(id) ? id : QString();
}

void ThumbnailImageStore::updatePriority(
    const QString& id, ThumbnailImageRetentionPriority priority)
{
    if (id.isEmpty()) {
        return;
    }

    QMutexLocker locker(&d->mutex);
    Private::EntryIterator entry = d->findEntry(id);
    if (entry == d->images.end()) {
        return;
    }

    d->refreshEntry(entry, priority);
    d->trimToBudget();
}

void ThumbnailImageStore::release(const QString& id)
{
    if (id.isEmpty()) {
        return;
    }

    QMutexLocker locker(&d->mutex);
    Private::EntryIterator entry = d->findEntry(id);
    if (entry == d->images.end()) {
        return;
    }

    d->eraseEntry(entry);
}

void ThumbnailImageStore::clear()
{
    QMutexLocker locker(&d->mutex);
    d->images.clear();
    d->entriesById.clear();
    d->evictionOrder.clear();
    d->byteCost = 0;
}

void ThumbnailImageStore::setByteBudget(qsizetype byteBudget)
{
    QMutexLocker locker(&d->mutex);
    d->byteBudget = byteBudget > 0 ? byteBudget : defaultThumbnailStoreByteBudget();
    d->trimToBudget();
}

QImage ThumbnailImageStore::image(const QString& id) const
{
    QMutexLocker locker(&d->mutex);
    Private::EntryIterator entry = d->findEntry(id);
    if (entry == d->images.end()) {
        return {};
    }

    d->refreshEntry(entry, entry->priority);
    return entry->image;
}

qsizetype ThumbnailImageStore::byteBudget() const
{
    QMutexLocker locker(&d->mutex);
    return d->byteBudget;
}

qsizetype ThumbnailImageStore::byteCost() const
{
    QMutexLocker locker(&d->mutex);
    return d->byteCost;
}

qsizetype ThumbnailImageStore::size() const
{
    QMutexLocker locker(&d->mutex);
    return static_cast<qsizetype>(d->images.size());
}

ThumbnailImageProvider::ThumbnailImageProvider(std::shared_ptr<ThumbnailImageStore> store)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_store(std::move(store))
{
}

QImage ThumbnailImageProvider::requestImage(
    const QString& id, QSize* size, const QSize& requestedSize)
{
    Q_UNUSED(requestedSize)

    const QImage image = m_store == nullptr ? QImage() : m_store->image(id);
    if (size != nullptr) {
        *size = image.size();
    }
    return image;
}

std::shared_ptr<ThumbnailImageStore> sharedThumbnailImageStore()
{
    static const std::shared_ptr<ThumbnailImageStore> store
        = std::make_shared<ThumbnailImageStore>(defaultThumbnailStoreByteBudget());
    return store;
}

void configureSharedThumbnailImageStoreByteBudget(qsizetype byteBudget)
{
    sharedThumbnailImageStore()->setByteBudget(byteBudget);
}

QUrl thumbnailImageSourceForId(const QString& id)
{
    if (id.isEmpty()) {
        return {};
    }

    return QUrl(QStringLiteral("image://kiriview-thumbnails/%1").arg(id));
}
}
