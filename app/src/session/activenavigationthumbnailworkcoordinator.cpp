// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailworkcoordinator.h"

#include "diagnostics/diagnosticlogprojection.h"
#include "session/thumbnaillogging.h"

#include <QDebug>
#include <QMetaObject>
#include <utility>

namespace {
constexpr std::size_t foregroundThumbnailCapacity = 2;

QString fallbackThumbnailFailureError(kiriview::ActiveNavigationThumbnailFailureKind failureKind)
{
    switch (failureKind) {
    case kiriview::ActiveNavigationThumbnailFailureKind::CacheLookupProviderUnavailable:
        return QStringLiteral("Thumbnail cache lookup provider is unavailable.");
    case kiriview::ActiveNavigationThumbnailFailureKind::CacheLookupInvalid:
        return QStringLiteral("Thumbnail cache lookup returned an invalid cache entry.");
    case kiriview::ActiveNavigationThumbnailFailureKind::CacheLookupFailed:
        return QStringLiteral("Thumbnail cache lookup failed.");
    case kiriview::ActiveNavigationThumbnailFailureKind::CacheInstallFailed:
        return QStringLiteral("Thumbnail cache installation failed.");
    case kiriview::ActiveNavigationThumbnailFailureKind::GenerationFailed:
        return QStringLiteral("Thumbnail generation failed.");
    case kiriview::ActiveNavigationThumbnailFailureKind::VideoExtractionInvalidRequest:
        return QStringLiteral("Video thumbnail extraction received an invalid request.");
    case kiriview::ActiveNavigationThumbnailFailureKind::VideoSourceUnavailable:
        return QStringLiteral("The video thumbnail source is unavailable.");
    case kiriview::ActiveNavigationThumbnailFailureKind::VideoUnsupportedMedia:
        return QStringLiteral("The video source is unsupported for thumbnail extraction.");
    case kiriview::ActiveNavigationThumbnailFailureKind::VideoBackendFailure:
        return QStringLiteral("The video thumbnail backend failed.");
    case kiriview::ActiveNavigationThumbnailFailureKind::VideoExtractionTimedOut:
        return QStringLiteral("Video thumbnail extraction timed out.");
    case kiriview::ActiveNavigationThumbnailFailureKind::VideoNoRepresentativeImage:
        return QStringLiteral("No representative video thumbnail is available.");
    case kiriview::ActiveNavigationThumbnailFailureKind::ResourceLimitExceeded:
        return QStringLiteral("Thumbnail generation exceeded the application resource limit.");
    case kiriview::ActiveNavigationThumbnailFailureKind::ImageStoreInsertFailed:
        return QStringLiteral("Thumbnail image store insertion failed.");
    case kiriview::ActiveNavigationThumbnailFailureKind::GenerationProviderUnavailable:
        return QStringLiteral("Thumbnail generation provider is unavailable.");
    }
    return QStringLiteral("Thumbnail work failed.");
}

const char* thumbnailFailureCategory(kiriview::ActiveNavigationThumbnailFailureKind failureKind)
{
    using FailureKind = kiriview::ActiveNavigationThumbnailFailureKind;

    switch (failureKind) {
    case FailureKind::CacheLookupProviderUnavailable:
        return "cache-lookup-provider-unavailable";
    case FailureKind::CacheLookupInvalid:
        return "cache-lookup-invalid";
    case FailureKind::CacheLookupFailed:
        return "cache-lookup-failed";
    case FailureKind::CacheInstallFailed:
        return "cache-install-failed";
    case FailureKind::GenerationFailed:
        return "generation-failed";
    case FailureKind::VideoExtractionInvalidRequest:
        return "video-extraction-invalid-request";
    case FailureKind::VideoSourceUnavailable:
        return "video-source-unavailable";
    case FailureKind::VideoUnsupportedMedia:
        return "video-unsupported-media";
    case FailureKind::VideoBackendFailure:
        return "video-backend-failure";
    case FailureKind::VideoExtractionTimedOut:
        return "video-extraction-timed-out";
    case FailureKind::VideoNoRepresentativeImage:
        return "video-no-representative-image";
    case FailureKind::ResourceLimitExceeded:
        return "resource-limit-exceeded";
    case FailureKind::ImageStoreInsertFailed:
        return "image-store-insert-failed";
    case FailureKind::GenerationProviderUnavailable:
        return "generation-provider-unavailable";
    }

    return "unknown";
}

void appendScheduleEffects(
    std::vector<kiriview::ActiveNavigationThumbnailScheduleEffect>& destination,
    std::vector<kiriview::ActiveNavigationThumbnailScheduleEffect> source)
{
    for (auto& effect : source) {
        destination.push_back(std::move(effect));
    }
}
}

namespace kiriview {
ActiveNavigationThumbnailWorkCoordinator::ActiveNavigationThumbnailWorkCoordinator(QObject* owner,
    ActiveNavigationThumbnailRowPort& rowPort, ThumbnailCacheLookupProvider lookupProvider,
    ThumbnailGenerationProvider generationProvider, ThumbnailSourceAdapter sourceAdapter,
    ActiveNavigationThumbnailFailureDiagnosticCallback failureDiagnosticCallback)
    : m_rowPort(rowPort)
    , m_scheduler(std::move(sourceAdapter), foregroundThumbnailCapacity)
    , m_executor(owner, std::move(lookupProvider), std::move(generationProvider),
          [this](ActiveNavigationThumbnailWorkCompletion completion) {
              applyEffects(m_scheduler.acceptCompletion(std::move(completion)));
          })
    , m_failureDiagnosticCallback(std::move(failureDiagnosticCallback))
{
    m_rowPort.subscribeToResidencyReconciliation(this, [this]() { reconcileResidencyChange(); });
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
    ActiveNavigationThumbnailResidencyChange residencyChange = m_rowPort.takeResidencyChange();
    std::vector<ActiveNavigationThumbnailScheduleEffect> effects
        = m_scheduler.reconcileImageResidency(residencyChange.losses, false);
    auto refreshed = m_scheduler.refreshRows(std::move(snapshot));
    if (!refreshed.has_value()) {
        if (residencyChange.admissionOpportunity) {
            appendScheduleEffects(effects, m_scheduler.reconcileImageResidency({}, true));
        }
        applyEffects(std::move(effects));
        return false;
    }
    appendScheduleEffects(effects, std::move(*refreshed));
    if (residencyChange.admissionOpportunity) {
        appendScheduleEffects(effects, m_scheduler.reconcileImageResidency({}, true));
    }
    applyEffects(std::move(effects));
    return true;
}

void ActiveNavigationThumbnailWorkCoordinator::invalidateRows()
{
    applyEffects(m_scheduler.invalidate());
}

void ActiveNavigationThumbnailWorkCoordinator::setCurrentNumber(int currentNumber)
{
    ActiveNavigationThumbnailResidencyChange residencyChange = m_rowPort.takeResidencyChange();
    auto effects = m_scheduler.reconcileImageResidency(residencyChange.losses, false);
    appendScheduleEffects(effects, m_scheduler.setCurrentNumber(currentNumber));
    if (residencyChange.admissionOpportunity) {
        appendScheduleEffects(effects, m_scheduler.reconcileImageResidency({}, true));
    }
    applyEffects(std::move(effects));
}

bool ActiveNavigationThumbnailWorkCoordinator::replaceDemandSnapshot(
    const ActiveNavigationThumbnailDemandSnapshot& snapshot)
{
    ActiveNavigationThumbnailResidencyChange residencyChange = m_rowPort.takeResidencyChange();
    std::vector<ActiveNavigationThumbnailScheduleEffect> effects
        = m_scheduler.reconcileImageResidency(residencyChange.losses, false);
    auto replaced = m_scheduler.replaceDemandSnapshot(snapshot);
    if (!replaced.has_value()) {
        if (residencyChange.admissionOpportunity) {
            appendScheduleEffects(effects, m_scheduler.reconcileImageResidency({}, true));
        }
        applyEffects(std::move(effects));
        return false;
    }
    appendScheduleEffects(effects, std::move(*replaced));
    if (residencyChange.admissionOpportunity) {
        appendScheduleEffects(effects, m_scheduler.reconcileImageResidency({}, true));
    }
    applyEffects(std::move(effects));
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
        std::visit([this](auto&& value) { applyEffect(std::forward<decltype(value)>(value)); },
            std::move(effect));
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
    const ActiveNavigationThumbnailApplyPendingEffect& effect)
{
    m_rowPort.applyPending(effect.sourceKey);
}

void ActiveNavigationThumbnailWorkCoordinator::applyEffect(
    const ActiveNavigationThumbnailApplyUnsupportedEffect& effect)
{
    m_rowPort.applyUnsupported(effect.sourceKey);
}

void ActiveNavigationThumbnailWorkCoordinator::applyEffect(
    const ActiveNavigationThumbnailUpdateRetentionEffect& effect)
{
    m_rowPort.updateRetentionPriority(
        effect.sourceKey, imageRetentionPriority(effect.retentionClass));
}

void ActiveNavigationThumbnailWorkCoordinator::applyEffect(
    const ActiveNavigationThumbnailAcceptCompletionEffect& effect)
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
    if (const auto* ready
        = std::get_if<ActiveNavigationThumbnailReadyWorkResult>(&completion.result)) {
        if (ready->diagnostic.has_value()) {
            ActiveNavigationThumbnailFailureKind failureKind
                = ActiveNavigationThumbnailFailureKind::CacheInstallFailed;
            switch (ready->diagnostic->kind) {
            case ActiveNavigationThumbnailDiagnosticKind::CacheInstallFailed:
                failureKind = ActiveNavigationThumbnailFailureKind::CacheInstallFailed;
                break;
            }
            reportFailureDiagnostic(completion.workId, completion.sourceKey, completion.workKind,
                completion.bucket, failureKind, ready->diagnostic->errorString);
        }
        if (completion.workKind == ActiveNavigationThumbnailWorkKind::Background
            && m_rowPort.hasUsableReadyImage(completion.sourceKey)) {
            return;
        }
        if (!m_rowPort.installReadyImage(completion.sourceKey, ready->image,
                imageRetentionPriority(effect.retentionClass),
                completion.workKind == ActiveNavigationThumbnailWorkKind::Background)) {
            reportFailureDiagnostic(completion.workId, completion.sourceKey, completion.workKind,
                completion.bucket, ActiveNavigationThumbnailFailureKind::ImageStoreInsertFailed,
                {});
            if (completion.workKind != ActiveNavigationThumbnailWorkKind::Background
                && !m_rowPort.hasUsableReadyImage(completion.sourceKey)) {
                m_rowPort.applyFailed(completion.sourceKey);
            }
            applyEffects(m_scheduler.reconcileImageResidency({ completion.sourceKey }, false));
        }
        return;
    }
    const auto& failed = std::get<ActiveNavigationThumbnailFailedWorkResult>(completion.result);
    reportFailureDiagnostic(completion.workId, completion.sourceKey, completion.workKind,
        completion.bucket, failed.failureKind, failed.errorString);
    if (completion.workKind != ActiveNavigationThumbnailWorkKind::Background
        && !m_rowPort.hasUsableReadyImage(completion.sourceKey)) {
        m_rowPort.applyFailed(completion.sourceKey);
    }
}

void ActiveNavigationThumbnailWorkCoordinator::reconcileResidencyChange()
{
    ActiveNavigationThumbnailResidencyChange change = m_rowPort.takeResidencyChange();
    if (change.empty()) {
        return;
    }
    applyEffects(m_scheduler.reconcileImageResidency(change.losses, change.admissionOpportunity));
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
                                  << sourceKey.row.rowNumber << "url"
                                  << kiriview::diagnosticSourceReference(sourceKey.sourceUrl)
                                  << "bucket" << static_cast<int>(bucket) << "failure"
                                  << thumbnailFailureCategory(failureKind) << "error"
                                  << kiriview::diagnosticDetailReference(resolvedErrorString);
    if (m_failureDiagnosticCallback) {
        m_failureDiagnosticCallback(diagnostic);
    }
}
}
