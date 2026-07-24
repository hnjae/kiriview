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
    Q_DISABLE_COPY_MOVE(ActiveNavigationThumbnailWorkCoordinator)

    bool resetRows(ActiveNavigationThumbnailSchedulingSnapshot snapshot);
    bool refreshRows(ActiveNavigationThumbnailSchedulingSnapshot snapshot);
    void invalidateRows();
    void setCurrentNumber(int currentNumber);
    bool replaceDemandSnapshot(const ActiveNavigationThumbnailDemandSnapshot& snapshot);

private:
    static ThumbnailImageRetentionPriority imageRetentionPriority(
        ActiveNavigationThumbnailRetentionClass retentionClass);
    void applyEffects(std::vector<ActiveNavigationThumbnailScheduleEffect> effects);
    void applyEffect(ActiveNavigationThumbnailCancelWorkEffect effect);
    void applyEffect(ActiveNavigationThumbnailStartWorkEffect effect);
    void applyEffect(const ActiveNavigationThumbnailApplyPendingEffect& effect);
    void applyEffect(const ActiveNavigationThumbnailApplyUnsupportedEffect& effect);
    void applyEffect(const ActiveNavigationThumbnailUpdateRetentionEffect& effect);
    void applyEffect(const ActiveNavigationThumbnailAcceptCompletionEffect& effect);
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
