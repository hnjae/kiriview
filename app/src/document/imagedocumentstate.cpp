// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentstate.h"

#include "location/imagedocumentlocation.h"

#include <utility>

namespace {
template <typename Value> bool replaceIfChanged(Value& current, const Value& next)
{
    if (current == next) {
        return false;
    }

    current = next;
    return true;
}
}

namespace kiriview {
ImageDocumentState::ImageDocumentState(ChangeCallback changeCallback)
    : m_ownedChanges(std::make_unique<ImageDocumentChangeBatcher>(std::move(changeCallback)))
    , m_changes(m_ownedChanges.get())
{
}

ImageDocumentState::ImageDocumentState(ImageDocumentChangeBatcher& changes)
    : m_changes(&changes)
{
}

ImageDocumentState::ChangeBatch ImageDocumentState::beginChangeBatch()
{
    return m_changes->beginBatch();
}

const ImageDocumentSelectedTarget& ImageDocumentState::selectedTarget() const
{
    return m_selectedTarget;
}

const QUrl& ImageDocumentState::sourceUrl() const { return m_selectedTarget.url; }

ImageDocumentPageKind ImageDocumentState::sourceKind() const { return m_selectedTarget.kind; }

const OpenedCollectionScopeLocation& ImageDocumentState::selectedOpenedCollectionScope() const
{
    return m_selectedTarget.openedCollectionScope;
}

const DisplayedImageLocation& ImageDocumentState::displayedImageLocation() const
{
    return m_displayedImageLocation;
}

const OpenedCollectionScopeLocation& ImageDocumentState::displayedOpenedCollectionScope() const
{
    return m_displayedImageLocation.openedCollectionScope();
}

const QUrl& ImageDocumentState::displayedUrl() const { return m_displayedImageLocation.imageUrl(); }

ImageDocumentStatus ImageDocumentState::status() const { return m_status; }

bool ImageDocumentState::loading() const { return m_loading; }

quint64 ImageDocumentState::presentationLifecycleRevision() const
{
    return m_presentationLifecycleRevision;
}

const QString& ImageDocumentState::errorString() const { return m_errorString; }

const std::optional<ImageLoadFailure>& ImageDocumentState::loadFailure() const
{
    return m_loadFailure;
}

QString ImageDocumentState::windowTitleFileName() const
{
    return windowTitleFileNameForImageLocation(sourceUrl(), selectedOpenedCollectionScope());
}

const QUrl& ImageDocumentState::containerNavigationUrl() const { return m_containerNavigationUrl; }

const QUrl& ImageDocumentState::loadingContainerNavigationUrl() const
{
    return m_loadingContainerNavigationUrl;
}

bool ImageDocumentState::containerNavigationAvailable() const
{
    return !m_containerNavigationUrl.isEmpty();
}

bool ImageDocumentState::unsupportedOpenedCollectionVideo() const
{
    return m_unsupportedOpenedCollectionVideo;
}

const EmbeddedMetadata& ImageDocumentState::embeddedMetadata() const { return m_embeddedMetadata; }

void ImageDocumentState::setSelectedTarget(const ImageDocumentSelectedTarget& target)
{
    const QUrl previousSourceUrl = sourceUrl();
    const OpenedCollectionScopeLocation previousScope = selectedOpenedCollectionScope();
    const QString previousWindowTitle = windowTitleFileName();
    if (!replaceIfChanged(m_selectedTarget, target)) {
        return;
    }

    [[maybe_unused]] auto batch = beginChangeBatch();
    if (previousSourceUrl != sourceUrl()) {
        notify(ImageDocumentChange::SourceUrl);
    }
    if (previousScope != selectedOpenedCollectionScope()) {
        notify(ImageDocumentChange::SelectedTargetScope);
    }
    if (previousWindowTitle != windowTitleFileName()) {
        notify(ImageDocumentChange::WindowTitleFileName);
    }
}

void ImageDocumentState::setDisplayedImageLocation(const DisplayedImageLocation& location)
{
    replaceDisplayedImageLocation(location);
}

void ImageDocumentState::clearDisplayedImageLocation()
{
    replaceDisplayedImageLocation(DisplayedImageLocation {});
}

void ImageDocumentState::replaceDisplayedImageLocation(const DisplayedImageLocation& location)
{
    const QUrl previousDisplayedUrl = displayedUrl();
    if (!replaceIfChanged(m_displayedImageLocation, location)) {
        return;
    }

    if (previousDisplayedUrl != displayedUrl()) {
        notify(ImageDocumentChange::DisplayedUrl);
    }
}

void ImageDocumentState::setStatus(ImageDocumentStatus status)
{
    if (status != ImageDocumentStatus::Error) {
        m_loadFailure.reset();
    }

    if (replaceIfChanged(m_status, status)) {
        notify(ImageDocumentChange::Status);
    }
}

void ImageDocumentState::setLoading(bool loading)
{
    if (replaceIfChanged(m_loading, loading)) {
        notify(ImageDocumentChange::Loading);
    }
}

void ImageDocumentState::advancePresentationLifecycle()
{
    ++m_presentationLifecycleRevision;
    if (m_presentationLifecycleRevision == 0) {
        ++m_presentationLifecycleRevision;
    }
    notify(ImageDocumentChange::PresentationLifecycle);
}

void ImageDocumentState::setErrorString(const QString& errorString)
{
    m_loadFailure.reset();

    if (replaceIfChanged(m_errorString, errorString)) {
        notify(ImageDocumentChange::ErrorString);
    }
}

void ImageDocumentState::setLoadFailure(ImageLoadFailure failure)
{
    m_loadFailure = std::move(failure);

    if (replaceIfChanged(m_errorString, m_loadFailure->userMessage)) {
        notify(ImageDocumentChange::ErrorString);
    }
}

void ImageDocumentState::setContainerNavigationUrl(const QUrl& containerUrl)
{
    if (replaceIfChanged(m_containerNavigationUrl, containerUrl)) {
        notify(ImageDocumentChange::ContainerNavigation);
    }
}

void ImageDocumentState::setLoadingContainerNavigationUrl(const QUrl& containerUrl)
{
    m_loadingContainerNavigationUrl = containerUrl;
}

void ImageDocumentState::clearLoadingContainerNavigationUrl()
{
    m_loadingContainerNavigationUrl = QUrl();
}

void ImageDocumentState::setUnsupportedOpenedCollectionVideo(bool unsupported)
{
    if (replaceIfChanged(m_unsupportedOpenedCollectionVideo, unsupported)) {
        notify(ImageDocumentChange::UnsupportedOpenedCollectionVideo);
    }
}

void ImageDocumentState::setEmbeddedMetadata(EmbeddedMetadata metadata)
{
    m_embeddedMetadata = std::move(metadata);
    notify(ImageDocumentChange::EmbeddedMetadata);
}

void ImageDocumentState::notify(ImageDocumentChange change) { m_changes->notify(change); }
}
