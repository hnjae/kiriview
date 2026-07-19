// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "displayimagestore.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"
#include "cache/imagecachepolicy.h"
#include <QMutex>
#include <QMutexLocker>
#include <QtGlobal>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <utility>

namespace kiriview {
bool operator==(const DisplayImageReuseKey& left, const DisplayImageReuseKey& right)
{
    return left.locationIdentity == right.locationIdentity
        && left.sourceIdentity == right.sourceIdentity
        && left.imageReaderTransformations == right.imageReaderTransformations
        && left.originalSize == right.originalSize && left.rasterSize == right.rasterSize
        && left.quality == right.quality && left.previewOrigin == right.previewOrigin
        && left.pageRole == right.pageRole;
}

bool operator!=(const DisplayImageReuseKey& left, const DisplayImageReuseKey& right)
{
    return !(left == right);
}

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

    qsizetype defaultDisplayStoreByteBudget() { return displayImageCachePreferredByteBudget(); }

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
        QString sourceIdentity;
        DisplayedPageRole pageRole = DisplayedPageRole::Primary;
        DisplayImageQuality quality = DisplayImageQuality::Exact;
        DisplayImageRetentionPriority priority = DisplayImageRetentionPriority::Nearby;
        qsizetype byteCost = 0;
        quint64 generation = 0;
        QString debugLabel;
        DisplayImagePreviewOrigin previewOrigin = DisplayImagePreviewOrigin::None;
        quint64 lastUse = 0;
        bool releaseRequested = false;
        int visiblePins = 0;
        int staleRetainedPins = 0;
        int pendingLoadPins = 0;
        int frameRetentionPins = 0;
        int bufferedDisplayPins = 0;
        std::optional<DisplayImageReuseKey> reuseKey;
        std::optional<EvictionKey> evictionKey;

        int totalPinCount() const
        {
            return visiblePins + staleRetainedPins + pendingLoadPins + frameRetentionPins
                + bufferedDisplayPins;
        }
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
    mutable qsizetype lastIdLookupEntryScanCount = 0;
    mutable qsizetype lastReuseLookupEntryScanCount = 0;

    EntryIterator findEntry(const QString& id)
    {
        lastIdLookupEntryScanCount = 0;
        const auto entry = entriesById.find(id);
        return entry == entriesById.end() ? images.end() : entry->second;
    }

    EntryIterator findReusableEntry(const DisplayImageReuseKey& reuseKey)
    {
        lastReuseLookupEntryScanCount = 0;
        const auto entry = entriesByReuseKey.find(reuseKey);
        return entry == entriesByReuseKey.end() ? images.end() : entry->second;
    }

    int& pinCount(Entry& entry, DisplayImagePinKind kind)
    {
        switch (kind) {
        case DisplayImagePinKind::Visible:
            return entry.visiblePins;
        case DisplayImagePinKind::StaleRetained:
            return entry.staleRetainedPins;
        case DisplayImagePinKind::PendingLoad:
            return entry.pendingLoadPins;
        case DisplayImagePinKind::FrameRetention:
            return entry.frameRetentionPins;
        case DisplayImagePinKind::BufferedDisplay:
            return entry.bufferedDisplayPins;
        }

        return entry.visiblePins;
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
        if (entry->totalPinCount() > 0) {
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
            entry.id,
            entry.image,
            entry.originalSize,
            entry.rasterSize,
            entry.sourceIdentity,
            entry.pageRole,
            entry.quality,
            entry.priority,
            entry.byteCost,
            entry.generation,
            entry.debugLabel,
            entry.previewOrigin,
            entry.reuseKey,
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
        } while (entriesById.find(id) != entriesById.end());
        return id;
    }

    void indexEntry(EntryIterator entry)
    {
        entriesById.emplace(entry->id, entry);
        if (entry->reuseKey.has_value()) {
            entriesByReuseKey.emplace(*entry->reuseKey, entry);
        }
        addEvictionIndex(entry);
    }

    QString insertNewEntry(DisplayImageEntry entry, qsizetype entryByteCost,
        std::optional<DisplayImageReuseKey> reuseKey = std::nullopt)
    {
        const QString id = nextEntryId();
        const QSize originalSize = normalizedOriginalSize(entry);
        const QSize rasterSize = normalizedRasterSize(entry);

        images.push_back(Entry {
            id,
            std::move(entry.image),
            originalSize,
            rasterSize,
            std::move(entry.sourceIdentity),
            entry.pageRole,
            entry.quality,
            entry.priority,
            entryByteCost,
            entry.generation,
            std::move(entry.debugLabel),
            entry.previewOrigin,
            ++useClock,
            false,
            0,
            0,
            0,
            0,
            0,
            std::move(reuseKey),
            std::nullopt,
        });
        auto inserted = std::prev(images.end());
        byteCost = saturatedQtByteSum(byteCost, entryByteCost);
        indexEntry(inserted);
        trimToBudget();
        return entriesById.find(id) == entriesById.end() ? QString() : id;
    }

    void removeEntry(EntryIterator entry)
    {
        removeEvictionIndex(*entry);
        entriesById.erase(entry->id);
        if (entry->reuseKey.has_value()) {
            entriesByReuseKey.erase(*entry->reuseKey);
        }
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
    d->byteBudget = byteBudget > 0 ? byteBudget : defaultDisplayStoreByteBudget();
}

DisplayImageStore::~DisplayImageStore() = default;

QString DisplayImageStore::insert(DisplayImageEntry entry)
{
    if (entry.image.isNull()) {
        return {};
    }

    const qsizetype byteCost = imageByteCost(entry.image);
    QMutexLocker locker(&d->mutex);
    if (byteCost <= 0 || byteCost > d->byteBudget) {
        return {};
    }

    return d->insertNewEntry(std::move(entry), byteCost);
}

QString DisplayImageStore::acquireReusable(DisplayImageEntry entry, DisplayImageReuseKey reuseKey)
{
    if (entry.image.isNull()) {
        return {};
    }

    const qsizetype byteCost = imageByteCost(entry.image);
    QMutexLocker locker(&d->mutex);
    auto reusable = d->findReusableEntry(reuseKey);
    if (reusable != d->images.end()) {
        reusable->priority = entry.priority;
        reusable->releaseRequested = false;
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

void DisplayImageStore::updatePriority(const QString& id, DisplayImageRetentionPriority priority)
{
    if (id.isEmpty()) {
        return;
    }

    QMutexLocker locker(&d->mutex);
    auto entry = d->findEntry(id);
    if (entry == d->images.end()) {
        return;
    }

    entry->priority = priority;
    d->touchEntry(entry);
    d->trimToBudget();
}

bool DisplayImageStore::acquirePinLease(const QString& id, DisplayImagePinKind kind)
{
    if (id.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&d->mutex);
    auto entry = d->findEntry(id);
    if (entry == d->images.end()) {
        return false;
    }

    ++d->pinCount(*entry, kind);
    d->touchPinnedEntry(entry);
    return true;
}

void DisplayImageStore::releasePinLease(const QString& id, DisplayImagePinKind kind)
{
    if (id.isEmpty()) {
        return;
    }

    QMutexLocker locker(&d->mutex);
    auto entry = d->findEntry(id);
    if (entry == d->images.end()) {
        return;
    }

    int& pinCount = d->pinCount(*entry, kind);
    if (pinCount > 0) {
        --pinCount;
    }
    if (entry->releaseRequested && entry->totalPinCount() == 0) {
        d->removeEntry(entry);
        return;
    }
    d->touchEntry(entry);
    d->trimToBudget();
}

void DisplayImageStore::release(const QString& id)
{
    if (id.isEmpty()) {
        return;
    }

    QMutexLocker locker(&d->mutex);
    auto entry = d->findEntry(id);
    if (entry == d->images.end()) {
        return;
    }

    if (entry->totalPinCount() > 0) {
        entry->releaseRequested = true;
        d->touchPinnedEntry(entry);
        return;
    }

    d->removeEntry(entry);
}

void DisplayImageStore::clear()
{
    QMutexLocker locker(&d->mutex);
    d->images.clear();
    d->entriesById.clear();
    d->entriesByReuseKey.clear();
    d->evictionEntries.clear();
    d->byteCost = 0;
}

void DisplayImageStore::setByteBudget(qsizetype byteBudget)
{
    QMutexLocker locker(&d->mutex);
    d->byteBudget = byteBudget > 0 ? byteBudget : defaultDisplayStoreByteBudget();
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

DisplayImageStoreDebugStats DisplayImageStore::debugStats() const
{
    QMutexLocker locker(&d->mutex);
    return DisplayImageStoreDebugStats {
        static_cast<qsizetype>(d->images.size()),
        static_cast<qsizetype>(d->entriesById.size()),
        static_cast<qsizetype>(d->entriesByReuseKey.size()),
        static_cast<qsizetype>(d->evictionEntries.size()),
        d->byteCost,
        d->lastIdLookupEntryScanCount,
        d->lastReuseLookupEntryScanCount,
    };
}

std::shared_ptr<DisplayImageStore> sharedDisplayImageStore()
{
    static const std::shared_ptr<DisplayImageStore> store
        = std::make_shared<DisplayImageStore>(defaultDisplayStoreByteBudget());
    return store;
}

void configureSharedDisplayImageStoreByteBudget(qsizetype byteBudget)
{
    sharedDisplayImageStore()->setByteBudget(byteBudget);
}
}
