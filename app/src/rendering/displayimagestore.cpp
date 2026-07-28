// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "displayimagestore.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"
#include <QMutex>
#include <QMutexLocker>
#include <QtGlobal>
#include <algorithm>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <utility>

namespace kiriview {
namespace {
    int priorityRank(DisplayImageRetentionPriority priority)
    {
        switch (priority) {
        case DisplayImageRetentionPriority::Background:
            return 0;
        case DisplayImageRetentionPriority::Nearby:
            return 1;
        case DisplayImageRetentionPriority::Visible:
            return 2;
        }

        return 0;
    }

    QSize normalizedOriginalSize(const DisplayImageEntry& entry)
    {
        if (entry.originalSize.isValid() && !entry.originalSize.isEmpty()) {
            return entry.originalSize;
        }
        if (entry.rasterSize.isValid() && !entry.rasterSize.isEmpty()) {
            return entry.rasterSize;
        }
        return entry.image.size();
    }

    QSize normalizedRasterSize(const DisplayImageEntry& entry)
    {
        if (entry.rasterSize.isValid() && !entry.rasterSize.isEmpty()) {
            return entry.rasterSize;
        }
        return entry.image.size();
    }

    int compareSize(const QSize& left, const QSize& right)
    {
        if (left.width() != right.width()) {
            return left.width() < right.width() ? -1 : 1;
        }
        if (left.height() != right.height()) {
            return left.height() < right.height() ? -1 : 1;
        }
        return 0;
    }

    struct DisplayImageReuseKeyLess
    {
        bool operator()(const DisplayImageReuseKey& left, const DisplayImageReuseKey& right) const
        {
            if (left.locationIdentity != right.locationIdentity) {
                return left.locationIdentity < right.locationIdentity;
            }
            if (left.sourceIdentity != right.sourceIdentity) {
                return left.sourceIdentity < right.sourceIdentity;
            }
            if (left.sourceRevision != right.sourceRevision) {
                return left.sourceRevision.digest() < right.sourceRevision.digest();
            }
            if (left.rasterIdentity.kind != right.rasterIdentity.kind) {
                return static_cast<int>(left.rasterIdentity.kind)
                    < static_cast<int>(right.rasterIdentity.kind);
            }
            if (left.rasterIdentity.authoredFrame != right.rasterIdentity.authoredFrame) {
                return left.rasterIdentity.authoredFrame < right.rasterIdentity.authoredFrame;
            }
            const int leftTransform = static_cast<int>(left.imageReaderTransformations);
            const int rightTransform = static_cast<int>(right.imageReaderTransformations);
            if (leftTransform != rightTransform) {
                return leftTransform < rightTransform;
            }
            if (const int originalSizeComparison
                = compareSize(left.originalSize, right.originalSize);
                originalSizeComparison != 0) {
                return originalSizeComparison < 0;
            }
            if (const int rasterSizeComparison = compareSize(left.rasterSize, right.rasterSize);
                rasterSizeComparison != 0) {
                return rasterSizeComparison < 0;
            }
            if (left.quality != right.quality) {
                return static_cast<int>(left.quality) < static_cast<int>(right.quality);
            }
            if (left.previewOrigin != right.previewOrigin) {
                return static_cast<int>(left.previewOrigin) < static_cast<int>(right.previewOrigin);
            }
            return static_cast<int>(left.pageRole) < static_cast<int>(right.pageRole);
        }
    };

}

DisplayImageRasterIdentity DisplayImageRasterIdentity::provisionalPreview()
{
    return { DisplayImageRasterKind::ProvisionalPreview, -1 };
}

DisplayImageRasterIdentity DisplayImageRasterIdentity::authoritativeStill()
{
    return { DisplayImageRasterKind::AuthoritativeStill, -1 };
}

DisplayImageRasterIdentity DisplayImageRasterIdentity::timedFrame(int authoredFrame)
{
    return { DisplayImageRasterKind::TimedFrame, authoredFrame };
}

DisplayImageRasterIdentity DisplayImageRasterIdentity::refinement()
{
    return { DisplayImageRasterKind::Refinement, -1 };
}

bool DisplayImageRasterIdentity::isValid() const
{
    return kind == DisplayImageRasterKind::TimedFrame ? authoredFrame >= 0 : authoredFrame == -1;
}

class DisplayImageStore::Private
{
public:
    struct EvictionKey
    {
        int priority = 0;
        quint64 lastUse = 0;
        QString id;
    };

    struct EvictionKeyLess
    {
        bool operator()(const EvictionKey& left, const EvictionKey& right) const
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

    struct Entry
    {
        QString id;
        QImage image;
        QSize originalSize;
        QSize rasterSize;
        DisplayImageQuality quality = DisplayImageQuality::Exact;
        DisplayImageRetentionPriority priority = DisplayImageRetentionPriority::Nearby;
        qsizetype byteCost = 0;
        quint64 lastUse = 0;
        int frameLeaseCount = 0;
        DisplayImageReuseKey reuseKey;
        std::optional<EvictionKey> evictionKey;
    };

    using EntryList = std::list<Entry>;
    using EntryIterator = EntryList::iterator;

    mutable QMutex mutex;
    EntryList images;
    std::map<QString, EntryIterator> entriesById;
    std::map<DisplayImageReuseKey, EntryIterator, DisplayImageReuseKeyLess> entriesByReuseKey;
    std::map<EvictionKey, EntryIterator, EvictionKeyLess> evictionEntries;
    qsizetype byteBudget = 0;
    qsizetype byteCost = 0;
    mutable quint64 useClock = 0;
    quint64 nextId = 1;

    EntryIterator findEntry(const QString& id)
    {
        const auto entry = entriesById.find(id);
        return entry == entriesById.end() ? images.end() : entry->second;
    }

    EntryIterator findReusableEntry(const DisplayImageReuseKey& reuseKey)
    {
        const auto entry = entriesByReuseKey.find(reuseKey);
        return entry == entriesByReuseKey.end() ? images.end() : entry->second;
    }

    void removeEvictionIndex(Entry& entry)
    {
        if (!entry.evictionKey.has_value()) {
            return;
        }

        const auto indexed = evictionEntries.find(*entry.evictionKey);
        if (indexed != evictionEntries.end() && indexed->second->id == entry.id) {
            evictionEntries.erase(indexed);
        }
        entry.evictionKey = std::nullopt;
    }

    EvictionKey evictionKeyFor(const Entry& entry) const
    {
        return EvictionKey {
            priorityRank(entry.priority),
            entry.lastUse,
            entry.id,
        };
    }

    void addEvictionIndex(EntryIterator entry)
    {
        if (entry->frameLeaseCount > 0) {
            entry->evictionKey = std::nullopt;
            return;
        }

        EvictionKey evictionKey = evictionKeyFor(*entry);
        entry->evictionKey = evictionKey;
        evictionEntries.emplace(std::move(evictionKey), entry);
    }

    void touchEntry(EntryIterator entry)
    {
        removeEvictionIndex(*entry);
        entry->lastUse = ++useClock;
        addEvictionIndex(entry);
    }

    void touchPinnedEntry(EntryIterator entry)
    {
        removeEvictionIndex(*entry);
        entry->lastUse = ++useClock;
    }

    DisplayImageStoreEntry publicEntry(const Entry& entry) const
    {
        return DisplayImageStoreEntry {
            entry.image,
            entry.originalSize,
            entry.rasterSize,
            entry.reuseKey.imageReaderTransformations,
            entry.quality,
            entry.byteCost,
        };
    }

    QString nextEntryId()
    {
        QString id;
        do {
            id = QStringLiteral("display-%1").arg(nextId++);
            if (nextId == 0) {
                ++nextId;
            }
        } while (entriesById.contains(id));
        return id;
    }

    void indexEntry(EntryIterator entry)
    {
        entriesById.emplace(entry->id, entry);
        entriesByReuseKey.emplace(entry->reuseKey, entry);
        addEvictionIndex(entry);
    }

    QString insertNewEntry(
        DisplayImageEntry entry, qsizetype entryByteCost, DisplayImageReuseKey reuseKey)
    {
        const QString id = nextEntryId();
        const QSize originalSize = normalizedOriginalSize(entry);
        const QSize rasterSize = normalizedRasterSize(entry);

        images.push_back(Entry {
            id,
            std::move(entry.image),
            originalSize,
            rasterSize,
            entry.quality,
            entry.priority,
            entryByteCost,
            ++useClock,
            0,
            std::move(reuseKey),
            std::nullopt,
        });
        auto inserted = std::prev(images.end());
        byteCost = saturatedQtByteSum(byteCost, entryByteCost);
        indexEntry(inserted);
        trimToBudget();
        return entriesById.contains(id) ? id : QString();
    }

    void removeEntry(EntryIterator entry)
    {
        removeEvictionIndex(*entry);
        entriesById.erase(entry->id);
        entriesByReuseKey.erase(entry->reuseKey);
        byteCost -= entry->byteCost;
        images.erase(entry);
    }

    void trimToBudget()
    {
        while (byteCost > byteBudget) {
            if (evictionEntries.empty()) {
                return;
            }
            removeEntry(evictionEntries.begin()->second);
        }
    }
};

DisplayImageStore::DisplayImageStore(qsizetype byteBudget)
    : d(std::make_unique<Private>())
{
    d->byteBudget = std::max<qsizetype>(0, byteBudget);
}

DisplayImageStore::~DisplayImageStore() = default;

QString DisplayImageStore::acquireReusable(DisplayImageEntry entry, DisplayImageReuseKey reuseKey)
{
    if (entry.image.isNull() || reuseKey.locationIdentity.isEmpty()
        || reuseKey.sourceIdentity.isEmpty() || !reuseKey.sourceRevision.isValid()
        || !reuseKey.rasterIdentity.isValid()) {
        return {};
    }

    const qsizetype byteCost = imageByteCost(entry.image);
    QMutexLocker locker(&d->mutex);
    auto reusable = d->findReusableEntry(reuseKey);
    if (reusable != d->images.end()) {
        reusable->priority = entry.priority;
        d->touchEntry(reusable);
        d->trimToBudget();
        return reusable->id;
    }

    if (byteCost <= 0 || byteCost > d->byteBudget) {
        return {};
    }

    return d->insertNewEntry(std::move(entry), byteCost, std::move(reuseKey));
}

std::optional<DisplayImageStoreEntry> DisplayImageStore::entry(const QString& id) const
{
    if (id.isEmpty()) {
        return std::nullopt;
    }

    QMutexLocker locker(&d->mutex);
    auto entry = d->findEntry(id);
    if (entry == d->images.end()) {
        return std::nullopt;
    }

    d->touchEntry(entry);
    return d->publicEntry(*entry);
}

bool DisplayImageStore::acquireFrameLease(const QString& id)
{
    if (id.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&d->mutex);
    auto entry = d->findEntry(id);
    if (entry == d->images.end()) {
        return false;
    }

    ++entry->frameLeaseCount;
    d->touchPinnedEntry(entry);
    return true;
}

void DisplayImageStore::releaseFrameLease(const QString& id)
{
    if (id.isEmpty()) {
        return;
    }

    QMutexLocker locker(&d->mutex);
    auto entry = d->findEntry(id);
    if (entry == d->images.end()) {
        return;
    }

    if (entry->frameLeaseCount > 0) {
        --entry->frameLeaseCount;
    }
    d->touchEntry(entry);
    d->trimToBudget();
}

qsizetype DisplayImageStore::byteBudget() const
{
    QMutexLocker locker(&d->mutex);
    return d->byteBudget;
}

qsizetype DisplayImageStore::byteCost() const
{
    QMutexLocker locker(&d->mutex);
    return d->byteCost;
}

qsizetype DisplayImageStore::size() const
{
    QMutexLocker locker(&d->mutex);
    return static_cast<qsizetype>(d->images.size());
}

}
