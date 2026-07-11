// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILWORKCOORDINATOR_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILWORKCOORDINATOR_H

#include "session/activenavigationthumbnailjobexecutor.h"
#include "session/activenavigationthumbnailrowstore.h"
#include "session/activenavigationthumbnailscheduler.h"

#include <QString>
#include <QUrl>
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

    void resetRows(std::vector<ThumbnailSourceKey> rows, quint64 navigationGeneration);
    void invalidateRows();
    void setCurrentNumber(int currentNumber);
    bool replaceDemandSnapshot(ActiveNavigationThumbnailDemandSnapshot snapshot);
    const std::vector<ActiveNavigationThumbnailFailureDiagnostic>& failureDiagnostics() const;

private:
    static ThumbnailImageRetentionPriority imageRetentionPriority(
        ActiveNavigationThumbnailWorkKind kind, ActiveNavigationThumbnailDemandPriority priority);
    void applyEffects(std::vector<ActiveNavigationThumbnailScheduleEffect> effects);
    void publishCompletion(const ActiveNavigationThumbnailScheduleEffect& effect);
    void recordFailureDiagnostic(ActiveNavigationThumbnailWorkId workId,
        const ThumbnailSourceKey& sourceKey, ActiveNavigationThumbnailWorkKind workKind,
        ActiveNavigationThumbnailDemandBucket bucket,
        ActiveNavigationThumbnailFailureKind failureKind, const QString& errorString);

    ActiveNavigationThumbnailRowPort& m_rowPort;
    ActiveNavigationThumbnailScheduler m_scheduler;
    ActiveNavigationThumbnailJobExecutor m_executor;
    std::vector<ActiveNavigationThumbnailFailureDiagnostic> m_failureDiagnostics;
};
}

#endif
