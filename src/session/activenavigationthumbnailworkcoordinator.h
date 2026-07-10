// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILWORKCOORDINATOR_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILWORKCOORDINATOR_H

#include "session/activenavigationthumbnaildemand.h"
#include "session/activenavigationthumbnailjobexecutor.h"
#include "session/activenavigationthumbnailrowstore.h"

#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

class QObject;

namespace kiriview {
struct ActiveNavigationThumbnailFailureDiagnostic
{
    ActiveNavigationThumbnailWorkId workId;
    ThumbnailSourceKey sourceKey;
    ActiveNavigationThumbnailWorkKind workKind = ActiveNavigationThumbnailWorkKind::Foreground;
    ActiveNavigationThumbnailDemandBucket bucket = ActiveNavigationThumbnailDemandBucket::None;
    ActiveNavigationThumbnailFailureKind failureKind
        = ActiveNavigationThumbnailFailureKind::CacheLookupFailed;
    QString errorString;
};

class ActiveNavigationThumbnailWorkCoordinator final
{
public:
    ActiveNavigationThumbnailWorkCoordinator(QObject* owner,
        ActiveNavigationThumbnailRowPort& rowPort, ThumbnailCacheLookupProvider lookupProvider,
        ThumbnailGenerationProvider generationProvider, ThumbnailSourceAdapter sourceAdapter);
    ~ActiveNavigationThumbnailWorkCoordinator();

    ActiveNavigationThumbnailWorkCoordinator(const ActiveNavigationThumbnailWorkCoordinator&)
        = delete;
    ActiveNavigationThumbnailWorkCoordinator& operator=(
        const ActiveNavigationThumbnailWorkCoordinator&)
        = delete;

    void resetRows(std::size_t rowCount, quint64 navigationGeneration);
    void invalidateRows();
    bool beginDemandWindow(quint64 navigationGeneration);
    void finishDemandWindow(quint64 navigationGeneration);
    bool reportDemand(int number, const QUrl& url, ActiveNavigationThumbnailDemandBucket bucket,
        ActiveNavigationThumbnailDemandPriority priority, quint64 navigationGeneration);
    const std::vector<ActiveNavigationThumbnailFailureDiagnostic>& failureDiagnostics() const;

private:
    struct AcceptedDemand
    {
        ThumbnailSourceKey sourceKey;
        ActiveNavigationThumbnailDemandBucket bucket = ActiveNavigationThumbnailDemandBucket::None;
        ActiveNavigationThumbnailDemandPriority priority
            = ActiveNavigationThumbnailDemandPriority::Nearby;
        ThumbnailSourceAdapterPlan sourcePlan;
    };

    struct ActiveWorkClaim
    {
        ActiveNavigationThumbnailWorkId id;
        ActiveNavigationThumbnailWorkKind kind = ActiveNavigationThumbnailWorkKind::Foreground;
        AcceptedDemand demand;
    };

    struct WorkState
    {
        std::optional<AcceptedDemand> acceptedDemand;
        std::optional<ActiveWorkClaim> activeWork;
        std::vector<ActiveNavigationThumbnailDemandBucket> completedBackgroundBuckets;
        quint64 demandWindowEpoch = 0;
    };

    static bool sameFreshThumbnailSourceKey(
        const ThumbnailSourceKey& left, const ThumbnailSourceKey& right);
    static bool sameSourceAdapterPlan(
        const ThumbnailSourceAdapterPlan& left, const ThumbnailSourceAdapterPlan& right);
    static bool sameAcceptedDemand(const AcceptedDemand& left, const AcceptedDemand& right);
    static bool supportsGeneratedThumbnail(const ThumbnailSourceAdapterPlan& plan);
    static ThumbnailImageRetentionPriority imageRetentionPriority(
        ActiveNavigationThumbnailDemandPriority priority);
    static ThumbnailImageRetentionPriority imageRetentionPriority(
        ActiveNavigationThumbnailWorkKind kind, ActiveNavigationThumbnailDemandPriority priority);

    void markDemandWindowRow(std::size_t row, WorkState& state);
    void expireDemandOutsideCurrentWindow();
    void cancelActiveWork(std::size_t row, WorkState& state);
    void cancelActiveBackgroundWork();
    void cancelAllActiveWork();
    bool hasActiveForegroundWork() const;
    void startWork(
        WorkState& state, const AcceptedDemand& demand, ActiveNavigationThumbnailWorkKind kind);
    void recordFailureDiagnostic(ActiveNavigationThumbnailWorkId workId,
        const ThumbnailSourceKey& sourceKey, ActiveNavigationThumbnailWorkKind workKind,
        ActiveNavigationThumbnailDemandBucket bucket,
        ActiveNavigationThumbnailFailureKind failureKind, const QString& errorString);
    bool activeWorkMatches(const WorkState& state, ActiveNavigationThumbnailWorkId workId,
        const AcceptedDemand& demand, ActiveNavigationThumbnailWorkKind kind) const;
    bool backgroundBucketCompleted(
        const WorkState& state, ActiveNavigationThumbnailDemandBucket bucket) const;
    void markBackgroundBucketCompleted(
        WorkState& state, ActiveNavigationThumbnailDemandBucket bucket);
    void maybeScheduleBackgroundWork();
    ThumbnailSourceAdapterPlan sourcePlanForDemand(const ThumbnailSourceKey& sourceKey,
        ActiveNavigationThumbnailDemandBucket bucket,
        ActiveNavigationThumbnailDemandPriority priority) const;
    void startBackgroundWork(std::size_t row, WorkState& state,
        ActiveNavigationThumbnailDemandBucket bucket, ThumbnailSourceAdapterPlan sourcePlan);
    void finishWork(ActiveNavigationThumbnailWorkCompletion completion);

    ActiveNavigationThumbnailRowPort& m_rowPort;
    ActiveNavigationThumbnailJobExecutor m_executor;
    ThumbnailSourceAdapter m_sourceAdapter;
    ActiveNavigationThumbnailDemandTracker m_demandTracker;
    std::vector<WorkState> m_rows;
    quint64 m_navigationGeneration = 0;
    quint64 m_nextWorkId = 1;
    std::vector<ActiveNavigationThumbnailFailureDiagnostic> m_failureDiagnostics;
    bool m_backgroundArmed = false;
    bool m_demandWindowOpen = false;
    quint64 m_demandWindowGeneration = 0;
    quint64 m_demandWindowEpoch = 0;
    std::vector<std::size_t> m_demandWindowRows;
    std::vector<std::size_t> m_previousDemandWindowRows;
    std::optional<std::size_t> m_activeBackgroundRowIndex;
};
}

#endif
