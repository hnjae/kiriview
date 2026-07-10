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

QString fallbackThumbnailFailureError(kiriview::ActiveNavigationThumbnailFailureKind failureKind)
{
    switch (failureKind) {
    case kiriview::ActiveNavigationThumbnailFailureKind::CacheLookupProviderUnavailable:
        return QStringLiteral("Thumbnail cache lookup provider is unavailable.");
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
    : m_rowPort(rowPort)
    , m_executor(owner, std::move(lookupProvider), std::move(generationProvider),
          [this](ActiveNavigationThumbnailWorkCompletion completion) {
              finishWork(std::move(completion));
          })
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
    cancelAllActiveWork();
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
    cancelActiveBackgroundWork();
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
    cancelActiveBackgroundWork();

    if (state.acceptedDemand.has_value()
        && sameFreshThumbnailSourceKey(state.acceptedDemand->sourceKey, demand.sourceKey)
        && state.acceptedDemand->bucket == demand.bucket
        && state.acceptedDemand->priority != demand.priority) {
        state.acceptedDemand = demand;
        m_rowPort.updateRetentionPriority(sourceKey, imageRetentionPriority(demand.priority));
        if (state.activeWork.has_value()) {
            state.activeWork->demand = demand;
        }
        if (!m_demandWindowOpen) {
            maybeScheduleBackgroundWork();
        }
        return true;
    }

    cancelActiveWork(*rowIndex, state);
    state.acceptedDemand = demand;
    if (supportsGeneratedThumbnail(sourcePlan)) {
        m_rowPort.applyPending(sourceKey);
        state.activeWork = ActiveWorkClaim {
            { m_nextWorkId++ },
            ActiveNavigationThumbnailWorkKind::Foreground,
            demand,
        };
        startWork(state, demand, ActiveNavigationThumbnailWorkKind::Foreground);
    } else {
        m_rowPort.applyUnsupported(sourceKey);
        state.activeWork.reset();
        qCDebug(kiriviewThumbnailLog) << "Thumbnail demand unsupported" << number << url;
    }
    if (!m_demandWindowOpen) {
        maybeScheduleBackgroundWork();
    }
    return true;
}

const std::vector<ActiveNavigationThumbnailFailureDiagnostic>&
ActiveNavigationThumbnailWorkCoordinator::failureDiagnostics() const
{
    return m_failureDiagnostics;
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
    ActiveNavigationThumbnailWorkKind kind, ActiveNavigationThumbnailDemandPriority priority)
{
    if (kind == ActiveNavigationThumbnailWorkKind::Background) {
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

        if (state.activeWork.has_value()) {
            cancelActiveWork(rowIndex, state);
        }
        state.acceptedDemand.reset();
        m_rowPort.updateRetentionPriority(
            m_rowPort.sourceKeyAt(rowIndex), ThumbnailImageRetentionPriority::Background);
    }

    m_previousDemandWindowRows.clear();
}

void ActiveNavigationThumbnailWorkCoordinator::cancelActiveWork(std::size_t row, WorkState& state)
{
    if (!state.activeWork.has_value()) {
        return;
    }

    qCDebug(kiriviewThumbnailLog) << "Canceling thumbnail job" << state.activeWork->id.value
                                  << "kind" << static_cast<int>(state.activeWork->kind) << "number"
                                  << m_rowPort.sourceKeyAt(row).rowNumber << "bucket"
                                  << static_cast<int>(state.activeWork->demand.bucket);
    if (state.activeWork->kind == ActiveNavigationThumbnailWorkKind::Background) {
        m_activeBackgroundRowIndex.reset();
    }
    m_executor.cancel(state.activeWork->id);
    state.activeWork.reset();
}

void ActiveNavigationThumbnailWorkCoordinator::cancelActiveBackgroundWork()
{
    if (!m_activeBackgroundRowIndex.has_value() || *m_activeBackgroundRowIndex >= m_rows.size()) {
        m_activeBackgroundRowIndex.reset();
        return;
    }

    WorkState& state = m_rows.at(*m_activeBackgroundRowIndex);
    if (state.activeWork.has_value()
        && state.activeWork->kind == ActiveNavigationThumbnailWorkKind::Background) {
        cancelActiveWork(*m_activeBackgroundRowIndex, state);
        return;
    }

    m_activeBackgroundRowIndex.reset();
}

void ActiveNavigationThumbnailWorkCoordinator::cancelAllActiveWork()
{
    for (std::size_t row = 0; row < m_rows.size(); ++row) {
        cancelActiveWork(row, m_rows.at(row));
    }
}

bool ActiveNavigationThumbnailWorkCoordinator::hasActiveForegroundWork() const
{
    return std::any_of(m_rows.cbegin(), m_rows.cend(), [](const WorkState& state) {
        return state.activeWork.has_value()
            && state.activeWork->kind == ActiveNavigationThumbnailWorkKind::Foreground;
    });
}

void ActiveNavigationThumbnailWorkCoordinator::startWork(
    WorkState& state, const AcceptedDemand& demand, ActiveNavigationThumbnailWorkKind kind)
{
    if (!state.activeWork.has_value()) {
        return;
    }
    m_executor.start(ActiveNavigationThumbnailWorkRequest {
        state.activeWork->id,
        demand.sourceKey,
        demand.bucket,
        kind,
        demand.sourcePlan,
    });
}

void ActiveNavigationThumbnailWorkCoordinator::recordFailureDiagnostic(
    ActiveNavigationThumbnailWorkId workId, const ThumbnailSourceKey& sourceKey,
    ActiveNavigationThumbnailWorkKind workKind, ActiveNavigationThumbnailDemandBucket bucket,
    ActiveNavigationThumbnailFailureKind failureKind, const QString& errorString)
{
    const QString resolvedErrorString
        = errorString.isEmpty() ? fallbackThumbnailFailureError(failureKind) : errorString;
    m_failureDiagnostics.push_back(ActiveNavigationThumbnailFailureDiagnostic {
        workId,
        sourceKey,
        workKind,
        bucket,
        failureKind,
        resolvedErrorString,
    });
    qCDebug(kiriviewThumbnailLog) << "Thumbnail failure diagnostic" << workId.value << "kind"
                                  << static_cast<int>(workKind) << "number" << sourceKey.rowNumber
                                  << "url" << sourceKey.url << "bucket" << static_cast<int>(bucket)
                                  << "failure" << static_cast<int>(failureKind) << "error"
                                  << resolvedErrorString;
}

bool ActiveNavigationThumbnailWorkCoordinator::activeWorkMatches(const WorkState& state,
    ActiveNavigationThumbnailWorkId workId, const AcceptedDemand& demand,
    ActiveNavigationThumbnailWorkKind kind) const
{
    return state.activeWork.has_value() && state.activeWork->id == workId
        && state.activeWork->kind == kind && sameAcceptedDemand(state.activeWork->demand, demand);
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
    if (!m_backgroundArmed || hasActiveForegroundWork()) {
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
            if (state.demandWindowEpoch != m_demandWindowEpoch || state.activeWork.has_value()) {
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
    state.activeWork = ActiveWorkClaim {
        { m_nextWorkId++ },
        ActiveNavigationThumbnailWorkKind::Background,
        demand,
    };
    m_activeBackgroundRowIndex = row;
    startWork(state, demand, ActiveNavigationThumbnailWorkKind::Background);
}

void ActiveNavigationThumbnailWorkCoordinator::finishWork(
    ActiveNavigationThumbnailWorkCompletion completion)
{
    const std::optional<std::size_t> rowIndex
        = m_rowPort.rowIndexForSourceKey(completion.sourceKey);
    if (!rowIndex.has_value() || *rowIndex >= m_rows.size()) {
        return;
    }

    WorkState& state = m_rows.at(*rowIndex);
    if (!state.activeWork.has_value() || state.activeWork->id != completion.workId
        || !sameFreshThumbnailSourceKey(state.activeWork->demand.sourceKey, completion.sourceKey)
        || state.activeWork->demand.bucket != completion.bucket
        || state.activeWork->kind != completion.workKind) {
        return;
    }

    const ActiveNavigationThumbnailWorkKind kind = state.activeWork->kind;
    const AcceptedDemand demand = state.activeWork->demand;
    state.activeWork.reset();
    if (kind == ActiveNavigationThumbnailWorkKind::Background) {
        m_activeBackgroundRowIndex.reset();
    }
    if (completion.result.kind == ActiveNavigationThumbnailWorkResultKind::Ready) {
        if (kind == ActiveNavigationThumbnailWorkKind::Background
            && m_rowPort.hasUsableReadyImage(completion.sourceKey)) {
            markBackgroundBucketCompleted(state, completion.bucket);
            maybeScheduleBackgroundWork();
            return;
        }
        if (!m_rowPort.installReadyImage(completion.sourceKey, completion.result.image,
                imageRetentionPriority(kind, demand.priority),
                kind == ActiveNavigationThumbnailWorkKind::Background)) {
            recordFailureDiagnostic(completion.workId, completion.sourceKey, kind,
                completion.bucket, ActiveNavigationThumbnailFailureKind::ImageStoreInsertFailed,
                {});
            markBackgroundBucketCompleted(state, completion.bucket);
            if (kind != ActiveNavigationThumbnailWorkKind::Background
                && !m_rowPort.hasUsableReadyImage(completion.sourceKey)) {
                m_rowPort.applyFailed(completion.sourceKey);
            }
        } else {
            markBackgroundBucketCompleted(state, completion.bucket);
        }
    } else {
        recordFailureDiagnostic(completion.workId, completion.sourceKey, kind, completion.bucket,
            completion.result.failureKind, completion.result.errorString);
        markBackgroundBucketCompleted(state, completion.bucket);
        if (kind == ActiveNavigationThumbnailWorkKind::Background) {
            maybeScheduleBackgroundWork();
        } else if (!m_rowPort.hasUsableReadyImage(completion.sourceKey)) {
            m_rowPort.applyFailed(completion.sourceKey);
        }
    }
    maybeScheduleBackgroundWork();
}
}
