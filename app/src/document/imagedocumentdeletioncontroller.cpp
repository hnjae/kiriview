// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentdeletioncontroller.h"

#include "async/imagecallback.h"
#include "imagedocumentstate.h"
#include "localization/imageerrortext.h"
#include "location/imagedocumentlocation.h"

#include <utility>

namespace {
QString genericFileDeletionErrorMessage()
{
    return kiriview::imageErrorText(kiriview::ImageErrorTextId::DeleteFile);
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
    static_cast<void>(m_deletionState.cancelFileDeletion());
    m_fallbackController.cancel();
    m_fileDeletionJob.cancel();
    m_publishedFileDeletionOperationId = 0;
}

bool ImageDocumentDeletionController::inProgress() const { return m_deletionState.inProgress(); }

void ImageDocumentDeletionController::deleteDisplayedFile(FileDeletionMode mode)
{
    if (m_deletionState.inProgress() || !documentReadyForFileDeletion(m_state)) {
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
    m_fileDeletionJob.cancel();
    if (!m_deletionState.acceptsFileDeletion(operation.operationId)) {
        return;
    }
    publishFileDeletionStarted(operation.operationId);
    if (!m_deletionState.acceptsFileDeletion(operation.operationId)) {
        return;
    }

    ImageIoJob startedJob
        = m_fileDeletionProvider(m_parent, FileDeletionRequest { removalPlan.targetUrl, mode },
            [this, operationId = operation.operationId, fallbackPlan = removalPlan.fallbackPlan](
                FileDeletionResult result, const KioOperationFailure& failure) {
                finishFileDeletion(operationId, fallbackPlan, result, failure);
            });
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
    const ImageDocumentDeletionFileOperationClaim operation
        = m_deletionState.claimFileDeletion(operationId);
    if (!operation.accepted) {
        return;
    }

    switch (fileDeletionCompletionAction(result)) {
    case FileDeletionCompletionAction::ClearDeletedTargetAndOpenFallback:
        reportRuntimePlan(imageDocumentClearDeletedImagePlan());
        if (!m_deletionState.acceptsClaimedFileDeletion(operationId)) {
            return;
        }
        m_fallbackController.open(fallbackPlan);
        break;
    case FileDeletionCompletionAction::Ignore:
        break;
    case FileDeletionCompletionAction::ReportFailure:
        reportFailure(failure);
        break;
    }

    if (!m_deletionState.acceptsClaimedFileDeletion(operationId)) {
        return;
    }
    if (m_deletionState.settleClaimedFileDeletion(operationId)) {
        publishFileDeletionSettled(operationId);
    }
}

void ImageDocumentDeletionController::publishFileDeletionStarted(quint64 operationId)
{
    const bool inProgressWasPublished = m_publishedFileDeletionOperationId != 0;
    m_publishedFileDeletionOperationId = operationId;
    if (!inProgressWasPublished) {
        invokeIfSet(m_callbacks.inProgressChanged);
    }
}

void ImageDocumentDeletionController::publishFileDeletionSettled(quint64 operationId)
{
    if (m_publishedFileDeletionOperationId != operationId) {
        return;
    }

    m_publishedFileDeletionOperationId = 0;
    invokeIfSet(m_callbacks.inProgressChanged);
}

void ImageDocumentDeletionController::cancel()
{
    const quint64 publishedOperationId = m_publishedFileDeletionOperationId;
    const bool inProgressChanged = m_deletionState.cancelFileDeletion();
    m_fileDeletionJob.cancel();
    m_fallbackController.cancel();
    if (inProgressChanged && publishedOperationId != 0
        && m_publishedFileDeletionOperationId == publishedOperationId) {
        publishFileDeletionSettled(publishedOperationId);
    }
}

void ImageDocumentDeletionController::reportRuntimePlan(ImageDocumentRuntimePlan plan)
{
    invokeIfSet(m_callbacks.runtimePlan, std::move(plan));
}

void ImageDocumentDeletionController::reportFailure(const KioOperationFailure& failure)
{
    const QString message
        = failure.userMessage.isEmpty() ? genericFileDeletionErrorMessage() : failure.userMessage;
    invokeIfSet(m_callbacks.failed, message);
}
}
