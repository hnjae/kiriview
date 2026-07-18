// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageopenworkflow.h"

#include "bridge/imageopenworkflowconversion.h"
#include "location/imagedocumentlocation.h"

#include <iterator>
#include <optional>
#include <utility>

namespace {
kiriview::Bridge::ImageDocumentSourceLoadKind sourceLoadKind(
    const kiriview::ImageDocumentSourceLoadSnapshot& snapshot,
    const kiriview::ImageDocumentSourceLoadRequest& request)
{
    if (snapshot.currentSourceUrl == request.sourceUrl()) {
        return kiriview::Bridge::ImageDocumentSourceLoadKind::CurrentSource;
    }

    if (request.sameScopePageNavigation()) {
        return kiriview::Bridge::ImageDocumentSourceLoadKind::SameScopeImageNavigation;
    }

    return kiriview::Bridge::ImageDocumentSourceLoadKind::ReplacementSource;
}

bool sourceWithinDisplayedComicBookArchive(
    const kiriview::ImageDocumentSourceLoadSnapshot& snapshot,
    const kiriview::ImageDocumentSourceLoadRequest& request)
{
    return snapshot.displayedOpenedCollectionScope.isComicBook()
        && kiriview::openedCollectionScopeContainsUrl(
            snapshot.displayedOpenedCollectionScope, request.sourceUrl());
}

kiriview::Bridge::ImageDocumentSourceLoadPolicyInput sourceLoadPolicyInput(
    const kiriview::ImageDocumentSourceLoadSnapshot& snapshot,
    const kiriview::ImageDocumentSourceLoadRequest& request)
{
    return kiriview::Bridge::ImageDocumentSourceLoadPolicyInput {
        sourceLoadKind(snapshot, request),
        request.preserveTwoPageSpreadTransition(),
        snapshot.rightToLeftReadingEnabled,
        sourceWithinDisplayedComicBookArchive(snapshot, request),
        !request.containerNavigationUrl().isEmpty(),
    };
}

void appendSourceLoadRuntimeOperation(kiriview::ImageDocumentRuntimePlan& runtimePlan,
    kiriview::RustImageDocumentSourceLoadOperation operation,
    const kiriview::ImageDocumentSourceLoadRequest& request)
{
    using Operation = kiriview::RustImageDocumentSourceLoadOperation;

    switch (operation) {
    case Operation::CancelFileDeletion:
        runtimePlan.push_back(kiriview::CancelFileDeletionOperation {});
        return;
    case Operation::CancelAllNavigation:
        runtimePlan.push_back(kiriview::CancelAllNavigationOperation {});
        return;
    case Operation::CancelPredecode:
        runtimePlan.push_back(kiriview::CancelPredecodeOperation {});
        return;
    case Operation::FinishSpreadTransition:
        runtimePlan.push_back(kiriview::FinishSpreadTransitionOperation {});
        return;
    case Operation::ResetRightToLeftReading:
        runtimePlan.push_back(kiriview::ResetRightToLeftReadingOperation {});
        return;
    case Operation::NotifyRightToLeftReadingChanged:
        runtimePlan.push_back(kiriview::NotifyRightToLeftReadingChangedOperation {});
        return;
    case Operation::ClearSecondaryPage:
        runtimePlan.push_back(kiriview::ClearSecondaryPageOperation {});
        return;
    case Operation::BeginSameScopeImageNavigationPresentation:
        runtimePlan.push_back(kiriview::BeginSameScopeImageNavigationPresentationOperation {});
        return;
    case Operation::ClearLoadingContainerNavigationUrl:
        runtimePlan.push_back(kiriview::ClearLoadingContainerNavigationUrlOperation {});
        return;
    case Operation::SetLoadingContainerNavigationUrlToRequested:
        runtimePlan.push_back(kiriview::SetLoadingContainerNavigationUrlOperation {
            request.containerNavigationUrl(),
        });
        return;
    case Operation::SetContainerNavigationUrlToRequested:
        runtimePlan.push_back(kiriview::SetContainerNavigationUrlOperation {
            request.containerNavigationUrl(),
        });
        return;
    case Operation::PrepareSourceLoad:
        runtimePlan.push_back(kiriview::PrepareSourceLoadOperation { request });
        return;
    case Operation::SetSourceUrlToRequested:
        runtimePlan.push_back(kiriview::SetSourceUrlOperation {
            kiriview::ImageDocumentPageTarget { request.sourceUrl(), request.sourceKind() },
        });
        return;
    case Operation::BeginOpen:
        runtimePlan.push_back(kiriview::BeginOpenOperation {});
        return;
    }
}

kiriview::ImageDocumentRuntimePlan sourceLoadRuntimePlan(
    const kiriview::RustImageDocumentSourceLoadPlan& sourceLoadPlan,
    const kiriview::ImageDocumentSourceLoadRequest& request)
{
    kiriview::ImageDocumentRuntimePlan runtimePlan;
    runtimePlan.reserve(sourceLoadPlan.operations.size());
    for (kiriview::RustImageDocumentSourceLoadOperation operation : sourceLoadPlan.operations) {
        appendSourceLoadRuntimeOperation(runtimePlan, operation, request);
    }
    return runtimePlan;
}

std::optional<bool> boolTarget(kiriview::RustImageOpenBoolTarget target)
{
    switch (target) {
    case kiriview::RustImageOpenBoolTarget::False:
        return false;
    case kiriview::RustImageOpenBoolTarget::True:
        return true;
    case kiriview::RustImageOpenBoolTarget::Unchanged:
        break;
    }

    return std::nullopt;
}

std::optional<kiriview::ImageDocumentStatus> documentStatus(
    kiriview::RustImageOpenStatusTarget target)
{
    switch (target) {
    case kiriview::RustImageOpenStatusTarget::Null:
        return kiriview::ImageDocumentStatus::Null;
    case kiriview::RustImageOpenStatusTarget::Loading:
        return kiriview::ImageDocumentStatus::Loading;
    case kiriview::RustImageOpenStatusTarget::Ready:
        return kiriview::ImageDocumentStatus::Ready;
    case kiriview::RustImageOpenStatusTarget::Error:
        return kiriview::ImageDocumentStatus::Error;
    case kiriview::RustImageOpenStatusTarget::Unchanged:
        break;
    }

    return std::nullopt;
}

std::optional<QUrl> urlTarget(kiriview::RustImageOpenUrlTarget target,
    const kiriview::ImageLoadSession* session, const std::optional<QUrl>& containerUrl)
{
    switch (target) {
    case kiriview::RustImageOpenUrlTarget::Empty:
        return QUrl();
    case kiriview::RustImageOpenUrlTarget::SessionImage:
        return session == nullptr ? std::nullopt : std::optional<QUrl>(session->imageUrl());
    case kiriview::RustImageOpenUrlTarget::SessionContainerNavigation:
        return session == nullptr ? std::nullopt
                                  : std::optional<QUrl>(session->containerNavigationUrl());
    case kiriview::RustImageOpenUrlTarget::DerivedContainerNavigation:
        return session == nullptr
            ? std::nullopt
            : std::optional<QUrl>(kiriview::containerNavigationUrlForLocation(session->location()));
    case kiriview::RustImageOpenUrlTarget::Container:
        return containerUrl;
    case kiriview::RustImageOpenUrlTarget::Unchanged:
        break;
    }

    return std::nullopt;
}

std::optional<kiriview::ImageDocumentPageKind> sourceKindTarget(
    kiriview::RustImageOpenSourceKindTarget target, const kiriview::ImageLoadSession* session)
{
    if (target == kiriview::RustImageOpenSourceKindTarget::Session && session != nullptr) {
        return session->kind();
    }
    return std::nullopt;
}

std::optional<kiriview::DisplayedImageLocation> displayedLocationTarget(
    kiriview::RustImageOpenDisplayedLocationTarget target,
    const kiriview::ImageLoadSession* session)
{
    if (target == kiriview::RustImageOpenDisplayedLocationTarget::Session && session != nullptr) {
        return session->location();
    }
    return std::nullopt;
}

std::optional<QString> errorStringTarget(
    kiriview::RustImageOpenErrorStringTarget target, const std::optional<QString>& errorString)
{
    switch (target) {
    case kiriview::RustImageOpenErrorStringTarget::Clear:
        return QString();
    case kiriview::RustImageOpenErrorStringTarget::Provided:
        return errorString;
    case kiriview::RustImageOpenErrorStringTarget::Unchanged:
        break;
    }
    return std::nullopt;
}

std::optional<kiriview::ImageLoadFailure> loadFailureTarget(
    kiriview::RustImageOpenErrorStringTarget target,
    const std::optional<kiriview::ImageLoadFailure>& loadFailure)
{
    return target == kiriview::RustImageOpenErrorStringTarget::Provided ? loadFailure
                                                                        : std::nullopt;
}

std::optional<kiriview::EmbeddedMetadata> embeddedMetadataTarget(
    bool clearEmbeddedMetadata, const std::optional<kiriview::EmbeddedMetadata>& embeddedMetadata)
{
    if (embeddedMetadata.has_value()) {
        return embeddedMetadata;
    }
    if (clearEmbeddedMetadata) {
        return kiriview::EmbeddedMetadata {};
    }
    return std::nullopt;
}

kiriview::ImageOpenResolvedStateDelta resolvedStateDelta(
    const kiriview::RustImageOpenStateDelta& delta, const kiriview::ImageLoadSession* session,
    const std::optional<QUrl>& containerUrl, const std::optional<QString>& errorString,
    const std::optional<kiriview::ImageLoadFailure>& loadFailure,
    const std::optional<kiriview::EmbeddedMetadata>& embeddedMetadata)
{
    return kiriview::ImageOpenResolvedStateDelta {
        urlTarget(delta.source_url, session, containerUrl),
        sourceKindTarget(delta.source_kind, session),
        displayedLocationTarget(delta.displayed_location, session),
        urlTarget(delta.container_navigation_url, session, containerUrl),
        boolTarget(delta.loading),
        documentStatus(delta.status),
        errorStringTarget(delta.error_string, errorString),
        loadFailureTarget(delta.error_string, loadFailure),
        boolTarget(delta.unsupported_opened_collection_video),
        embeddedMetadataTarget(delta.clear_embedded_metadata, embeddedMetadata),
        delta.clear_loading_container_navigation_url,
    };
}

void appendRuntimeOperationsForOpenEffect(kiriview::ImageDocumentRuntimePlan& plan,
    kiriview::RustImageOpenEffect effect, const std::optional<QUrl>& containerUrl)
{
    switch (effect) {
    case kiriview::RustImageOpenEffect::ClearImage: {
        kiriview::ImageDocumentRuntimePlan clearPlan = kiriview::imageDocumentClearImagePlan();
        plan.insert(plan.end(), std::make_move_iterator(clearPlan.begin()),
            std::make_move_iterator(clearPlan.end()));
        return;
    }
    case kiriview::RustImageOpenEffect::ClearLoadingPresentation: {
        kiriview::ImageDocumentRuntimePlan clearPlan
            = kiriview::imageDocumentClearLoadingPresentationPlan();
        plan.insert(plan.end(), std::make_move_iterator(clearPlan.begin()),
            std::make_move_iterator(clearPlan.end()));
        return;
    }
    case kiriview::RustImageOpenEffect::ResetZoom:
        plan.push_back(kiriview::ResetZoomOperation {});
        return;
    case kiriview::RustImageOpenEffect::UpdatePageNavigation:
        plan.push_back(kiriview::UpdatePageNavigationOperation {});
        return;
    case kiriview::RustImageOpenEffect::ScheduleAdjacentImagePredecode:
        plan.push_back(kiriview::ScheduleAdjacentImagePredecodeOperation {});
        return;
    case kiriview::RustImageOpenEffect::PrepareFailedContainer:
        if (containerUrl.has_value()) {
            plan.push_back(kiriview::PrepareFailedContainerOperation { *containerUrl });
        }
        return;
    case kiriview::RustImageOpenEffect::FinishSpreadTransition:
        plan.push_back(kiriview::FinishSpreadTransitionOperation {});
        return;
    case kiriview::RustImageOpenEffect::ClearSecondaryPage:
        plan.push_back(kiriview::ClearSecondaryPageOperation {});
        return;
    }
}

kiriview::ImageDocumentRuntimePlan resolvedRuntimePlan(
    const kiriview::RustImageOpenTransition& transition, const std::optional<QUrl>& containerUrl)
{
    kiriview::ImageDocumentRuntimePlan plan;
    plan.reserve(transition.effects.size());
    for (kiriview::RustImageOpenEffect effect : transition.effects) {
        appendRuntimeOperationsForOpenEffect(plan, effect, containerUrl);
    }
    return plan;
}

kiriview::ImageOpenApplicationPlan resolvedApplicationPlan(
    const kiriview::RustImageOpenTransition& transition,
    const kiriview::ImageLoadSession* session = nullptr,
    const std::optional<QUrl>& containerUrl = std::nullopt,
    const std::optional<QString>& errorString = std::nullopt,
    const std::optional<kiriview::ImageLoadFailure>& loadFailure = std::nullopt,
    const std::optional<kiriview::EmbeddedMetadata>& embeddedMetadata = std::nullopt)
{
    return kiriview::ImageOpenApplicationPlan {
        resolvedStateDelta(transition.state_delta, session, containerUrl, errorString, loadFailure,
            embeddedMetadata),
        resolvedRuntimePlan(transition, containerUrl),
    };
}

kiriview::ImageOpenApplicationPlan sourceLoadErrorApplicationPlan(
    const kiriview::RustImageOpenTransition& transition, const kiriview::ImageLoadSession& session,
    kiriview::ImageLoadFailure failure)
{
    const std::optional<QUrl> containerUrl = session.containerNavigationUrl();
    const std::optional<QString> errorString = failure.userMessage;
    const std::optional<kiriview::ImageLoadFailure> loadFailure = std::move(failure);
    return resolvedApplicationPlan(transition, &session, containerUrl, errorString, loadFailure);
}
}

namespace kiriview::ImageOpenWorkflow {
ImageDocumentRuntimePlan sourceLoadPlan(
    const ImageDocumentSourceLoadSnapshot& snapshot, const ImageDocumentSourceLoadRequest& request)
{
    const Bridge::ImageDocumentSourceLoadPolicyInput input
        = sourceLoadPolicyInput(snapshot, request);
    return sourceLoadRuntimePlan(
        rustImageDocumentSourceLoadPlan(rustImageDocumentSourceLoadPolicyInput(input)), request);
}

ImageOpenApplicationPlan beginSourceLoadPlan(ImageOpenBeginSourceLoadSnapshot snapshot)
{
    return resolvedApplicationPlan(rustImageOpenTransition(
        rustBeginSourceLoadEvent(snapshot.hasImage, snapshot.hasLoadingContainerNavigationTarget)));
}

ImageOpenApplicationPlan finishEmptySourceLoadPlan()
{
    return resolvedApplicationPlan(rustImageOpenTransition(
        rustImageOpenWorkflowEvent(RustImageOpenWorkflowEventKind::FinishEmptySourceLoad)));
}

ImageOpenApplicationPlan resolveSourceImagePlan(const ImageLoadSession& session)
{
    return resolvedApplicationPlan(rustImageOpenTransition(rustImageOpenWorkflowEvent(
                                       RustImageOpenWorkflowEventKind::ResolveSourceImage)),
        &session);
}

ImageOpenApplicationPlan finishUnsupportedOpenedCollectionVideoLoadPlan(
    const ImageLoadSession& session)
{
    return resolvedApplicationPlan(
        rustImageOpenTransition(rustImageOpenWorkflowEvent(
            RustImageOpenWorkflowEventKind::FinishUnsupportedOpenedCollectionVideoLoad)),
        &session);
}

ImageOpenApplicationPlan finishPlayableOpenedCollectionVideoLoadPlan(
    const ImageLoadSession& session)
{
    return resolvedApplicationPlan(
        rustImageOpenTransition(rustImageOpenWorkflowEvent(
            RustImageOpenWorkflowEventKind::FinishPlayableOpenedCollectionVideoLoad)),
        &session);
}

ImageOpenApplicationPlan finishSuccessfulImageLoadPlan(
    ImageOpenSuccessfulImageLoadSnapshot snapshot, const ImageLoadSession& session)
{
    return resolvedApplicationPlan(rustImageOpenTransition(rustSuccessfulImageLoadEvent(
                                       snapshot.hasRequestContainerNavigationTarget)),
        &session);
}

ImageOpenApplicationPlan finishSuccessfulImageLoadPlan(
    ImageOpenSuccessfulImageLoadSnapshot snapshot, const ImageLoadSession& session,
    EmbeddedMetadata metadata)
{
    return resolvedApplicationPlan(rustImageOpenTransition(rustSuccessfulImageLoadEvent(
                                       snapshot.hasRequestContainerNavigationTarget)),
        &session, std::nullopt, std::nullopt, std::nullopt, std::move(metadata));
}

ImageOpenApplicationPlan finishLoadWithErrorPlan(
    const ImageLoadSession& session, ImageLoadFailure failure)
{
    return sourceLoadErrorApplicationPlan(
        rustImageOpenTransition(rustSourceLoadErrorEvent(session.hasContainerNavigationTarget())),
        session, std::move(failure));
}

ImageOpenApplicationPlan finishContainerNavigationLoadWithErrorPlan(
    const QUrl& containerUrl, const QString& errorString)
{
    return resolvedApplicationPlan(
        rustImageOpenTransition(rustImageOpenWorkflowEvent(
            RustImageOpenWorkflowEventKind::FinishContainerNavigationLoadWithError)),
        nullptr, containerUrl, errorString);
}

ImageOpenApplicationPlan finishAnimationLoadWithErrorPlan(const QString& errorString)
{
    return resolvedApplicationPlan(
        rustImageOpenTransition(rustImageOpenWorkflowEvent(
            RustImageOpenWorkflowEventKind::FinishAnimationLoadWithError)),
        nullptr, std::nullopt, errorString);
}

ImageOpenApplicationPlan finishPresentationLoadWithErrorPlan(ImageLoadFailure failure)
{
    const std::optional<QString> errorString = failure.userMessage;
    const std::optional<ImageLoadFailure> loadFailure = std::move(failure);
    return resolvedApplicationPlan(
        rustImageOpenTransition(rustImageOpenWorkflowEvent(
            RustImageOpenWorkflowEventKind::FinishAnimationLoadWithError)),
        nullptr, std::nullopt, errorString, loadFailure);
}
}
