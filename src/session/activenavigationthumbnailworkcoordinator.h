// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILWORKCOORDINATOR_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILWORKCOORDINATOR_H

#include "session/activenavigationthumbnaildemand.h"
#include "session/activenavigationthumbnailrowstore.h"
#include "thumbnail/thumbnailcachelookup.h"
#include "thumbnail/thumbnailgeneration.h"
#include "thumbnail/thumbnailoriginalidentity.h"

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

class QObject;

namespace kiriview {
enum class ActiveNavigationThumbnailWorkKind {
    Foreground,
    Background,
};

enum class ActiveNavigationThumbnailFailureKind {
    CacheLookupInvalid,
    CacheLookupFailed,
    GenerationFailed,
    ImageStoreInsertFailed,
    GenerationProviderUnavailable,
};

struct ActiveNavigationThumbnailFailureDiagnostic
{
    quint64 jobId = 0;
    ThumbnailSourceKey sourceKey;
    ActiveNavigationThumbnailWorkKind workKind = ActiveNavigationThumbnailWorkKind::Foreground;
    ActiveNavigationThumbnailDemandBucket bucket = ActiveNavigationThumbnailDemandBucket::None;
    ActiveNavigationThumbnailFailureKind failureKind
        = ActiveNavigationThumbnailFailureKind::CacheLookupFailed;
    QString errorString;
};

struct ActiveNavigationThumbnailCompletion
{
    ThumbnailSourceKey sourceKey;
    ActiveNavigationThumbnailDemandBucket bucket = ActiveNavigationThumbnailDemandBucket::None;
    ActiveNavigationThumbnailResult result;
};

enum class ThumbnailSourceAdapterPlanKind {
    Unsupported,
    CacheableLocalFile,
    CacheableOpenedCollectionEntry,
    InMemoryOnly,
};

struct ThumbnailSourceAdapterRequest
{
    ThumbnailSourceKey sourceKey;
    ActiveNavigationThumbnailDemandBucket requestedBucket
        = ActiveNavigationThumbnailDemandBucket::None;
    ActiveNavigationThumbnailDemandPriority priority
        = ActiveNavigationThumbnailDemandPriority::Nearby;
};

struct ThumbnailSourceAdapterPlan
{
    ThumbnailSourceAdapterPlanKind kind = ThumbnailSourceAdapterPlanKind::Unsupported;
    QByteArray localPathBytes;
    ThumbnailOriginalIdentity originalIdentity;
    OpenedCollectionScopeLocation openedCollectionScope;
};

using ThumbnailSourceAdapter
    = std::function<ThumbnailSourceAdapterPlan(ThumbnailSourceAdapterRequest)>;

ThumbnailSourceAdapter defaultThumbnailSourceAdapter();

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
    bool acceptCompletion(const ActiveNavigationThumbnailCompletion& completion);

    const std::vector<ActiveNavigationThumbnailFailureDiagnostic>& failureDiagnostics() const;
    qsizetype activeJobCount() const;
    qsizetype canceledJobCount() const;

private:
    enum class ThumbnailWorkKind {
        Foreground,
        Background,
    };

    struct AcceptedDemand
    {
        ThumbnailSourceKey sourceKey;
        ActiveNavigationThumbnailDemandBucket bucket = ActiveNavigationThumbnailDemandBucket::None;
        ActiveNavigationThumbnailDemandPriority priority
            = ActiveNavigationThumbnailDemandPriority::Nearby;
        ThumbnailSourceAdapterPlan sourcePlan;
    };

    struct ActiveJobSlot
    {
        quint64 id = 0;
        ThumbnailWorkKind kind = ThumbnailWorkKind::Foreground;
        AcceptedDemand demand;
        ImageIoJob job;
    };

    struct WorkState
    {
        std::optional<AcceptedDemand> acceptedDemand;
        std::optional<ActiveJobSlot> activeJob;
        std::vector<ActiveNavigationThumbnailDemandBucket> completedBackgroundBuckets;
        quint64 demandWindowEpoch = 0;
    };

    static bool sameFreshThumbnailSourceKey(
        const ThumbnailSourceKey& left, const ThumbnailSourceKey& right);
    static bool sameSourceAdapterPlan(
        const ThumbnailSourceAdapterPlan& left, const ThumbnailSourceAdapterPlan& right);
    static bool sameAcceptedDemand(const AcceptedDemand& left, const AcceptedDemand& right);
    static bool supportsGeneratedThumbnail(const ThumbnailSourceAdapterPlan& plan);
    static bool usesCacheLookup(const ThumbnailSourceAdapterPlan& plan);
    static bool enablesCacheInstall(const ThumbnailSourceAdapterPlan& plan);
    static ThumbnailImageRetentionPriority imageRetentionPriority(
        ActiveNavigationThumbnailDemandPriority priority);
    static ThumbnailImageRetentionPriority imageRetentionPriority(
        ThumbnailWorkKind kind, ActiveNavigationThumbnailDemandPriority priority);

    void markDemandWindowRow(std::size_t row, WorkState& state);
    void expireDemandOutsideCurrentWindow();
    void cancelActiveJob(std::size_t row, WorkState& state);
    void cancelActiveBackgroundJob();
    void cancelAllActiveJobs();
    bool hasActiveForegroundJob() const;
    void startLookupJob(WorkState& state, const AcceptedDemand& demand, ThumbnailWorkKind kind);
    void startGenerationJob(WorkState& state, const AcceptedDemand& demand, ThumbnailWorkKind kind);
    void recordFailureDiagnostic(quint64 jobId, const ThumbnailSourceKey& sourceKey,
        ThumbnailWorkKind workKind, ActiveNavigationThumbnailDemandBucket bucket,
        ActiveNavigationThumbnailFailureKind failureKind, const QString& errorString);
    bool activeJobMatches(const WorkState& state, quint64 jobId, const AcceptedDemand& demand,
        ThumbnailWorkKind kind) const;
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
    void finishLookup(quint64 jobId, const ThumbnailSourceKey& sourceKey,
        ActiveNavigationThumbnailDemandBucket bucket, ThumbnailWorkKind workKind,
        ThumbnailCacheLookupResult lookupResult);
    void finishGeneration(quint64 jobId, const ThumbnailSourceKey& sourceKey,
        ActiveNavigationThumbnailDemandBucket bucket, ThumbnailWorkKind workKind,
        ThumbnailGenerationResult generationResult);

    QObject* m_owner = nullptr;
    ActiveNavigationThumbnailRowPort& m_rowPort;
    ThumbnailCacheLookupProvider m_lookupProvider;
    ThumbnailGenerationProvider m_generationProvider;
    ThumbnailSourceAdapter m_sourceAdapter;
    ActiveNavigationThumbnailDemandTracker m_demandTracker;
    std::vector<WorkState> m_rows;
    quint64 m_navigationGeneration = 0;
    quint64 m_nextJobId = 1;
    std::vector<quint64> m_canceledJobIds;
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
