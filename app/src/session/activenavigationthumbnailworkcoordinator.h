// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILWORKCOORDINATOR_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILWORKCOORDINATOR_H

#include "session/activenavigationthumbnailjobexecutor.h"
#include "session/activenavigationthumbnailrowstore.h"
#include "session/activenavigationthumbnailscheduler.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>
#include <vector>

namespace kiriview {
struct ActiveNavigationThumbnailFailureDiagnostic
{
    ActiveNavigationThumbnailWorkId workId;
    ThumbnailSourceRevisionKey sourceKey;
    ActiveNavigationThumbnailWorkKind workKind = ActiveNavigationThumbnailWorkKind::Foreground;
    ActiveNavigationThumbnailDemandBucket bucket = ActiveNavigationThumbnailDemandBucket::None;
    ActiveNavigationThumbnailFailureKind failureKind
        = ActiveNavigationThumbnailFailureKind::CacheLookupFailed;
    QString errorString;
};

using ActiveNavigationThumbnailFailureDiagnosticCallback
    = std::function<void(const ActiveNavigationThumbnailFailureDiagnostic&)>;

class ActiveNavigationThumbnailWorkCoordinator final : public QObject
{
public:
    ActiveNavigationThumbnailWorkCoordinator(QObject* owner,
        ActiveNavigationThumbnailRowPort& rowPort, ThumbnailCacheLookupProvider lookupProvider,
        ThumbnailGenerationProvider generationProvider, ThumbnailSourceAdapter sourceAdapter,
        ActiveNavigationThumbnailFailureDiagnosticCallback failureDiagnosticCallback = {});
    ~ActiveNavigationThumbnailWorkCoordinator() override;

    ActiveNavigationThumbnailWorkCoordinator(const ActiveNavigationThumbnailWorkCoordinator&)
        = delete;
    ActiveNavigationThumbnailWorkCoordinator& operator=(
        const ActiveNavigationThumbnailWorkCoordinator&)
        = delete;

    bool resetRows(ActiveNavigationThumbnailSchedulingSnapshot snapshot);
    bool refreshRows(ActiveNavigationThumbnailSchedulingSnapshot snapshot);
    void invalidateRows();
    void setCurrentNumber(int currentNumber);
    bool replaceDemandSnapshot(ActiveNavigationThumbnailDemandSnapshot snapshot);

private:
    static ThumbnailImageRetentionPriority imageRetentionPriority(
        ActiveNavigationThumbnailRetentionClass retentionClass);
    void applyEffects(std::vector<ActiveNavigationThumbnailScheduleEffect> effects);
    void applyEffect(ActiveNavigationThumbnailCancelWorkEffect effect);
    void applyEffect(ActiveNavigationThumbnailStartWorkEffect effect);
    void applyEffect(ActiveNavigationThumbnailApplyPendingEffect effect);
    void applyEffect(ActiveNavigationThumbnailApplyUnsupportedEffect effect);
    void applyEffect(ActiveNavigationThumbnailUpdateRetentionEffect effect);
    void applyEffect(ActiveNavigationThumbnailAcceptCompletionEffect effect);
    void applyEffect(ActiveNavigationThumbnailScheduleContinuationEffect effect);
    void publishCompletion(const ActiveNavigationThumbnailAcceptCompletionEffect& effect);
    void reportFailureDiagnostic(ActiveNavigationThumbnailWorkId workId,
        const ThumbnailSourceRevisionKey& sourceKey, ActiveNavigationThumbnailWorkKind workKind,
        ActiveNavigationThumbnailDemandBucket bucket,
        ActiveNavigationThumbnailFailureKind failureKind, const QString& errorString);

    ActiveNavigationThumbnailRowPort& m_rowPort;
    ActiveNavigationThumbnailScheduler m_scheduler;
    ActiveNavigationThumbnailJobExecutor m_executor;
    ActiveNavigationThumbnailFailureDiagnosticCallback m_failureDiagnosticCallback;
};
}

#endif
