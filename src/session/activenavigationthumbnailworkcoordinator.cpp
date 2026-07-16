// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailworkcoordinator.h"

#include "session/thumbnaillogging.h"

#include <QDebug>
#include <QMetaObject>
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
    ThumbnailGenerationProvider generationProvider, ThumbnailSourceAdapter sourceAdapter,
    ActiveNavigationThumbnailFailureDiagnosticCallback failureDiagnosticCallback)
    : m_rowPort(rowPort)
    , m_scheduler(std::move(sourceAdapter))
    , m_executor(owner, std::move(lookupProvider), std::move(generationProvider),
          [this](ActiveNavigationThumbnailWorkCompletion completion) {
              applyEffects(m_scheduler.acceptCompletion(std::move(completion)));
          })
    , m_failureDiagnosticCallback(std::move(failureDiagnosticCallback))
{
}

ActiveNavigationThumbnailWorkCoordinator::~ActiveNavigationThumbnailWorkCoordinator()
{
    try {
        invalidateRows();
    } catch (...) {
    }
}

bool ActiveNavigationThumbnailWorkCoordinator::resetRows(
    ActiveNavigationThumbnailSchedulingSnapshot snapshot)
{
    auto effects = m_scheduler.reset(std::move(snapshot));
    if (!effects.has_value()) {
        return false;
    }
    applyEffects(std::move(*effects));
    return true;
}

bool ActiveNavigationThumbnailWorkCoordinator::refreshRows(
    ActiveNavigationThumbnailSchedulingSnapshot snapshot)
{
    return m_scheduler.refreshRows(std::move(snapshot));
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

ThumbnailImageRetentionPriority ActiveNavigationThumbnailWorkCoordinator::imageRetentionPriority(
    ActiveNavigationThumbnailRetentionClass retentionClass)
{
    switch (retentionClass) {
    case ActiveNavigationThumbnailRetentionClass::Visible:
        return ThumbnailImageRetentionPriority::Visible;
    case ActiveNavigationThumbnailRetentionClass::Nearby:
        return ThumbnailImageRetentionPriority::Nearby;
    case ActiveNavigationThumbnailRetentionClass::Background:
        return ThumbnailImageRetentionPriority::Background;
    }
    return ThumbnailImageRetentionPriority::Background;
}

void ActiveNavigationThumbnailWorkCoordinator::applyEffects(
    std::vector<ActiveNavigationThumbnailScheduleEffect> effects)
{
    for (ActiveNavigationThumbnailScheduleEffect& effect : effects) {
        std::visit([this](auto value) { applyEffect(std::move(value)); }, std::move(effect));
    }
}

void ActiveNavigationThumbnailWorkCoordinator::applyEffect(
    ActiveNavigationThumbnailCancelWorkEffect effect)
{
    m_executor.cancel(effect.workId);
}

void ActiveNavigationThumbnailWorkCoordinator::applyEffect(
    ActiveNavigationThumbnailStartWorkEffect effect)
{
    m_executor.start(std::move(effect.request));
}

void ActiveNavigationThumbnailWorkCoordinator::applyEffect(
    ActiveNavigationThumbnailApplyPendingEffect effect)
{
    m_rowPort.applyPending(effect.sourceKey);
}

void ActiveNavigationThumbnailWorkCoordinator::applyEffect(
    ActiveNavigationThumbnailApplyUnsupportedEffect effect)
{
    m_rowPort.applyUnsupported(effect.sourceKey);
}

void ActiveNavigationThumbnailWorkCoordinator::applyEffect(
    ActiveNavigationThumbnailUpdateRetentionEffect effect)
{
    m_rowPort.updateRetentionPriority(
        effect.sourceKey, imageRetentionPriority(effect.retentionClass));
}

void ActiveNavigationThumbnailWorkCoordinator::applyEffect(
    ActiveNavigationThumbnailAcceptCompletionEffect effect)
{
    publishCompletion(effect);
}

void ActiveNavigationThumbnailWorkCoordinator::applyEffect(
    ActiveNavigationThumbnailScheduleContinuationEffect effect)
{
    QMetaObject::invokeMethod(
        this,
        [this, admissionEpoch = effect.admissionEpoch]() {
            applyEffects(m_scheduler.continueAdmission(admissionEpoch));
        },
        Qt::QueuedConnection);
}

void ActiveNavigationThumbnailWorkCoordinator::publishCompletion(
    const ActiveNavigationThumbnailAcceptCompletionEffect& effect)
{
    const auto& completion = effect.completion;
    if (completion.result.kind == ActiveNavigationThumbnailWorkResultKind::Ready) {
        if (completion.workKind == ActiveNavigationThumbnailWorkKind::Background
            && m_rowPort.hasUsableReadyImage(completion.sourceKey)) {
            return;
        }
        if (!m_rowPort.installReadyImage(completion.sourceKey, completion.result.image,
                imageRetentionPriority(effect.retentionClass),
                completion.workKind == ActiveNavigationThumbnailWorkKind::Background)) {
            reportFailureDiagnostic(completion.workId, completion.sourceKey, completion.workKind,
                completion.bucket, ActiveNavigationThumbnailFailureKind::ImageStoreInsertFailed,
                {});
            if (completion.workKind != ActiveNavigationThumbnailWorkKind::Background
                && !m_rowPort.hasUsableReadyImage(completion.sourceKey)) {
                m_rowPort.applyFailed(completion.sourceKey);
            }
        }
        return;
    }
    reportFailureDiagnostic(completion.workId, completion.sourceKey, completion.workKind,
        completion.bucket, completion.result.failureKind, completion.result.errorString);
    if (completion.workKind != ActiveNavigationThumbnailWorkKind::Background
        && !m_rowPort.hasUsableReadyImage(completion.sourceKey)) {
        m_rowPort.applyFailed(completion.sourceKey);
    }
}

void ActiveNavigationThumbnailWorkCoordinator::reportFailureDiagnostic(
    ActiveNavigationThumbnailWorkId workId, const ThumbnailSourceRevisionKey& sourceKey,
    ActiveNavigationThumbnailWorkKind workKind, ActiveNavigationThumbnailDemandBucket bucket,
    ActiveNavigationThumbnailFailureKind failureKind, const QString& errorString)
{
    const QString resolvedErrorString
        = errorString.isEmpty() ? fallbackThumbnailFailureError(failureKind) : errorString;
    const ActiveNavigationThumbnailFailureDiagnostic diagnostic {
        workId,
        sourceKey,
        workKind,
        bucket,
        failureKind,
        resolvedErrorString,
    };
    qCDebug(kiriviewThumbnailLog) << "Thumbnail failure diagnostic" << workId.value << "kind"
                                  << static_cast<int>(workKind) << "number"
                                  << sourceKey.row.rowNumber << "url" << sourceKey.sourceUrl
                                  << "bucket" << static_cast<int>(bucket) << "failure"
                                  << static_cast<int>(failureKind) << "error"
                                  << resolvedErrorString;
    if (m_failureDiagnosticCallback) {
        m_failureDiagnosticCallback(diagnostic);
    }
}
}
