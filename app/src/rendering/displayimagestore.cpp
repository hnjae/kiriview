// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "displayimagestore.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"
#include "decoding/imagedecodelogging.h"
#include <QColorSpace>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QPoint>
#include <QtGlobal>
#include <algorithm>
#include <atomic>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace kiriview {
namespace {
    enum class DisplayImageOutputReservationFailureReason {
        SingleOutputLimit,
        AggregateLimit,
    };

    struct DisplayImageOutputReservationDiagnostics
    {
        qsizetype chargedBytes = 0;
        qsizetype residentEntries = 0;
        qsizetype residentBytes = 0;
        qsizetype evictableEntries = 0;
        qsizetype evictableBytes = 0;
        qsizetype nonEvictableEntries = 0;
        qsizetype nonEvictableBytes = 0;
        qsizetype leasedEntries = 0;
        qsizetype leasedBytes = 0;
        qsizetype externalAliasUnleasedEntries = 0;
        qsizetype externalAliasUnleasedBytes = 0;
        qsizetype outstandingAdmissionBytes = 0;
    };

    const char* reservationOriginName(DisplayImageOutputReservationOrigin origin)
    {
        switch (origin) {
        case DisplayImageOutputReservationOrigin::Unspecified:
            return "unspecified";
        case DisplayImageOutputReservationOrigin::InitialStaticOutput:
            return "initial-static-output";
        case DisplayImageOutputReservationOrigin::StaticRefinementOutput:
            return "static-refinement-output";
        case DisplayImageOutputReservationOrigin::AnimationOutput:
            return "animation-output";
        case DisplayImageOutputReservationOrigin::ProviderPreparationOutput:
            return "provider-preparation-output";
        }
        return "unspecified";
    }

    const char* reservationFailureReasonName(DisplayImageOutputReservationFailureReason reason)
    {
        switch (reason) {
        case DisplayImageOutputReservationFailureReason::SingleOutputLimit:
            return "single-output-limit";
        case DisplayImageOutputReservationFailureReason::AggregateLimit:
            return "aggregate-limit";
        }
        return "aggregate-limit";
    }

    void logOutputReservationFailure(DisplayImageOutputReservationOrigin origin,
        DisplayImageOutputReservationFailureReason reason, qsizetype requestedBytes,
        qsizetype budgetBytes, const DisplayImageOutputReservationDiagnostics& diagnostics)
    {
        qCWarning(kiriviewDecodeLog)
            << "display output reservation rejected"
            << "origin" << reservationOriginName(origin) << "reason"
            << reservationFailureReasonName(reason) << "requestedBytes" << requestedBytes
            << "budgetBytes" << budgetBytes << "chargedBytes" << diagnostics.chargedBytes
            << "residentEntries" << diagnostics.residentEntries << "residentBytes"
            << diagnostics.residentBytes << "evictableEntries" << diagnostics.evictableEntries
            << "evictableBytes" << diagnostics.evictableBytes << "nonEvictableEntries"
            << diagnostics.nonEvictableEntries << "nonEvictableBytes"
            << diagnostics.nonEvictableBytes << "leasedEntries" << diagnostics.leasedEntries
            << "leasedBytes" << diagnostics.leasedBytes << "externalAliasUnleasedEntries"
            << diagnostics.externalAliasUnleasedEntries << "externalAliasUnleasedBytes"
            << diagnostics.externalAliasUnleasedBytes << "outstandingAdmissionBytes"
            << diagnostics.outstandingAdmissionBytes;
    }

    struct DisplayImageOutputLedger
    {
        std::atomic<qsizetype> byteCost = 0;
    };

    struct AdmittedImageBacking
    {
        std::shared_ptr<kiriview::DisplayImageOutputAdmission> outputAdmission;
        QImage image;
    };

    void deleteAdmittedImageBacking(void* data) { delete static_cast<AdmittedImageBacking*>(data); }

    QImage imageRetainingOutputAdmission(
        QImage image, const std::shared_ptr<kiriview::DisplayImageOutputAdmission>& outputAdmission)
    {
        if (image.isNull() || outputAdmission == nullptr) {
            return {};
        }

        auto* backing = new AdmittedImageBacking { outputAdmission, std::move(image) };
        const uchar* const pixels = backing->image.constBits();
        const QColorSpace colorSpace = backing->image.colorSpace();
        const QList<QRgb> colorTable = backing->image.colorTable();
        const qreal devicePixelRatio = backing->image.devicePixelRatio();
        const qint64 dotsPerMeterX = backing->image.dotsPerMeterX();
        const qint64 dotsPerMeterY = backing->image.dotsPerMeterY();
        const QPoint offset = backing->image.offset();
        // The wrapper is the sole mutable owner of its QImage header. The backing image remains
        // immutable and keeps both the physical pixels and their admission alive until the last
        // wrapper alias retires.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        QImage admitted(const_cast<uchar*>(pixels), backing->image.width(), backing->image.height(),
            backing->image.bytesPerLine(), backing->image.format(), deleteAdmittedImageBacking,
            backing);
        if (admitted.isNull()) {
            delete backing;
            return {};
        }

        admitted.setColorSpace(colorSpace);
        if (!colorTable.isEmpty()) {
            admitted.setColorTable(colorTable);
        }
        admitted.setDevicePixelRatio(devicePixelRatio);
        admitted.setDotsPerMeterX(dotsPerMeterX);
        admitted.setDotsPerMeterY(dotsPerMeterY);
        admitted.setOffset(offset);
        Q_ASSERT(admitted.constBits() == pixels);
        return admitted;
    }

    bool imagesShareExactBacking(const QImage& left, const QImage& right)
    {
        return !left.isNull() && !right.isNull() && left.constBits() == right.constBits()
            && left.size() == right.size() && left.bytesPerLine() == right.bytesPerLine()
            && left.format() == right.format() && left.cacheKey() == right.cacheKey();
    }

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

    bool reuseKeysDescribeSameBacking(
        const DisplayImageReuseKey& left, const DisplayImageReuseKey& right)
    {
        return left.locationIdentity == right.locationIdentity
            && left.sourceIdentity == right.sourceIdentity
            && left.sourceRevision == right.sourceRevision
            && left.rasterIdentity.kind == right.rasterIdentity.kind
            && left.rasterIdentity.authoredFrame == right.rasterIdentity.authoredFrame
            && left.imageReaderTransformations == right.imageReaderTransformations
            && left.originalSize == right.originalSize && left.rasterSize == right.rasterSize
            && left.previewOrigin == right.previewOrigin;
    }

}

class DisplayImageOutputAdmission::Private
{
public:
    Private(std::shared_ptr<DisplayImageOutputLedger> ledger, qsizetype byteCost)
        : ledger(std::move(ledger))
        , byteCost(byteCost)
    {
        this->ledger->byteCost.fetch_add(byteCost);
    }

    ~Private()
    {
        if (ledger != nullptr) {
            ledger->byteCost.fetch_sub(byteCost.load());
        }
    }
    Q_DISABLE_COPY_MOVE(Private)

    std::shared_ptr<DisplayImageOutputLedger> ledger;
    std::atomic<qsizetype> byteCost = 0;
};

DisplayImageOutputAdmission::DisplayImageOutputAdmission(std::unique_ptr<Private> data)
    : d(std::move(data))
{
}

DisplayImageOutputAdmission::~DisplayImageOutputAdmission() = default;

qsizetype DisplayImageOutputAdmission::byteCost() const
{
    return d == nullptr ? 0 : d->byteCost.load();
}

bool DisplayImageOutputAdmission::retainOnly(qsizetype retainedByteCost)
{
    if (d == nullptr || retainedByteCost < 0) {
        return false;
    }

    qsizetype currentByteCost = d->byteCost.load();
    while (retainedByteCost < currentByteCost) {
        if (d->byteCost.compare_exchange_weak(currentByteCost, retainedByteCost)) {
            d->ledger->byteCost.fetch_sub(currentByteCost - retainedByteCost);
            return true;
        }
    }
    return retainedByteCost == currentByteCost;
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
        std::shared_ptr<DisplayImageOutputAdmission> outputAdmission;
        QImage image;
        QSize originalSize;
        QSize rasterSize;
        DisplayImageQuality quality = DisplayImageQuality::Exact;
        DisplayImageRetentionPriority priority = DisplayImageRetentionPriority::Nearby;
        qsizetype byteCost = 0;
        quint64 lastUse = 0;
        int frameLeaseCount = 0;
        DisplayImageReuseKey reuseKey;
        std::vector<DisplayImageReuseKey> reuseAliases;
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
    std::shared_ptr<DisplayImageOutputLedger> outputLedger
        = std::make_shared<DisplayImageOutputLedger>();
    OutputPressureReclaimer outputPressureReclaimer;
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

    QString insertNewEntry(DisplayImageEntry entry, qsizetype entryByteCost,
        DisplayImageReuseKey reuseKey, std::shared_ptr<DisplayImageOutputAdmission> outputAdmission)
    {
        Q_ASSERT(outputAdmission != nullptr);
        const QString id = nextEntryId();
        const QSize originalSize = normalizedOriginalSize(entry);
        const QSize rasterSize = normalizedRasterSize(entry);
        QImage admittedImage
            = imageRetainingOutputAdmission(std::move(entry.image), outputAdmission);
        if (admittedImage.isNull()) {
            return {};
        }

        images.push_back(Entry {
            id,
            std::move(outputAdmission),
            std::move(admittedImage),
            originalSize,
            rasterSize,
            entry.quality,
            entry.priority,
            entryByteCost,
            ++useClock,
            0,
            std::move(reuseKey),
            {},
            std::nullopt,
        });
        auto inserted = std::prev(images.end());
        indexEntry(inserted);
        trimToBudget();
        return entriesById.contains(id) ? id : QString();
    }

    void removeEntry(EntryIterator entry)
    {
        removeEvictionIndex(*entry);
        entriesById.erase(entry->id);
        entriesByReuseKey.erase(entry->reuseKey);
        for (const DisplayImageReuseKey& reuseAlias : entry->reuseAliases) {
            entriesByReuseKey.erase(reuseAlias);
        }
        images.erase(entry);
    }

    qsizetype aggregateByteCost() const { return outputLedger->byteCost.load(); }

    DisplayImageOutputReservationDiagnostics reservationDiagnostics() const
    {
        DisplayImageOutputReservationDiagnostics diagnostics;
        diagnostics.chargedBytes = aggregateByteCost();
        qsizetype residentAdmissionBytes = 0;
        for (const Entry& entry : images) {
            ++diagnostics.residentEntries;
            diagnostics.residentBytes
                = saturatedQtByteSum(diagnostics.residentBytes, entry.byteCost);
            const qsizetype admissionBytes
                = entry.outputAdmission == nullptr ? 0 : entry.outputAdmission->byteCost();
            residentAdmissionBytes = saturatedQtByteSum(residentAdmissionBytes, admissionBytes);
            const bool evictable = entry.frameLeaseCount == 0 && entry.outputAdmission != nullptr
                && entry.outputAdmission.use_count() == 2 && entry.image.isDetached();
            if (evictable) {
                ++diagnostics.evictableEntries;
                diagnostics.evictableBytes
                    = saturatedQtByteSum(diagnostics.evictableBytes, admissionBytes);
            } else {
                ++diagnostics.nonEvictableEntries;
                diagnostics.nonEvictableBytes
                    = saturatedQtByteSum(diagnostics.nonEvictableBytes, admissionBytes);
            }
            if (entry.frameLeaseCount > 0) {
                ++diagnostics.leasedEntries;
                diagnostics.leasedBytes
                    = saturatedQtByteSum(diagnostics.leasedBytes, admissionBytes);
            } else if (!entry.image.isDetached()) {
                ++diagnostics.externalAliasUnleasedEntries;
                diagnostics.externalAliasUnleasedBytes
                    = saturatedQtByteSum(diagnostics.externalAliasUnleasedBytes, admissionBytes);
            }
        }
        diagnostics.outstandingAdmissionBytes = diagnostics.chargedBytes > residentAdmissionBytes
            ? diagnostics.chargedBytes - residentAdmissionBytes
            : 0;
        return diagnostics;
    }

    void trimToBudget(qsizetype additionalByteCost = 0)
    {
        while (saturatedQtByteSum(aggregateByteCost(), additionalByteCost) > byteBudget) {
            const auto evictable = std::ranges::find_if(evictionEntries, [](const auto& indexed) {
                const Entry& entry = *indexed.second;
                return entry.outputAdmission != nullptr && entry.outputAdmission.use_count() == 2
                    && entry.image.isDetached();
            });
            if (evictable == evictionEntries.end()) {
                return;
            }
            removeEntry(evictable->second);
        }
    }
};

DisplayImageStore::DisplayImageStore(qsizetype byteBudget)
    : d(std::make_unique<Private>())
{
    d->byteBudget = std::max<qsizetype>(0, byteBudget);
}

DisplayImageStore::~DisplayImageStore() = default;

QString DisplayImageStore::acquireReusable(DisplayImageEntry entry, DisplayImageReuseKey reuseKey,
    std::shared_ptr<DisplayImageOutputAdmission> outputAdmission)
{
    const auto retireUnstoredOutput = [&]() {
        entry.image = {};
        outputAdmission.reset();
    };
    if (entry.image.isNull() || reuseKey.locationIdentity.isEmpty()
        || reuseKey.sourceIdentity.isEmpty() || !reuseKey.sourceRevision.isValid()
        || !reuseKey.rasterIdentity.isValid()) {
        retireUnstoredOutput();
        return {};
    }

    const qsizetype byteCost = imageByteCost(entry.image);
    QMutexLocker locker(&d->mutex);
    if (outputAdmission != nullptr) {
        if (outputAdmission->d == nullptr
            || outputAdmission->d->ledger.get() != d->outputLedger.get() || byteCost <= 0
            || byteCost > outputAdmission->d->byteCost.load()) {
            retireUnstoredOutput();
            return {};
        }
    }
    auto reusable = d->findReusableEntry(reuseKey);
    if (reusable != d->images.end()) {
        reusable->priority = entry.priority;
        d->touchEntry(reusable);
        d->trimToBudget();
        const QString id = reusable->id;
        retireUnstoredOutput();
        return id;
    }

    if (outputAdmission != nullptr) {
        auto transferred
            = std::ranges::find_if(d->images, [&outputAdmission](const Private::Entry& stored) {
                  return stored.outputAdmission == outputAdmission;
              });
        if (transferred != d->images.end()) {
            if (!imagesShareExactBacking(transferred->image, entry.image)
                || !reuseKeysDescribeSameBacking(transferred->reuseKey, reuseKey)) {
                retireUnstoredOutput();
                return {};
            }
            transferred->reuseAliases.push_back(reuseKey);
            d->entriesByReuseKey.emplace(std::move(reuseKey), transferred);
            transferred->priority = entry.priority;
            d->touchEntry(transferred);
            d->trimToBudget();
            const QString id = transferred->id;
            retireUnstoredOutput();
            return id;
        }
        const bool retained = outputAdmission->retainOnly(byteCost);
        Q_ASSERT(retained);
    }

    if (byteCost <= 0 || byteCost > d->byteBudget) {
        retireUnstoredOutput();
        return {};
    }

    if (outputAdmission == nullptr) {
        auto data
            = std::make_unique<DisplayImageOutputAdmission::Private>(d->outputLedger, byteCost);
        outputAdmission = std::shared_ptr<DisplayImageOutputAdmission>(
            new DisplayImageOutputAdmission(std::move(data)));
    }

    return d->insertNewEntry(
        std::move(entry), byteCost, std::move(reuseKey), std::move(outputAdmission));
}

std::shared_ptr<DisplayImageOutputAdmission> DisplayImageStore::reserveOutput(
    qsizetype byteCost, DisplayImageOutputReservationOrigin origin)
{
    if (byteCost <= 0) {
        return {};
    }

    QMutexLocker locker(&d->mutex);
    if (byteCost > d->byteBudget) {
        const qsizetype budgetBytes = d->byteBudget;
        const DisplayImageOutputReservationDiagnostics diagnostics = d->reservationDiagnostics();
        locker.unlock();
        logOutputReservationFailure(origin,
            DisplayImageOutputReservationFailureReason::SingleOutputLimit, byteCost, budgetBytes,
            diagnostics);
        return {};
    }
    d->trimToBudget(byteCost);
    if (saturatedQtByteSum(d->aggregateByteCost(), byteCost) > d->byteBudget) {
        const OutputPressureReclaimer pressureReclaimer = d->outputPressureReclaimer;
        if (pressureReclaimer) {
            locker.unlock();
            pressureReclaimer();
            locker.relock();
            d->trimToBudget(byteCost);
        }
    }
    if (saturatedQtByteSum(d->aggregateByteCost(), byteCost) > d->byteBudget) {
        const qsizetype budgetBytes = d->byteBudget;
        const DisplayImageOutputReservationDiagnostics diagnostics = d->reservationDiagnostics();
        locker.unlock();
        logOutputReservationFailure(origin,
            DisplayImageOutputReservationFailureReason::AggregateLimit, byteCost, budgetBytes,
            diagnostics);
        return {};
    }

    auto data = std::make_unique<DisplayImageOutputAdmission::Private>(d->outputLedger, byteCost);
    return std::shared_ptr<DisplayImageOutputAdmission>(
        new DisplayImageOutputAdmission(std::move(data)));
}

qsizetype DisplayImageStore::availableOutputBytesForRequest(qsizetype requestedByteCost,
    qsizetype minimumRequiredByteCost, DisplayImageOutputReservationOrigin origin)
{
    QMutexLocker locker(&d->mutex);
    const qsizetype reclaimTarget = std::clamp<qsizetype>(requestedByteCost, 0, d->byteBudget);
    d->trimToBudget(reclaimTarget);
    if (saturatedQtByteSum(d->aggregateByteCost(), reclaimTarget) > d->byteBudget) {
        const OutputPressureReclaimer pressureReclaimer = d->outputPressureReclaimer;
        if (pressureReclaimer) {
            locker.unlock();
            pressureReclaimer();
            locker.relock();
            d->trimToBudget(reclaimTarget);
        }
    }
    const qsizetype aggregateByteCost = std::min(d->aggregateByteCost(), d->byteBudget);
    const qsizetype availableBytes = d->byteBudget - aggregateByteCost;
    if (minimumRequiredByteCost > availableBytes) {
        const qsizetype budgetBytes = d->byteBudget;
        const DisplayImageOutputReservationDiagnostics diagnostics = d->reservationDiagnostics();
        const DisplayImageOutputReservationFailureReason reason
            = minimumRequiredByteCost > budgetBytes
            ? DisplayImageOutputReservationFailureReason::SingleOutputLimit
            : DisplayImageOutputReservationFailureReason::AggregateLimit;
        locker.unlock();
        logOutputReservationFailure(
            origin, reason, minimumRequiredByteCost, budgetBytes, diagnostics);
    }
    return availableBytes;
}

void DisplayImageStore::setOutputPressureReclaimer(OutputPressureReclaimer reclaimer)
{
    QMutexLocker locker(&d->mutex);
    d->outputPressureReclaimer = std::move(reclaimer);
}

std::shared_ptr<DisplayImageOutputAdmission> DisplayImageStore::outputAdmissionForImage(
    const QImage& image) const
{
    if (image.isNull()) {
        return {};
    }

    QMutexLocker locker(&d->mutex);
    const auto matching = std::ranges::find_if(d->images, [&image](const Private::Entry& entry) {
        return imagesShareExactBacking(entry.image, image);
    });
    return matching == d->images.end() ? nullptr : matching->outputAdmission;
}

qsizetype DisplayImageStore::availableOutputBytes() const
{
    QMutexLocker locker(&d->mutex);
    const qsizetype aggregateByteCost = std::min(d->aggregateByteCost(), d->byteBudget);
    return d->byteBudget - aggregateByteCost;
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
    return d->aggregateByteCost();
}

qsizetype DisplayImageStore::size() const
{
    QMutexLocker locker(&d->mutex);
    return static_cast<qsizetype>(d->images.size());
}

}
