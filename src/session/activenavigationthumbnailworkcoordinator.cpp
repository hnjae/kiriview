// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailworkcoordinator.h"

#include "session/thumbnaillogging.h"

#include <QDebug>
#include <utility>

namespace {
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
ActiveNavigationThumbnailWorkCoordinator::ActiveNavigationThumbnailWorkCoordinator(QObject* owner,
    ActiveNavigationThumbnailRowPort& rowPort, ThumbnailCacheLookupProvider lookupProvider,
    ThumbnailGenerationProvider generationProvider, ThumbnailSourceAdapter sourceAdapter)
    : m_rowPort(rowPort)
    , m_scheduler(std::move(sourceAdapter))
    , m_executor(owner, std::move(lookupProvider), std::move(generationProvider),
          [this](ActiveNavigationThumbnailWorkCompletion completion) {
              applyEffects(m_scheduler.acceptCompletion(std::move(completion)));
          })
{
}

ActiveNavigationThumbnailWorkCoordinator::~ActiveNavigationThumbnailWorkCoordinator()
{
    invalidateRows();
}

void ActiveNavigationThumbnailWorkCoordinator::resetRows(
    std::vector<ThumbnailSourceKey> rows, quint64 navigationGeneration)
{
    applyEffects(m_scheduler.reset(std::move(rows), navigationGeneration));
}

void ActiveNavigationThumbnailWorkCoordinator::invalidateRows()
{
    applyEffects(m_scheduler.invalidate());
}

void ActiveNavigationThumbnailWorkCoordinator::setCurrentNumber(int currentNumber)
{
    applyEffects(m_scheduler.setCurrentNumber(currentNumber));
}

bool ActiveNavigationThumbnailWorkCoordinator::replaceDemandSnapshot(
    ActiveNavigationThumbnailDemandSnapshot snapshot)
{
    auto effects = m_scheduler.replaceDemandSnapshot(std::move(snapshot));
    if (!effects.has_value()) {
        return false;
    }
    applyEffects(std::move(*effects));
    return true;
}

const std::vector<ActiveNavigationThumbnailFailureDiagnostic>&
ActiveNavigationThumbnailWorkCoordinator::failureDiagnostics() const
{
    return m_failureDiagnostics;
}

ThumbnailImageRetentionPriority ActiveNavigationThumbnailWorkCoordinator::imageRetentionPriority(
    ActiveNavigationThumbnailWorkKind kind, ActiveNavigationThumbnailDemandPriority priority)
{
    if (kind == ActiveNavigationThumbnailWorkKind::Background) {
        return ThumbnailImageRetentionPriority::Background;
    }
    return priority == ActiveNavigationThumbnailDemandPriority::Visible
        ? ThumbnailImageRetentionPriority::Visible
        : ThumbnailImageRetentionPriority::Nearby;
}

void ActiveNavigationThumbnailWorkCoordinator::applyEffects(
    std::vector<ActiveNavigationThumbnailScheduleEffect> effects)
{
    for (ActiveNavigationThumbnailScheduleEffect& effect : effects) {
        switch (effect.kind) {
        case ActiveNavigationThumbnailScheduleEffectKind::CancelWork:
            m_executor.cancel(effect.workId);
            break;
        case ActiveNavigationThumbnailScheduleEffectKind::StartWork:
            m_executor.start(std::move(effect.workRequest));
            break;
        case ActiveNavigationThumbnailScheduleEffectKind::ApplyPending:
            m_rowPort.applyPending(effect.sourceKey);
            break;
        case ActiveNavigationThumbnailScheduleEffectKind::ApplyUnsupported:
            m_rowPort.applyUnsupported(effect.sourceKey);
            break;
        case ActiveNavigationThumbnailScheduleEffectKind::UpdateRetention:
            m_rowPort.updateRetentionPriority(effect.sourceKey, effect.retentionPriority);
            break;
        case ActiveNavigationThumbnailScheduleEffectKind::AcceptCompletion:
            publishCompletion(effect);
            break;
        }
    }
}

void ActiveNavigationThumbnailWorkCoordinator::publishCompletion(
    const ActiveNavigationThumbnailScheduleEffect& effect)
{
    const auto& completion = effect.completion;
    if (completion.result.kind == ActiveNavigationThumbnailWorkResultKind::Ready) {
        if (completion.workKind == ActiveNavigationThumbnailWorkKind::Background
            && m_rowPort.hasUsableReadyImage(completion.sourceKey)) {
            return;
        }
        if (!m_rowPort.installReadyImage(completion.sourceKey, completion.result.image,
                imageRetentionPriority(completion.workKind, effect.demandPriority),
                completion.workKind == ActiveNavigationThumbnailWorkKind::Background)) {
            recordFailureDiagnostic(completion.workId, completion.sourceKey, completion.workKind,
                completion.bucket, ActiveNavigationThumbnailFailureKind::ImageStoreInsertFailed,
                {});
            if (completion.workKind != ActiveNavigationThumbnailWorkKind::Background
                && !m_rowPort.hasUsableReadyImage(completion.sourceKey)) {
                m_rowPort.applyFailed(completion.sourceKey);
            }
        }
        return;
    }
    recordFailureDiagnostic(completion.workId, completion.sourceKey, completion.workKind,
        completion.bucket, completion.result.failureKind, completion.result.errorString);
    if (completion.workKind != ActiveNavigationThumbnailWorkKind::Background
        && !m_rowPort.hasUsableReadyImage(completion.sourceKey)) {
        m_rowPort.applyFailed(completion.sourceKey);
    }
}

void ActiveNavigationThumbnailWorkCoordinator::recordFailureDiagnostic(
    ActiveNavigationThumbnailWorkId workId, const ThumbnailSourceKey& sourceKey,
    ActiveNavigationThumbnailWorkKind workKind, ActiveNavigationThumbnailDemandBucket bucket,
    ActiveNavigationThumbnailFailureKind failureKind, const QString& errorString)
{
    const QString resolvedErrorString
        = errorString.isEmpty() ? fallbackThumbnailFailureError(failureKind) : errorString;
    m_failureDiagnostics.push_back(
        { workId, sourceKey, workKind, bucket, failureKind, resolvedErrorString });
    qCDebug(kiriviewThumbnailLog) << "Thumbnail failure diagnostic" << workId.value << "kind"
                                  << static_cast<int>(workKind) << "number" << sourceKey.rowNumber
                                  << "url" << sourceKey.url << "bucket" << static_cast<int>(bucket)
                                  << "failure" << static_cast<int>(failureKind) << "error"
                                  << resolvedErrorString;
}
}
