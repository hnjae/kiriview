// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentdeletioncontroller.h"

#include "async/imagecallback.h"
#include "imagedocumentstate.h"
#include "localization/imageerrortext.h"
#include "location/imagedocumentlocation.h"

#include <QPointer>
#include <optional>
#include <utility>

namespace {
QString fileDeletionErrorMessage(const kiriview::KioOperationFailure& failure)
{
    return failure.userMessage.isEmpty()
        ? kiriview::imageErrorText(kiriview::ImageErrorTextId::DeleteFile)
        : failure.userMessage;
}

bool documentReadyForFileDeletion(const kiriview::ImageDocumentState& state)
{
    return state.status() == kiriview::ImageDocumentStatus::Ready;
}

bool displayedOpenedCollectionVideoHasDeletionTarget(kiriview::ImageDocumentPageKind sourceKind,
    const kiriview::DisplayedImageLocation& displayedLocation)
{
    return sourceKind == kiriview::ImageDocumentPageKind::Video
        && kiriview::displayedLocationIsInsideOpenedCollectionScope(displayedLocation);
}
}

namespace kiriview {
ImageDocumentDeletionController::ImageDocumentDeletionController(QObject* parent,
    ImageDocumentState& state, HasDisplayedImageCallback hasDisplayedImage,
    ImageDocumentPageCandidateProvider candidateProvider, FileDeletionProvider fileDeletionProvider,
    Callbacks callbacks, std::function<ResolvedNavigationSource(const QUrl&)> resolveExternalSource)
    : m_parent(parent)
    , m_state(state)
    , m_hasDisplayedImage(std::move(hasDisplayedImage))
    , m_callbacks(std::move(callbacks))
    , m_fileDeletionProvider(fileDeletionProviderWithDefault(std::move(fileDeletionProvider)))
    , m_fallbackController(
          m_parent, std::move(candidateProvider),
          [this](ImageDocumentRuntimePlan plan) { reportRuntimePlan(std::move(plan)); },
          std::move(resolveExternalSource))
{
}

ImageDocumentDeletionController::~ImageDocumentDeletionController()
{
    m_callbackLifetime.reset();
    static_cast<void>(m_deletionState.cancelFileDeletion());
    m_fallbackController.cancel();
    m_fileDeletionJob.cancel();
    m_publishedFileDeletionOperationId = 0;
}

bool ImageDocumentDeletionController::inProgress() const { return m_deletionState.inProgress(); }

void ImageDocumentDeletionController::deleteDisplayedFile(FileDeletionMode mode)
{
    const std::weak_ptr<int> lifetime = m_callbackLifetime;
    const QPointer<QObject> owner = m_parent;
    if (owner == nullptr || m_deletionState.inProgress()
        || !documentReadyForFileDeletion(m_state)) {
        return;
    }

    const DisplayedImageLocation displayedLocation = m_state.displayedImageLocation();
    const quint64 presentationLifecycleRevision = m_state.presentationLifecycleRevision();
    const ImageDocumentPageKind sourceKind = m_state.sourceKind();
    const ImageRemovalPlan removalPlan = imageRemovalPlanForDisplayedLocation(displayedLocation);
    if (!removalPlan.hasTarget()) {
        return;
    }

    const bool hasDisplayedImage = m_hasDisplayedImage && m_hasDisplayedImage();
    if (lifetime.expired() || owner == nullptr) {
        return;
    }
    if (!hasDisplayedImage
        && !displayedOpenedCollectionVideoHasDeletionTarget(sourceKind, displayedLocation)) {
        return;
    }
    if (m_deletionState.inProgress() || !documentReadyForFileDeletion(m_state)
        || m_state.presentationLifecycleRevision() != presentationLifecycleRevision
        || m_state.displayedImageLocation() != displayedLocation) {
        return;
    }

    const ImageDocumentDeletionFileOperationStart operation = m_deletionState.startFileDeletion();
    if (!operation.accepted) {
        return;
    }

    m_fallbackController.cancel();
    if (lifetime.expired() || owner == nullptr) {
        return;
    }
    m_fileDeletionJob.cancel();
    if (lifetime.expired() || owner == nullptr) {
        return;
    }
    if (!m_deletionState.acceptsFileDeletion(operation.operationId)) {
        return;
    }
    publishFileDeletionStarted(operation.operationId);
    if (lifetime.expired() || !m_deletionState.acceptsFileDeletion(operation.operationId)) {
        return;
    }

    ImageIoJob startedJob
        = m_fileDeletionProvider(owner.data(), FileDeletionRequest { removalPlan.targetUrl, mode },
            [this, operationId = operation.operationId, fallbackPlan = removalPlan.fallbackPlan](
                FileDeletionResult result, const KioOperationFailure& failure) {
                finishFileDeletion(operationId, fallbackPlan, result, failure);
            });
    if (lifetime.expired() || owner == nullptr) {
        startedJob.cancel();
        return;
    }
    if (!m_deletionState.acceptsFileDeletion(operation.operationId)) {
        startedJob.cancel();
        return;
    }
    m_fileDeletionJob = std::move(startedJob);
}

void ImageDocumentDeletionController::finishFileDeletion(quint64 operationId,
    const ImageRemovalFallbackPlan& fallbackPlan, FileDeletionResult result,
    const KioOperationFailure& failure)
{
    const std::weak_ptr<int> lifetime = m_callbackLifetime;
    const QPointer<QObject> owner = m_parent;
    const ImageDocumentDeletionFileOperationClaim operation
        = m_deletionState.claimFileDeletion(operationId);
    if (!operation.accepted) {
        return;
    }

    std::optional<QString> failureMessage;
    switch (fileDeletionCompletionAction(result)) {
    case FileDeletionCompletionAction::ClearDeletedTargetAndOpenFallback:
        reportRuntimePlan(imageDocumentClearDeletedImagePlan());
        if (lifetime.expired() || owner == nullptr
            || !m_deletionState.acceptsClaimedFileDeletion(operationId)) {
            return;
        }
        m_fallbackController.open(fallbackPlan);
        if (lifetime.expired() || owner == nullptr) {
            return;
        }
        break;
    case FileDeletionCompletionAction::Ignore:
        break;
    case FileDeletionCompletionAction::ReportFailure:
        failureMessage = fileDeletionErrorMessage(failure);
        break;
    }

    if (!m_deletionState.acceptsClaimedFileDeletion(operationId)) {
        return;
    }
    if (!m_deletionState.settleClaimedFileDeletion(operationId)) {
        return;
    }

    const FailedCallback failed = m_callbacks.failed;
    publishFileDeletionSettled(operationId);
    if (!lifetime.expired() && failureMessage.has_value() && owner != nullptr) {
        invokeIfSet(failed, *failureMessage);
    }
}

void ImageDocumentDeletionController::publishFileDeletionStarted(quint64 operationId)
{
    const bool inProgressWasPublished = m_publishedFileDeletionOperationId != 0;
    m_publishedFileDeletionOperationId = operationId;
    if (!inProgressWasPublished) {
        InProgressChangedCallback callback = m_callbacks.inProgressChanged;
        invokeIfSet(callback);
    }
}

void ImageDocumentDeletionController::publishFileDeletionSettled(quint64 operationId)
{
    if (m_publishedFileDeletionOperationId != operationId) {
        return;
    }

    m_publishedFileDeletionOperationId = 0;
    InProgressChangedCallback callback = m_callbacks.inProgressChanged;
    invokeIfSet(callback);
}

void ImageDocumentDeletionController::cancel()
{
    const std::weak_ptr<int> lifetime = m_callbackLifetime;
    const quint64 publishedOperationId = m_publishedFileDeletionOperationId;
    const bool inProgressChanged = m_deletionState.cancelFileDeletion();
    m_fileDeletionJob.cancel();
    if (lifetime.expired()) {
        return;
    }
    m_fallbackController.cancel();
    if (lifetime.expired()) {
        return;
    }
    if (inProgressChanged && publishedOperationId != 0
        && m_publishedFileDeletionOperationId == publishedOperationId) {
        publishFileDeletionSettled(publishedOperationId);
    }
}

void ImageDocumentDeletionController::reportRuntimePlan(ImageDocumentRuntimePlan plan)
{
    RuntimePlanCallback callback = m_callbacks.runtimePlan;
    invokeIfSet(callback, std::move(plan));
}
}
