// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageopenworkflow.h"

#include "location/imagedocumentlocation.h"

#include <iterator>
#include <utility>

namespace {
using namespace kiriview;

bool shouldResetRightToLeftReading(
    const ImageDocumentSourceLoadSnapshot& snapshot, const ImageDocumentSourceLoadRequest& request)
{
    return snapshot.rightToLeftReadingEnabled && request.containerNavigationUrl().isEmpty()
        && !(snapshot.displayedOpenedCollectionScope.isComicBook()
            && openedCollectionScopeContainsUrl(
                snapshot.displayedOpenedCollectionScope, request.sourceUrl()));
}

void appendClearImage(ImageDocumentRuntimePlan& plan)
{
    ImageDocumentRuntimePlan clearPlan = imageDocumentClearImagePlan();
    plan.insert(plan.end(), std::make_move_iterator(clearPlan.begin()),
        std::make_move_iterator(clearPlan.end()));
}

ImageOpenApplicationPlan openedCollectionVideoPlan(
    const ImageLoadSession& session, bool unsupported)
{
    ImageOpenApplicationPlan plan;
    plan.stateDelta.sourceUrl = session.imageUrl();
    plan.stateDelta.sourceKind = session.kind();
    plan.stateDelta.displayedLocation = session.location();
    plan.stateDelta.containerNavigationUrl = containerNavigationUrlForLocation(session.location());
    plan.stateDelta.loading = false;
    plan.stateDelta.status = ImageDocumentStatus::Ready;
    plan.stateDelta.errorString = QString();
    plan.stateDelta.unsupportedOpenedCollectionVideo = unsupported;
    plan.stateDelta.embeddedMetadata = EmbeddedMetadata {};
    plan.stateDelta.clearLoadingContainerNavigationUrl = true;
    plan.runtimePlan.emplace_back(ClearSecondaryPageOperation {});
    plan.runtimePlan.emplace_back(UpdatePageNavigationOperation {});
    return plan;
}

ImageOpenApplicationPlan successfulImageLoadPlan(ImageOpenSuccessfulImageLoadSnapshot snapshot,
    const ImageLoadSession& session, std::optional<EmbeddedMetadata> metadata)
{
    ImageOpenApplicationPlan plan;
    plan.stateDelta.sourceUrl = session.imageUrl();
    plan.stateDelta.displayedLocation = session.location();
    plan.stateDelta.containerNavigationUrl = snapshot.hasRequestContainerNavigationTarget
        ? session.containerNavigationUrl()
        : containerNavigationUrlForLocation(session.location());
    plan.stateDelta.loading = false;
    plan.stateDelta.status = ImageDocumentStatus::Ready;
    plan.stateDelta.errorString = QString();
    plan.stateDelta.unsupportedOpenedCollectionVideo = false;
    plan.stateDelta.embeddedMetadata = std::move(metadata);
    plan.stateDelta.clearLoadingContainerNavigationUrl = true;
    plan.runtimePlan.emplace_back(UpdatePageNavigationOperation {});
    plan.runtimePlan.emplace_back(ScheduleAdjacentImagePredecodeOperation {});
    return plan;
}
}

namespace kiriview::ImageOpenWorkflow {
ImageDocumentRuntimePlan sourceLoadPlan(
    const ImageDocumentSourceLoadSnapshot& snapshot, const ImageDocumentSourceLoadRequest& request)
{
    ImageDocumentRuntimePlan plan;
    plan.emplace_back(CancelFileDeletionOperation {});
    const bool resetReading = shouldResetRightToLeftReading(snapshot, request);

    if (snapshot.currentSourceUrl == request.sourceUrl()) {
        if (resetReading) {
            plan.emplace_back(ResetRightToLeftReadingOperation {});
            plan.emplace_back(NotifyRightToLeftReadingChangedOperation {});
        }
        plan.emplace_back(ClearLoadingContainerNavigationUrlOperation {});
        if (!request.containerNavigationUrl().isEmpty()) {
            plan.emplace_back(
                SetContainerNavigationUrlOperation { request.containerNavigationUrl() });
        }
        return plan;
    }

    if (request.sameScopePageNavigation()) {
        if (resetReading) {
            plan.emplace_back(ResetRightToLeftReadingOperation {});
            plan.emplace_back(NotifyRightToLeftReadingChangedOperation {});
        }
        plan.emplace_back(ClearLoadingContainerNavigationUrlOperation {});
        plan.emplace_back(PrepareSourceLoadOperation { request });
        plan.emplace_back(SetSourceUrlOperation {
            ImageDocumentPageTarget { request.sourceUrl(), request.sourceKind() } });
        plan.emplace_back(BeginOpenOperation {});
        return plan;
    }

    plan.emplace_back(CancelAllNavigationOperation {});
    plan.emplace_back(CancelPredecodeOperation {});
    if (resetReading) {
        plan.emplace_back(ResetRightToLeftReadingOperation {});
    }
    plan.emplace_back(ClearSecondaryPageOperation {});
    plan.emplace_back(
        SetLoadingContainerNavigationUrlOperation { request.containerNavigationUrl() });
    plan.emplace_back(PrepareSourceLoadOperation { request });
    plan.emplace_back(SetSourceUrlOperation {
        ImageDocumentPageTarget { request.sourceUrl(), request.sourceKind() } });
    plan.emplace_back(BeginOpenOperation {});
    if (resetReading) {
        plan.emplace_back(NotifyRightToLeftReadingChangedOperation {});
    }
    return plan;
}

ImageOpenApplicationPlan beginSourceLoadPlan(ImageOpenBeginSourceLoadSnapshot snapshot)
{
    ImageOpenApplicationPlan plan;
    plan.stateDelta.unsupportedOpenedCollectionVideo = false;
    plan.stateDelta.embeddedMetadata = EmbeddedMetadata {};
    plan.stateDelta.errorString = QString();
    if (!snapshot.hasImage && !snapshot.hasLoadingContainerNavigationTarget) {
        plan.stateDelta.containerNavigationUrl = QUrl();
    }
    plan.stateDelta.loading = true;
    plan.stateDelta.status = ImageDocumentStatus::Loading;
    if (snapshot.hasImage) {
        plan.runtimePlan.emplace_back(ClearPresentationImageOperation {});
    } else {
        appendClearImage(plan.runtimePlan);
    }
    return plan;
}

ImageOpenApplicationPlan finishEmptySourceLoadPlan()
{
    ImageOpenApplicationPlan plan;
    appendClearImage(plan.runtimePlan);
    plan.stateDelta.unsupportedOpenedCollectionVideo = false;
    plan.stateDelta.embeddedMetadata = EmbeddedMetadata {};
    plan.stateDelta.errorString = QString();
    plan.stateDelta.loading = false;
    plan.stateDelta.containerNavigationUrl = QUrl();
    plan.stateDelta.status = ImageDocumentStatus::Null;
    plan.stateDelta.clearLoadingContainerNavigationUrl = true;
    return plan;
}

ImageOpenApplicationPlan resolveSourceImagePlan(const ImageLoadSession& session)
{
    ImageOpenApplicationPlan plan;
    plan.stateDelta.sourceUrl = session.imageUrl();
    plan.stateDelta.sourceKind = session.kind();
    return plan;
}

ImageOpenApplicationPlan finishUnsupportedOpenedCollectionVideoLoadPlan(
    const ImageLoadSession& session)
{
    return openedCollectionVideoPlan(session, true);
}

ImageOpenApplicationPlan finishPlayableOpenedCollectionVideoLoadPlan(
    const ImageLoadSession& session)
{
    return openedCollectionVideoPlan(session, false);
}

ImageOpenApplicationPlan finishSuccessfulImageLoadPlan(
    ImageOpenSuccessfulImageLoadSnapshot snapshot, const ImageLoadSession& session)
{
    return successfulImageLoadPlan(snapshot, session, std::nullopt);
}

ImageOpenApplicationPlan finishSuccessfulImageLoadPlan(
    ImageOpenSuccessfulImageLoadSnapshot snapshot, const ImageLoadSession& session,
    EmbeddedMetadata metadata)
{
    return successfulImageLoadPlan(snapshot, session, std::move(metadata));
}

ImageOpenApplicationPlan finishLoadWithErrorPlan(
    const ImageLoadSession& session, ImageLoadFailure failure)
{
    ImageOpenApplicationPlan plan;
    plan.stateDelta.loading = false;
    plan.stateDelta.errorString = failure.userMessage;
    plan.stateDelta.loadFailure = std::move(failure);
    plan.stateDelta.unsupportedOpenedCollectionVideo = false;
    plan.stateDelta.status = ImageDocumentStatus::Error;
    plan.stateDelta.clearLoadingContainerNavigationUrl = true;
    if (session.hasContainerNavigationTarget()) {
        appendClearImage(plan.runtimePlan);
        plan.stateDelta.containerNavigationUrl = session.containerNavigationUrl();
        plan.stateDelta.sourceUrl = session.containerNavigationUrl();
    } else {
        plan.stateDelta.containerNavigationUrl = QUrl();
        plan.stateDelta.embeddedMetadata = EmbeddedMetadata {};
    }
    return plan;
}

ImageOpenApplicationPlan finishContainerNavigationLoadWithErrorPlan(
    const QUrl& containerUrl, const QString& errorString)
{
    ImageOpenApplicationPlan plan;
    appendClearImage(plan.runtimePlan);
    plan.stateDelta.sourceUrl = containerUrl;
    plan.stateDelta.containerNavigationUrl = containerUrl;
    plan.stateDelta.loading = false;
    plan.stateDelta.status = ImageDocumentStatus::Error;
    plan.stateDelta.errorString = errorString;
    plan.stateDelta.unsupportedOpenedCollectionVideo = false;
    plan.stateDelta.clearLoadingContainerNavigationUrl = true;
    return plan;
}
}
