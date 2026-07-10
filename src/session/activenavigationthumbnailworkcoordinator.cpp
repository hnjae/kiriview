// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "activenavigationthumbnailworkcoordinator.h"

#include "session/thumbnaillogging.h"

#include <QDebug>
#include <QFile>
#include <algorithm>
#include <array>
#include <utility>

namespace {
using Bucket = kiriview::ActiveNavigationThumbnailDemandBucket;

bool thumbnailSourceKeyHasSourceKind(const kiriview::ThumbnailSourceKey& sourceKey,
    kiriview::ActiveNavigationThumbnailSourceKind sourceKind)
{
    return sourceKey.sourceKind
        == kiriview::activeNavigationThumbnailSourceKindIdentity(sourceKind);
}

constexpr std::array<Bucket, 4> backgroundFillBuckets()
{
    return { Bucket::Normal, Bucket::Large, Bucket::XLarge, Bucket::XXLarge };
}

kiriview::ThumbnailSourceKind thumbnailSourceKind(const QString& sourceKind)
{
    if (sourceKind
        == kiriview::activeNavigationThumbnailSourceKindIdentity(
            kiriview::ActiveNavigationThumbnailSourceKind::DirectImage)) {
        return kiriview::ThumbnailSourceKind::DirectImage;
    }
    if (sourceKind
        == kiriview::activeNavigationThumbnailSourceKindIdentity(
            kiriview::ActiveNavigationThumbnailSourceKind::DirectVideo)) {
        return kiriview::ThumbnailSourceKind::DirectVideo;
    }
    if (sourceKind
        == kiriview::activeNavigationThumbnailSourceKindIdentity(
            kiriview::ActiveNavigationThumbnailSourceKind::ImageDocumentPageImage)) {
        return kiriview::ThumbnailSourceKind::ImageDocumentPageImage;
    }
    if (sourceKind
        == kiriview::activeNavigationThumbnailSourceKindIdentity(
            kiriview::ActiveNavigationThumbnailSourceKind::ImageDocumentPageVideo)) {
        return kiriview::ThumbnailSourceKind::ImageDocumentPageVideo;
    }

    return kiriview::ThumbnailSourceKind::DirectImage;
}

QString fallbackThumbnailFailureError(kiriview::ActiveNavigationThumbnailFailureKind failureKind)
{
    switch (failureKind) {
    case kiriview::ActiveNavigationThumbnailFailureKind::CacheLookupInvalid:
        return QStringLiteral("Thumbnail cache lookup returned an invalid cache entry.");
    case kiriview::ActiveNavigationThumbnailFailureKind::CacheLookupFailed:
        return QStringLiteral("Thumbnail cache lookup failed.");
    case kiriview::ActiveNavigationThumbnailFailureKind::GenerationFailed:
        return QStringLiteral("Thumbnail generation failed.");
    case kiriview::ActiveNavigationThumbnailFailureKind::ImageStoreInsertFailed:
        return QStringLiteral("Thumbnail image store insertion failed.");
    case kiriview::ActiveNavigationThumbnailFailureKind::GenerationProviderUnavailable:
        return QStringLiteral("Thumbnail generation provider is unavailable.");
    }

    return QStringLiteral("Thumbnail work failed.");
}
}

namespace kiriview {
ThumbnailSourceAdapter defaultThumbnailSourceAdapter()
{
    return [](ThumbnailSourceAdapterRequest request) {
        const bool supportedDirectSource = thumbnailSourceKeyHasSourceKind(request.sourceKey,
                                               ActiveNavigationThumbnailSourceKind::DirectImage)
            || thumbnailSourceKeyHasSourceKind(
                request.sourceKey, ActiveNavigationThumbnailSourceKind::DirectVideo);
        if (!supportedDirectSource || !request.sourceKey.url.isLocalFile()) {
            return ThumbnailSourceAdapterPlan {};
        }

        return ThumbnailSourceAdapterPlan {
            ThumbnailSourceAdapterPlanKind::CacheableLocalFile,
            QFile::encodeName(request.sourceKey.url.toLocalFile()),
            ThumbnailOriginalIdentity::fromLocalPathBytes(
                QFile::encodeName(request.sourceKey.url.toLocalFile())),
            {},
        };
    };
}

ActiveNavigationThumbnailWorkCoordinator::ActiveNavigationThumbnailWorkCoordinator(QObject* owner,
    ActiveNavigationThumbnailRowPort& rowPort, ThumbnailCacheLookupProvider lookupProvider,
    ThumbnailGenerationProvider generationProvider, ThumbnailSourceAdapter sourceAdapter)
    : m_owner(owner)
    , m_rowPort(rowPort)
    , m_lookupProvider(std::move(lookupProvider))
    , m_generationProvider(std::move(generationProvider))
    , m_sourceAdapter(std::move(sourceAdapter))
{
}

ActiveNavigationThumbnailWorkCoordinator::~ActiveNavigationThumbnailWorkCoordinator()
{
    invalidateRows();
}

void ActiveNavigationThumbnailWorkCoordinator::resetRows(
    std::size_t rowCount, quint64 navigationGeneration)
{
    invalidateRows();
    m_rows.resize(rowCount);
    m_navigationGeneration = navigationGeneration;
}

void ActiveNavigationThumbnailWorkCoordinator::invalidateRows()
{
    cancelAllActiveJobs();
    m_demandTracker.reset();
    m_backgroundArmed = false;
    m_demandWindowOpen = false;
    m_demandWindowGeneration = 0;
    m_demandWindowEpoch = 0;
    m_demandWindowRows.clear();
    m_previousDemandWindowRows.clear();
    m_activeBackgroundRowIndex.reset();
    m_rows.clear();
    m_navigationGeneration = 0;
}

bool ActiveNavigationThumbnailWorkCoordinator::beginDemandWindow(quint64 navigationGeneration)
{
    if (navigationGeneration == 0 || navigationGeneration != m_navigationGeneration) {
        return false;
    }

    m_demandWindowOpen = true;
    m_demandWindowGeneration = navigationGeneration;
    ++m_demandWindowEpoch;
    m_previousDemandWindowRows = std::move(m_demandWindowRows);
    m_demandWindowRows.clear();
    m_demandTracker.reset();
    cancelActiveBackgroundJob();
    return true;
}

void ActiveNavigationThumbnailWorkCoordinator::finishDemandWindow(quint64 navigationGeneration)
{
    if (!m_demandWindowOpen || navigationGeneration != m_demandWindowGeneration) {
        return;
    }

    expireDemandOutsideCurrentWindow();
    m_demandWindowOpen = false;
    m_demandWindowGeneration = 0;
    maybeScheduleBackgroundWork();
}

bool ActiveNavigationThumbnailWorkCoordinator::reportDemand(int number, const QUrl& url,
    ActiveNavigationThumbnailDemandBucket bucket, ActiveNavigationThumbnailDemandPriority priority,
    quint64 navigationGeneration)
{
    qCDebug(kiriviewThumbnailLog) << "Thumbnail demand" << number << url << "bucket"
                                  << static_cast<int>(bucket) << "priority"
                                  << static_cast<int>(priority) << "generation"
                                  << navigationGeneration;
    if (bucket == ActiveNavigationThumbnailDemandBucket::None || navigationGeneration == 0
        || navigationGeneration != m_navigationGeneration) {
        qCDebug(kiriviewThumbnailLog)
            << "Rejecting thumbnail demand for stale or empty generation" << navigationGeneration
            << "current" << m_navigationGeneration;
        return false;
    }

    const std::optional<std::size_t> rowIndex
        = m_rowPort.rowIndexForIdentity(number, url, navigationGeneration);
    if (!rowIndex.has_value() || *rowIndex >= m_rows.size()) {
        qCDebug(kiriviewThumbnailLog)
            << "Rejecting thumbnail demand for unknown row" << number << url;
        return false;
    }

    if (!m_demandTracker.record(ActiveNavigationThumbnailDemand {
            number,
            url,
            bucket,
            priority,
            navigationGeneration,
        })) {
        return false;
    }

    WorkState& state = m_rows.at(*rowIndex);
    markDemandWindowRow(*rowIndex, state);
    const ThumbnailSourceKey sourceKey = m_rowPort.sourceKeyAt(*rowIndex);
    const ThumbnailSourceAdapterPlan sourcePlan = sourcePlanForDemand(sourceKey, bucket, priority);
    const AcceptedDemand demand {
        sourceKey,
        bucket,
        priority,
        sourcePlan,
    };
    if (state.acceptedDemand.has_value() && sameAcceptedDemand(*state.acceptedDemand, demand)) {
        return false;
    }

    m_backgroundArmed = true;
    cancelActiveBackgroundJob();

    if (state.acceptedDemand.has_value()
        && sameFreshThumbnailSourceKey(state.acceptedDemand->sourceKey, demand.sourceKey)
        && state.acceptedDemand->bucket == demand.bucket
        && state.acceptedDemand->priority != demand.priority) {
        state.acceptedDemand = demand;
        m_rowPort.updateRetentionPriority(sourceKey, imageRetentionPriority(demand.priority));
        if (state.activeJob.has_value()) {
            state.activeJob->demand = demand;
        }
        if (!m_demandWindowOpen) {
            maybeScheduleBackgroundWork();
        }
        return true;
    }

    cancelActiveJob(*rowIndex, state);
    state.acceptedDemand = demand;
    if (supportsGeneratedThumbnail(sourcePlan)) {
        m_rowPort.applyPending(sourceKey);
        state.activeJob = ActiveJobSlot {
            m_nextJobId++,
            ThumbnailWorkKind::Foreground,
            demand,
            {},
        };
        if (usesCacheLookup(sourcePlan)) {
            startLookupJob(state, demand, ThumbnailWorkKind::Foreground);
        } else {
            startGenerationJob(state, demand, ThumbnailWorkKind::Foreground);
        }
    } else {
        m_rowPort.applyUnsupported(sourceKey);
        state.activeJob.reset();
        qCDebug(kiriviewThumbnailLog) << "Thumbnail demand unsupported" << number << url;
    }
    if (!m_demandWindowOpen) {
        maybeScheduleBackgroundWork();
    }
    return true;
}

bool ActiveNavigationThumbnailWorkCoordinator::acceptCompletion(
    const ActiveNavigationThumbnailCompletion& completion)
{
    const std::optional<std::size_t> rowIndex
        = m_rowPort.rowIndexForSourceKey(completion.sourceKey);
    if (!rowIndex.has_value() || *rowIndex >= m_rows.size()) {
        return false;
    }

    WorkState& state = m_rows.at(*rowIndex);
    if (!state.acceptedDemand.has_value()
        || !sameFreshThumbnailSourceKey(state.acceptedDemand->sourceKey, completion.sourceKey)
        || state.acceptedDemand->bucket != completion.bucket) {
        return false;
    }

    if (state.activeJob.has_value() && state.activeJob->kind == ThumbnailWorkKind::Background) {
        m_activeBackgroundRowIndex.reset();
    }
    state.activeJob.reset();
    return true;
}

const std::vector<ActiveNavigationThumbnailFailureDiagnostic>&
ActiveNavigationThumbnailWorkCoordinator::failureDiagnostics() const
{
    return m_failureDiagnostics;
}

qsizetype ActiveNavigationThumbnailWorkCoordinator::activeJobCount() const
{
    qsizetype count = 0;
    for (const WorkState& state : m_rows) {
        if (state.activeJob.has_value()) {
            ++count;
        }
    }
    return count;
}

qsizetype ActiveNavigationThumbnailWorkCoordinator::canceledJobCount() const
{
    return static_cast<qsizetype>(m_canceledJobIds.size());
}

bool ActiveNavigationThumbnailWorkCoordinator::sameFreshThumbnailSourceKey(
    const ThumbnailSourceKey& left, const ThumbnailSourceKey& right)
{
    return sameThumbnailSourceKey(left, right)
        && left.navigationGeneration == right.navigationGeneration;
}

bool ActiveNavigationThumbnailWorkCoordinator::sameSourceAdapterPlan(
    const ThumbnailSourceAdapterPlan& left, const ThumbnailSourceAdapterPlan& right)
{
    return left.kind == right.kind && left.localPathBytes == right.localPathBytes
        && left.originalIdentity.mode == right.originalIdentity.mode
        && left.originalIdentity.localPathBytes == right.originalIdentity.localPathBytes
        && left.originalIdentity.uri == right.originalIdentity.uri
        && left.originalIdentity.mtimeSeconds == right.originalIdentity.mtimeSeconds
        && left.originalIdentity.originalByteSize == right.originalIdentity.originalByteSize
        && left.originalIdentity.mimeType == right.originalIdentity.mimeType
        && sameOpenedCollectionScopeLocation(
            left.openedCollectionScope, right.openedCollectionScope);
}

bool ActiveNavigationThumbnailWorkCoordinator::sameAcceptedDemand(
    const AcceptedDemand& left, const AcceptedDemand& right)
{
    return sameFreshThumbnailSourceKey(left.sourceKey, right.sourceKey)
        && left.bucket == right.bucket && left.priority == right.priority
        && sameSourceAdapterPlan(left.sourcePlan, right.sourcePlan);
}

bool ActiveNavigationThumbnailWorkCoordinator::supportsGeneratedThumbnail(
    const ThumbnailSourceAdapterPlan& plan)
{
    return plan.kind == ThumbnailSourceAdapterPlanKind::InMemoryOnly
        || (plan.kind == ThumbnailSourceAdapterPlanKind::CacheableLocalFile
            && !plan.localPathBytes.isEmpty())
        || (plan.kind == ThumbnailSourceAdapterPlanKind::CacheableOpenedCollectionEntry
            && !plan.openedCollectionScope.isEmpty());
}

bool ActiveNavigationThumbnailWorkCoordinator::usesCacheLookup(
    const ThumbnailSourceAdapterPlan& plan)
{
    return plan.kind == ThumbnailSourceAdapterPlanKind::CacheableLocalFile
        && !plan.localPathBytes.isEmpty();
}

bool ActiveNavigationThumbnailWorkCoordinator::enablesCacheInstall(
    const ThumbnailSourceAdapterPlan& plan)
{
    return (plan.kind == ThumbnailSourceAdapterPlanKind::CacheableLocalFile
               && !plan.localPathBytes.isEmpty())
        || (plan.kind == ThumbnailSourceAdapterPlanKind::CacheableOpenedCollectionEntry
            && !plan.openedCollectionScope.isEmpty());
}

ThumbnailImageRetentionPriority ActiveNavigationThumbnailWorkCoordinator::imageRetentionPriority(
    ActiveNavigationThumbnailDemandPriority priority)
{
    switch (priority) {
    case ActiveNavigationThumbnailDemandPriority::Visible:
        return ThumbnailImageRetentionPriority::Visible;
    case ActiveNavigationThumbnailDemandPriority::Nearby:
        return ThumbnailImageRetentionPriority::Nearby;
    }

    return ThumbnailImageRetentionPriority::Nearby;
}

ThumbnailImageRetentionPriority ActiveNavigationThumbnailWorkCoordinator::imageRetentionPriority(
    ThumbnailWorkKind kind, ActiveNavigationThumbnailDemandPriority priority)
{
    if (kind == ThumbnailWorkKind::Background) {
        return ThumbnailImageRetentionPriority::Background;
    }

    return imageRetentionPriority(priority);
}

void ActiveNavigationThumbnailWorkCoordinator::markDemandWindowRow(
    std::size_t row, WorkState& state)
{
    if (m_demandWindowEpoch == 0) {
        ++m_demandWindowEpoch;
    }

    if (state.demandWindowEpoch == m_demandWindowEpoch) {
        return;
    }

    state.demandWindowEpoch = m_demandWindowEpoch;
    m_demandWindowRows.push_back(row);
}

void ActiveNavigationThumbnailWorkCoordinator::expireDemandOutsideCurrentWindow()
{
    for (std::size_t rowIndex : m_previousDemandWindowRows) {
        if (rowIndex >= m_rows.size()) {
            continue;
        }

        WorkState& state = m_rows.at(rowIndex);
        if (state.demandWindowEpoch == m_demandWindowEpoch) {
            continue;
        }

        if (state.activeJob.has_value()) {
            cancelActiveJob(rowIndex, state);
        }
        state.acceptedDemand.reset();
        m_rowPort.updateRetentionPriority(
            m_rowPort.sourceKeyAt(rowIndex), ThumbnailImageRetentionPriority::Background);
    }

    m_previousDemandWindowRows.clear();
}

void ActiveNavigationThumbnailWorkCoordinator::cancelActiveJob(std::size_t row, WorkState& state)
{
    if (!state.activeJob.has_value()) {
        return;
    }

    qCDebug(kiriviewThumbnailLog) << "Canceling thumbnail job" << state.activeJob->id << "kind"
                                  << static_cast<int>(state.activeJob->kind) << "number"
                                  << m_rowPort.sourceKeyAt(row).rowNumber << "bucket"
                                  << static_cast<int>(state.activeJob->demand.bucket);
    m_canceledJobIds.push_back(state.activeJob->id);
    if (state.activeJob->kind == ThumbnailWorkKind::Background) {
        m_activeBackgroundRowIndex.reset();
    }
    state.activeJob->job.cancel();
    state.activeJob.reset();
}

void ActiveNavigationThumbnailWorkCoordinator::cancelActiveBackgroundJob()
{
    if (!m_activeBackgroundRowIndex.has_value() || *m_activeBackgroundRowIndex >= m_rows.size()) {
        m_activeBackgroundRowIndex.reset();
        return;
    }

    WorkState& state = m_rows.at(*m_activeBackgroundRowIndex);
    if (state.activeJob.has_value() && state.activeJob->kind == ThumbnailWorkKind::Background) {
        cancelActiveJob(*m_activeBackgroundRowIndex, state);
        return;
    }

    m_activeBackgroundRowIndex.reset();
}

void ActiveNavigationThumbnailWorkCoordinator::cancelAllActiveJobs()
{
    for (std::size_t row = 0; row < m_rows.size(); ++row) {
        cancelActiveJob(row, m_rows.at(row));
    }
}

bool ActiveNavigationThumbnailWorkCoordinator::hasActiveForegroundJob() const
{
    return std::any_of(m_rows.cbegin(), m_rows.cend(), [](const WorkState& state) {
        return state.activeJob.has_value()
            && state.activeJob->kind == ThumbnailWorkKind::Foreground;
    });
}

void ActiveNavigationThumbnailWorkCoordinator::startLookupJob(
    WorkState& state, const AcceptedDemand& demand, ThumbnailWorkKind kind)
{
    if (state.activeJob == std::nullopt || !m_lookupProvider) {
        return;
    }

    ThumbnailCacheLookupRequest request;
    request.localPathBytes = demand.sourcePlan.localPathBytes;
    request.originalIdentity = demand.sourcePlan.originalIdentity;
    request.requestedBucket = demand.bucket;

    const quint64 jobId = state.activeJob->id;
    qCDebug(kiriviewThumbnailLog) << "Starting thumbnail lookup job" << jobId << "kind"
                                  << static_cast<int>(kind) << "number"
                                  << demand.sourceKey.rowNumber << "bucket"
                                  << static_cast<int>(demand.bucket);
    ImageIoJob job = m_lookupProvider(m_owner, std::move(request),
        [this, jobId, sourceKey = demand.sourceKey, bucket = demand.bucket, kind](
            ThumbnailCacheLookupResult result) mutable {
            finishLookup(jobId, sourceKey, bucket, kind, std::move(result));
        });
    const std::optional<std::size_t> rowIndex = m_rowPort.rowIndexForSourceKey(demand.sourceKey);
    if (!rowIndex.has_value() || *rowIndex >= m_rows.size()
        || !activeJobMatches(m_rows.at(*rowIndex), jobId, demand, kind)) {
        job.cancel();
        return;
    }
    m_rows.at(*rowIndex).activeJob->job = std::move(job);
}

void ActiveNavigationThumbnailWorkCoordinator::startGenerationJob(
    WorkState& state, const AcceptedDemand& demand, ThumbnailWorkKind kind)
{
    if (state.activeJob == std::nullopt || !m_generationProvider) {
        const quint64 jobId = state.activeJob.has_value() ? state.activeJob->id : 0;
        recordFailureDiagnostic(jobId, demand.sourceKey, kind, demand.bucket,
            ActiveNavigationThumbnailFailureKind::GenerationProviderUnavailable, {});
        state.activeJob.reset();
        if (kind == ThumbnailWorkKind::Background) {
            m_activeBackgroundRowIndex.reset();
            markBackgroundBucketCompleted(state, demand.bucket);
            maybeScheduleBackgroundWork();
        } else {
            m_rowPort.applyFailed(demand.sourceKey);
        }
        return;
    }

    ThumbnailGenerationRequest request;
    request.localPathBytes = demand.sourcePlan.localPathBytes;
    request.originalIdentity = demand.sourcePlan.originalIdentity;
    request.openedCollectionScope = demand.sourcePlan.openedCollectionScope;
    request.sourceUrl = demand.sourceKey.url;
    request.sourceLabel = demand.sourceKey.label;
    request.sourceKind = thumbnailSourceKind(demand.sourceKey.sourceKind);
    request.requestedBucket = demand.bucket;
    request.cacheInstallEnabled = enablesCacheInstall(demand.sourcePlan);

    const quint64 jobId = state.activeJob->id;
    qCDebug(kiriviewThumbnailLog) << "Starting thumbnail generation job" << jobId << "kind"
                                  << static_cast<int>(kind) << "number"
                                  << demand.sourceKey.rowNumber << "bucket"
                                  << static_cast<int>(demand.bucket);
    ImageIoJob job = m_generationProvider(m_owner, std::move(request),
        [this, jobId, sourceKey = demand.sourceKey, bucket = demand.bucket, kind](
            ThumbnailGenerationResult result) mutable {
            finishGeneration(jobId, sourceKey, bucket, kind, std::move(result));
        });
    const std::optional<std::size_t> rowIndex = m_rowPort.rowIndexForSourceKey(demand.sourceKey);
    if (!rowIndex.has_value() || *rowIndex >= m_rows.size()
        || !activeJobMatches(m_rows.at(*rowIndex), jobId, demand, kind)) {
        job.cancel();
        return;
    }
    m_rows.at(*rowIndex).activeJob->job = std::move(job);
}

void ActiveNavigationThumbnailWorkCoordinator::recordFailureDiagnostic(quint64 jobId,
    const ThumbnailSourceKey& sourceKey, ThumbnailWorkKind workKind,
    ActiveNavigationThumbnailDemandBucket bucket, ActiveNavigationThumbnailFailureKind failureKind,
    const QString& errorString)
{
    const QString resolvedErrorString
        = errorString.isEmpty() ? fallbackThumbnailFailureError(failureKind) : errorString;
    m_failureDiagnostics.push_back(ActiveNavigationThumbnailFailureDiagnostic {
        jobId,
        sourceKey,
        workKind == ThumbnailWorkKind::Foreground ? ActiveNavigationThumbnailWorkKind::Foreground
                                                  : ActiveNavigationThumbnailWorkKind::Background,
        bucket,
        failureKind,
        resolvedErrorString,
    });
    qCDebug(kiriviewThumbnailLog) << "Thumbnail failure diagnostic" << jobId << "kind"
                                  << static_cast<int>(workKind) << "number" << sourceKey.rowNumber
                                  << "url" << sourceKey.url << "bucket" << static_cast<int>(bucket)
                                  << "failure" << static_cast<int>(failureKind) << "error"
                                  << resolvedErrorString;
}

bool ActiveNavigationThumbnailWorkCoordinator::activeJobMatches(const WorkState& state,
    quint64 jobId, const AcceptedDemand& demand, ThumbnailWorkKind kind) const
{
    return state.activeJob.has_value() && state.activeJob->id == jobId
        && state.activeJob->kind == kind && sameAcceptedDemand(state.activeJob->demand, demand);
}

bool ActiveNavigationThumbnailWorkCoordinator::backgroundBucketCompleted(
    const WorkState& state, ActiveNavigationThumbnailDemandBucket bucket) const
{
    if (state.acceptedDemand.has_value()
        && static_cast<int>(state.acceptedDemand->bucket) >= static_cast<int>(bucket)) {
        return true;
    }

    return std::find(state.completedBackgroundBuckets.cbegin(),
               state.completedBackgroundBuckets.cend(), bucket)
        != state.completedBackgroundBuckets.cend();
}

void ActiveNavigationThumbnailWorkCoordinator::markBackgroundBucketCompleted(
    WorkState& state, ActiveNavigationThumbnailDemandBucket bucket)
{
    if (std::find(state.completedBackgroundBuckets.cbegin(),
            state.completedBackgroundBuckets.cend(), bucket)
        == state.completedBackgroundBuckets.cend()) {
        state.completedBackgroundBuckets.push_back(bucket);
    }
}

void ActiveNavigationThumbnailWorkCoordinator::maybeScheduleBackgroundWork()
{
    if (!m_backgroundArmed || hasActiveForegroundJob()) {
        return;
    }

    if (m_activeBackgroundRowIndex.has_value()) {
        return;
    }

    for (ActiveNavigationThumbnailDemandBucket bucket : backgroundFillBuckets()) {
        for (std::size_t rowIndex : m_demandWindowRows) {
            if (rowIndex >= m_rows.size()) {
                continue;
            }

            WorkState& state = m_rows.at(rowIndex);
            if (state.demandWindowEpoch != m_demandWindowEpoch || state.activeJob.has_value()) {
                continue;
            }

            if (backgroundBucketCompleted(state, bucket)
                || (state.acceptedDemand.has_value()
                    && !supportsGeneratedThumbnail(state.acceptedDemand->sourcePlan))) {
                continue;
            }

            const ThumbnailSourceKey sourceKey = m_rowPort.sourceKeyAt(rowIndex);
            ThumbnailSourceAdapterPlan sourcePlan = sourcePlanForDemand(
                sourceKey, bucket, ActiveNavigationThumbnailDemandPriority::Nearby);
            if (!supportsGeneratedThumbnail(sourcePlan)) {
                continue;
            }

            qCDebug(kiriviewThumbnailLog)
                << "Scheduling background thumbnail fill" << sourceKey.rowNumber << sourceKey.url
                << "bucket" << static_cast<int>(bucket) << "generation" << m_navigationGeneration;
            startBackgroundWork(rowIndex, state, bucket, std::move(sourcePlan));
            return;
        }
    }
}

ThumbnailSourceAdapterPlan ActiveNavigationThumbnailWorkCoordinator::sourcePlanForDemand(
    const ThumbnailSourceKey& sourceKey, ActiveNavigationThumbnailDemandBucket bucket,
    ActiveNavigationThumbnailDemandPriority priority) const
{
    if (!m_sourceAdapter) {
        return {};
    }

    return m_sourceAdapter(ThumbnailSourceAdapterRequest {
        sourceKey,
        bucket,
        priority,
    });
}

void ActiveNavigationThumbnailWorkCoordinator::startBackgroundWork(std::size_t row,
    WorkState& state, ActiveNavigationThumbnailDemandBucket bucket,
    ThumbnailSourceAdapterPlan sourcePlan)
{
    const AcceptedDemand demand {
        m_rowPort.sourceKeyAt(row),
        bucket,
        ActiveNavigationThumbnailDemandPriority::Nearby,
        std::move(sourcePlan),
    };
    state.activeJob = ActiveJobSlot {
        m_nextJobId++,
        ThumbnailWorkKind::Background,
        demand,
        {},
    };
    m_activeBackgroundRowIndex = row;
    if (usesCacheLookup(demand.sourcePlan)) {
        startLookupJob(state, demand, ThumbnailWorkKind::Background);
    } else {
        startGenerationJob(state, demand, ThumbnailWorkKind::Background);
    }
}

void ActiveNavigationThumbnailWorkCoordinator::finishLookup(quint64 jobId,
    const ThumbnailSourceKey& sourceKey, ActiveNavigationThumbnailDemandBucket bucket,
    ThumbnailWorkKind workKind, ThumbnailCacheLookupResult lookupResult)
{
    const std::optional<std::size_t> rowIndex = m_rowPort.rowIndexForSourceKey(sourceKey);
    if (!rowIndex.has_value() || *rowIndex >= m_rows.size()) {
        qCDebug(kiriviewThumbnailLog) << "Rejecting stale thumbnail lookup for missing source"
                                      << sourceKey.rowNumber << sourceKey.url;
        return;
    }

    WorkState& state = m_rows.at(*rowIndex);
    if (!state.activeJob.has_value() || state.activeJob->id != jobId
        || !sameFreshThumbnailSourceKey(state.activeJob->demand.sourceKey, sourceKey)
        || state.activeJob->demand.bucket != bucket || state.activeJob->kind != workKind) {
        qCDebug(kiriviewThumbnailLog)
            << "Rejecting stale thumbnail lookup completion" << jobId << sourceKey.rowNumber
            << sourceKey.url << "bucket" << static_cast<int>(bucket);
        return;
    }

    const ThumbnailWorkKind kind = state.activeJob->kind;
    const AcceptedDemand demand = state.activeJob->demand;
    qCDebug(kiriviewThumbnailLog) << "Thumbnail lookup finished" << jobId << "kind"
                                  << static_cast<int>(kind) << "number" << sourceKey.rowNumber
                                  << "bucket" << static_cast<int>(bucket) << "status"
                                  << static_cast<int>(lookupResult.status);

    switch (lookupResult.status) {
    case ThumbnailCacheLookupStatus::Ready: {
        state.activeJob.reset();
        if (kind == ThumbnailWorkKind::Background) {
            m_activeBackgroundRowIndex.reset();
        }
        if (kind == ThumbnailWorkKind::Background && m_rowPort.hasUsableReadyImage(sourceKey)) {
            markBackgroundBucketCompleted(state, bucket);
            maybeScheduleBackgroundWork();
            break;
        }

        if (!m_rowPort.installReadyImage(sourceKey, lookupResult.image,
                imageRetentionPriority(kind, demand.priority),
                kind == ThumbnailWorkKind::Background)) {
            recordFailureDiagnostic(jobId, sourceKey, kind, bucket,
                ActiveNavigationThumbnailFailureKind::ImageStoreInsertFailed, {});
            if (kind == ThumbnailWorkKind::Background) {
                markBackgroundBucketCompleted(state, bucket);
                maybeScheduleBackgroundWork();
            } else if (!m_rowPort.hasUsableReadyImage(sourceKey)) {
                markBackgroundBucketCompleted(state, bucket);
                m_rowPort.applyFailed(sourceKey);
            } else {
                markBackgroundBucketCompleted(state, bucket);
            }
            break;
        }
        qCDebug(kiriviewThumbnailLog)
            << "Installed thumbnail lookup result in image store, kind" << static_cast<int>(kind);
        markBackgroundBucketCompleted(state, bucket);
        break;
    }
    case ThumbnailCacheLookupStatus::Missing:
        if (kind == ThumbnailWorkKind::Foreground) {
            m_rowPort.applyPending(sourceKey);
        }
        state.activeJob = ActiveJobSlot {
            m_nextJobId++,
            kind,
            demand,
            {},
        };
        startGenerationJob(state, demand, kind);
        break;
    case ThumbnailCacheLookupStatus::Invalid:
    case ThumbnailCacheLookupStatus::Failed:
        recordFailureDiagnostic(jobId, sourceKey, kind, bucket,
            lookupResult.status == ThumbnailCacheLookupStatus::Invalid
                ? ActiveNavigationThumbnailFailureKind::CacheLookupInvalid
                : ActiveNavigationThumbnailFailureKind::CacheLookupFailed,
            lookupResult.errorString);
        state.activeJob.reset();
        if (kind == ThumbnailWorkKind::Background) {
            m_activeBackgroundRowIndex.reset();
        }
        if (kind == ThumbnailWorkKind::Background) {
            markBackgroundBucketCompleted(state, bucket);
            maybeScheduleBackgroundWork();
        } else {
            markBackgroundBucketCompleted(state, bucket);
            m_rowPort.applyFailed(sourceKey);
        }
        break;
    }
    maybeScheduleBackgroundWork();
}

void ActiveNavigationThumbnailWorkCoordinator::finishGeneration(quint64 jobId,
    const ThumbnailSourceKey& sourceKey, ActiveNavigationThumbnailDemandBucket bucket,
    ThumbnailWorkKind workKind, ThumbnailGenerationResult generationResult)
{
    const std::optional<std::size_t> rowIndex = m_rowPort.rowIndexForSourceKey(sourceKey);
    if (!rowIndex.has_value() || *rowIndex >= m_rows.size()) {
        qCDebug(kiriviewThumbnailLog) << "Rejecting stale thumbnail generation for missing source"
                                      << sourceKey.rowNumber << sourceKey.url;
        return;
    }

    WorkState& state = m_rows.at(*rowIndex);
    if (!state.activeJob.has_value() || state.activeJob->id != jobId
        || !sameFreshThumbnailSourceKey(state.activeJob->demand.sourceKey, sourceKey)
        || state.activeJob->demand.bucket != bucket || state.activeJob->kind != workKind) {
        qCDebug(kiriviewThumbnailLog)
            << "Rejecting stale thumbnail generation completion" << jobId << sourceKey.rowNumber
            << sourceKey.url << "bucket" << static_cast<int>(bucket);
        return;
    }

    const ThumbnailWorkKind kind = state.activeJob->kind;
    const AcceptedDemand demand = state.activeJob->demand;
    state.activeJob.reset();
    if (kind == ThumbnailWorkKind::Background) {
        m_activeBackgroundRowIndex.reset();
    }
    qCDebug(kiriviewThumbnailLog) << "Thumbnail generation finished" << jobId << "kind"
                                  << static_cast<int>(kind) << "number" << sourceKey.rowNumber
                                  << "bucket" << static_cast<int>(bucket) << "status"
                                  << static_cast<int>(generationResult.status);
    switch (generationResult.status) {
    case ThumbnailGenerationStatus::Ready: {
        if (kind == ThumbnailWorkKind::Background && m_rowPort.hasUsableReadyImage(sourceKey)) {
            markBackgroundBucketCompleted(state, bucket);
            break;
        }

        if (!m_rowPort.installReadyImage(sourceKey, generationResult.image,
                imageRetentionPriority(kind, demand.priority),
                kind == ThumbnailWorkKind::Background)) {
            recordFailureDiagnostic(jobId, sourceKey, kind, bucket,
                ActiveNavigationThumbnailFailureKind::ImageStoreInsertFailed, {});
            markBackgroundBucketCompleted(state, bucket);
            if (kind != ThumbnailWorkKind::Background) {
                m_rowPort.applyFailed(sourceKey);
            }
            break;
        }
        qCDebug(kiriviewThumbnailLog)
            << "Installed generated thumbnail in image store, kind" << static_cast<int>(kind);
        markBackgroundBucketCompleted(state, bucket);
        break;
    }
    case ThumbnailGenerationStatus::Failed:
        recordFailureDiagnostic(jobId, sourceKey, kind, bucket,
            ActiveNavigationThumbnailFailureKind::GenerationFailed, generationResult.errorString);
        if (kind == ThumbnailWorkKind::Background) {
            markBackgroundBucketCompleted(state, bucket);
        } else {
            markBackgroundBucketCompleted(state, bucket);
            m_rowPort.applyFailed(sourceKey);
        }
        break;
    }
    maybeScheduleBackgroundWork();
}
}
