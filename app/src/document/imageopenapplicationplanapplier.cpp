// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageopenapplicationplanapplier.h"

#include "imagedocumentstate.h"
#include "location/imagedocumentlocation.h"

#include <utility>

namespace {
QString finalErrorString(
    const kiriview::ImageDocumentState& state, const kiriview::ImageOpenResolvedStateDelta& delta)
{
    if (delta.loadFailure.has_value()) {
        return delta.loadFailure->userMessage;
    }
    if (delta.errorString.has_value()) {
        return *delta.errorString;
    }
    return state.errorString();
}

QUrl finalSourceUrl(
    const kiriview::ImageDocumentState& state, const kiriview::ImageOpenResolvedStateDelta& delta)
{
    return delta.sourceUrl.value_or(state.sourceUrl());
}

kiriview::DisplayedImageLocation finalDisplayedLocation(
    const kiriview::ImageDocumentState& state, const kiriview::ImageOpenResolvedStateDelta& delta)
{
    return delta.displayedLocation.value_or(state.displayedImageLocation());
}

kiriview::ImageDocumentPageKind finalSourceKind(
    const kiriview::ImageDocumentState& state, const kiriview::ImageOpenResolvedStateDelta& delta)
{
    return delta.sourceKind.value_or(state.sourceKind());
}

bool finalUnsupportedOpenedCollectionVideo(
    const kiriview::ImageDocumentState& state, const kiriview::ImageOpenResolvedStateDelta& delta)
{
    return delta.unsupportedOpenedCollectionVideo.value_or(
        state.unsupportedOpenedCollectionVideo());
}

QUrl finalContainerNavigationUrl(
    const kiriview::ImageDocumentState& state, const kiriview::ImageOpenResolvedStateDelta& delta)
{
    return delta.containerNavigationUrl.value_or(state.containerNavigationUrl());
}

bool finalReadyContainerNavigationUrlIsValid(
    const kiriview::ImageDocumentState& state, const kiriview::ImageOpenResolvedStateDelta& delta)
{
    return finalContainerNavigationUrl(state, delta)
        == containerNavigationUrlForLocation(finalDisplayedLocation(state, delta));
}

bool finalOpenedCollectionVideoIsValid(
    const kiriview::ImageDocumentState& state, const kiriview::ImageOpenResolvedStateDelta& delta)
{
    if (finalSourceKind(state, delta) != kiriview::ImageDocumentPageKind::Video) {
        return true;
    }

    return finalUnsupportedOpenedCollectionVideo(state, delta)
        || !finalDisplayedLocation(state, delta).openedCollectionScope().isEmpty();
}

bool finalImageOpenStateIsValid(
    const kiriview::ImageDocumentState& state, const kiriview::ImageOpenResolvedStateDelta& delta)
{
    const kiriview::ImageDocumentStatus status = delta.status.value_or(state.status());
    const bool loading = delta.loading.value_or(state.loading());
    const bool hasError = !finalErrorString(state, delta).isEmpty();

    switch (status) {
    case kiriview::ImageDocumentStatus::Null:
        return !loading && !hasError && finalContainerNavigationUrl(state, delta).isEmpty();
    case kiriview::ImageDocumentStatus::Loading:
        return loading && !hasError;
    case kiriview::ImageDocumentStatus::Ready:
        return !loading && !hasError && !finalSourceUrl(state, delta).isEmpty()
            && !finalDisplayedLocation(state, delta).isEmpty()
            && finalReadyContainerNavigationUrlIsValid(state, delta)
            && finalOpenedCollectionVideoIsValid(state, delta);
    case kiriview::ImageDocumentStatus::Error:
        return !loading && hasError;
    }

    return false;
}

class ImageOpenApplicationPlanApplier final
{
public:
    explicit ImageOpenApplicationPlanApplier(kiriview::ImageDocumentState& state)
        : m_state(state)
        , m_batch(m_state.beginChangeBatch())
    {
    }

    void apply(kiriview::ImageOpenApplicationPlan plan)
    {
        if (!finalImageOpenStateIsValid(m_state, plan.stateDelta)) {
            return;
        }

        applyStateDelta(plan.stateDelta);
        m_runtimePlan = std::move(plan.runtimePlan);
    }

    kiriview::ImageDocumentRuntimePlan takeRuntimePlan() { return std::move(m_runtimePlan); }

private:
    void applyStateDelta(const kiriview::ImageOpenResolvedStateDelta& delta)
    {
        applyUnsupportedOpenedCollectionVideo(delta.unsupportedOpenedCollectionVideo, false);

        if (trackedLoadCompletionBeforeVisibleState(delta)) {
            applyTrackedLoadCompletion(delta);
            applyContainerNavigationUrl(delta.containerNavigationUrl);
            applySourceUrl(delta.sourceUrl);
            applySourceKind(delta.sourceKind);
            applyDisplayedLocation(delta.displayedLocation);
            applyError(delta.errorString, delta.loadFailure);
            applyEmbeddedMetadata(delta.embeddedMetadata);
            applyStatus(delta.status);
            applyUnsupportedOpenedCollectionVideo(delta.unsupportedOpenedCollectionVideo, true);
            return;
        }

        applySourceUrl(delta.sourceUrl);
        applySourceKind(delta.sourceKind);
        applyDisplayedLocation(delta.displayedLocation);
        applyContainerNavigationUrl(delta.containerNavigationUrl);
        applyError(delta.errorString, delta.loadFailure);
        applyEmbeddedMetadata(delta.embeddedMetadata);
        if (delta.clearLoadingContainerNavigationUrl) {
            applyTrackedLoadCompletion(delta);
        } else {
            applyLoading(delta.loading);
        }
        applyStatus(delta.status);
        applyUnsupportedOpenedCollectionVideo(delta.unsupportedOpenedCollectionVideo, true);
    }

    bool trackedLoadCompletionBeforeVisibleState(
        const kiriview::ImageOpenResolvedStateDelta& delta) const
    {
        return delta.clearLoadingContainerNavigationUrl && !delta.displayedLocation.has_value();
    }

    void applyTrackedLoadCompletion(const kiriview::ImageOpenResolvedStateDelta& delta)
    {
        if (delta.clearLoadingContainerNavigationUrl) {
            m_state.clearLoadingContainerNavigationUrl();
        }
        applyLoading(delta.loading);
    }

    void applySourceUrl(const std::optional<QUrl>& url)
    {
        if (url.has_value()) {
            m_state.setSourceUrl(*url);
        }
    }

    void applySourceKind(std::optional<kiriview::ImageDocumentPageKind> sourceKind)
    {
        if (sourceKind.has_value()) {
            m_state.setSourceKind(*sourceKind);
        }
    }

    void applyDisplayedLocation(const std::optional<kiriview::DisplayedImageLocation>& location)
    {
        if (location.has_value()) {
            m_state.setDisplayedImageLocation(*location);
        }
    }

    void applyContainerNavigationUrl(const std::optional<QUrl>& url)
    {
        if (url.has_value()) {
            m_state.setContainerNavigationUrl(*url);
        }
    }

    void applyLoading(std::optional<bool> loading)
    {
        if (loading.has_value()) {
            m_state.setLoading(*loading);
        }
    }

    void applyStatus(std::optional<kiriview::ImageDocumentStatus> status)
    {
        if (status.has_value()) {
            m_state.setStatus(*status);
        }
    }

    void applyError(const std::optional<QString>& errorString,
        const std::optional<kiriview::ImageLoadFailure>& loadFailure)
    {
        if (loadFailure.has_value()) {
            m_state.setLoadFailure(*loadFailure);
            return;
        }

        if (errorString.has_value()) {
            m_state.setErrorString(*errorString);
        }
    }

    void applyEmbeddedMetadata(const std::optional<kiriview::EmbeddedMetadata>& metadata)
    {
        if (metadata.has_value()) {
            m_state.setEmbeddedMetadata(*metadata);
        }
    }

    void applyUnsupportedOpenedCollectionVideo(std::optional<bool> unsupported, bool targetValue)
    {
        if (unsupported.has_value() && *unsupported == targetValue) {
            m_state.setUnsupportedOpenedCollectionVideo(*unsupported);
        }
    }

    kiriview::ImageDocumentState& m_state;
    kiriview::ImageDocumentState::ChangeBatch m_batch;
    kiriview::ImageDocumentRuntimePlan m_runtimePlan;
};
}

namespace kiriview {
ImageDocumentRuntimePlan applyImageOpenApplicationPlan(
    ImageDocumentState& state, ImageOpenApplicationPlan plan)
{
    ImageOpenApplicationPlanApplier applier(state);
    applier.apply(std::move(plan));
    return applier.takeRuntimePlan();
}
}
