// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kiriimagedocument.h"

#include "decoding/imageformatregistry.h"
#include "document/imagedocumentruntime.h"
#include "facade/imagedocumentpublicsignals.h"
#include "location/imagelocation.h"
#include "system/filedeletion.h"

#include <ImageViewport/imageviewport.h>

#include <QtMath>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>

namespace {
using kiriview::ImageDocumentChange;
using kiriview::ImageDocumentStatus;
using kiriview::ImageZoomMode;

ImageZoomMode toImageZoomMode(KiriImageDocument::ZoomMode zoomMode)
{
    switch (zoomMode) {
    case KiriImageDocument::ZoomMode::Fit:
        return ImageZoomMode::Fit;
    case KiriImageDocument::ZoomMode::FitHeight:
        return ImageZoomMode::FitHeight;
    case KiriImageDocument::ZoomMode::FitWidth:
        return ImageZoomMode::FitWidth;
    case KiriImageDocument::ZoomMode::Manual:
        return ImageZoomMode::Manual;
    }

    return ImageZoomMode::Fit;
}

KiriImageDocument::ZoomMode fromImageZoomMode(ImageZoomMode zoomMode)
{
    switch (zoomMode) {
    case ImageZoomMode::Fit:
        return KiriImageDocument::ZoomMode::Fit;
    case ImageZoomMode::FitHeight:
        return KiriImageDocument::ZoomMode::FitHeight;
    case ImageZoomMode::FitWidth:
        return KiriImageDocument::ZoomMode::FitWidth;
    case ImageZoomMode::Manual:
        return KiriImageDocument::ZoomMode::Manual;
    }

    return KiriImageDocument::ZoomMode::Fit;
}

kiriview::FileDeletionMode toFileDeletionMode(KiriImageDocument::DeletionMode deletionMode)
{
    switch (deletionMode) {
    case KiriImageDocument::DeletionMode::MoveToTrash:
        return kiriview::FileDeletionMode::MoveToTrash;
    case KiriImageDocument::DeletionMode::DeletePermanently:
        return kiriview::FileDeletionMode::DeletePermanently;
    }

    return kiriview::FileDeletionMode::MoveToTrash;
}

KiriImageDocument::Status fromImageDocumentStatus(ImageDocumentStatus status)
{
    switch (status) {
    case ImageDocumentStatus::Null:
        return KiriImageDocument::Status::Null;
    case ImageDocumentStatus::Loading:
        return KiriImageDocument::Status::Loading;
    case ImageDocumentStatus::Ready:
        return KiriImageDocument::Status::Ready;
    case ImageDocumentStatus::Error:
        return KiriImageDocument::Status::Error;
    }

    return KiriImageDocument::Status::Null;
}

kiriview::ImageDocumentPublicSignalOperations publicSignalOperations(KiriImageDocument& document)
{
    kiriview::ImageDocumentPublicSignalOperations operations;
    operations.sourceUrlChanged = [&document]() { Q_EMIT document.sourceUrlChanged(); };
    operations.statusChanged = [&document]() { Q_EMIT document.statusChanged(); };
    operations.loadingChanged = [&document]() { Q_EMIT document.loadingChanged(); };
    operations.presentationLifecycleChanged
        = [&document]() { Q_EMIT document.presentationLifecycleTokenChanged(); };
    operations.errorStringChanged = [&document]() { Q_EMIT document.errorStringChanged(); };
    operations.windowTitleFileNameChanged
        = [&document]() { Q_EMIT document.windowTitleFileNameChanged(); };
    operations.displayedUrlChanged = [&document]() { Q_EMIT document.displayedUrlChanged(); };
    operations.completeAuthoritativeDisplayAvailableChanged
        = [&document]() { Q_EMIT document.completeAuthoritativeDisplayAvailableChanged(); };
    operations.imageSizeChanged = [&document]() { Q_EMIT document.imageSizeChanged(); };
    operations.viewportFrameChanged = [&document]() { Q_EMIT document.viewportFrameChanged(); };
    operations.zoomPercentKnownChanged
        = [&document]() { Q_EMIT document.zoomPercentKnownChanged(); };
    operations.zoomPercentChanged = [&document]() { Q_EMIT document.zoomPercentChanged(); };
    operations.zoomModeChanged = [&document]() { Q_EMIT document.zoomModeChanged(); };
    operations.maximumManualZoomPercentChanged
        = [&document]() { Q_EMIT document.maximumManualZoomPercentChanged(); };
    operations.pageNavigationChanged = [&document]() { Q_EMIT document.pageNavigationChanged(); };
    operations.containerNavigationChanged
        = [&document]() { Q_EMIT document.containerNavigationChanged(); };
    operations.fileDeletionInProgressChanged
        = [&document]() { Q_EMIT document.fileDeletionInProgressChanged(); };
    operations.twoPageModeChanged = [&document]() { Q_EMIT document.twoPageModeChanged(); };
    operations.rightToLeftReadingChanged
        = [&document]() { Q_EMIT document.rightToLeftReadingChanged(); };
    operations.imageDocumentSourceScopeChanged
        = [&document]() { Q_EMIT document.imageDocumentSourceScopeChanged(); };
    operations.unsupportedOpenedCollectionVideoChanged
        = [&document]() { Q_EMIT document.unsupportedOpenedCollectionVideoChanged(); };
    operations.embeddedMetadataChanged
        = [&document]() { Q_EMIT document.embeddedMetadataChanged(); };
    return operations;
}
}

KiriImageDocument::KiriImageDocument(QObject* parent)
    : KiriImageDocument(kiriview::ImageDocumentRuntimeDependencyOverrides {}, parent)
{
}

KiriImageDocument::KiriImageDocument(
    kiriview::ImageDocumentRuntimeDependencyOverrides dependencies, QObject* parent)
    : KiriImageDocument(std::move(dependencies), {}, parent)
{
}

KiriImageDocument::KiriImageDocument(kiriview::ImageDocumentRuntimeDependencyOverrides dependencies,
    std::function<void(const QString&)> fileDeletionFailed, QObject* parent)
    : QObject(parent)
{
    m_runtime = std::make_unique<kiriview::ImageDocumentRuntime>(
        this,
        [this](const std::vector<ImageDocumentChange>& changes) { handleDocumentChanges(changes); },
        std::move(dependencies), std::move(fileDeletionFailed),
        [this](const QString& message) { Q_EMIT unsupportedOpenedCollectionVideoEntered(message); },
        [this](const QString& message) { Q_EMIT containerNavigationBoundaryReached(message); });
}

KiriImageDocument::~KiriImageDocument() = default;

QUrl KiriImageDocument::sourceUrl() const { return m_runtime->sourceUrl(); }

kiriview::ImageDocumentPageKind KiriImageDocument::sourceKind() const
{
    return m_runtime->sourceKind();
}

void KiriImageDocument::setSourceUrl(const QUrl& sourceUrl) { m_runtime->setSourceUrl(sourceUrl); }

void KiriImageDocument::setSource(const kiriview::ResolvedNavigationSource& source)
{
    m_runtime->setSource(source);
}

kiriview::MediaEntrySourceVideoPlaybackDeviceResult
KiriImageDocument::loadOpenedCollectionVideoPlaybackDevice(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope,
    const QUrl& videoUrl) const
{
    return m_runtime->loadOpenedCollectionVideoPlaybackDevice(openedCollectionScope, videoUrl);
}

KiriImageDocument::Status KiriImageDocument::status() const
{
    return fromImageDocumentStatus(m_runtime->status());
}

bool KiriImageDocument::loading() const { return m_runtime->loading(); }

QString KiriImageDocument::presentationLifecycleToken() const
{
    return m_runtime->presentationLifecycleToken();
}

QString KiriImageDocument::errorString() const { return m_runtime->errorString(); }

QString KiriImageDocument::windowTitleFileName() const { return m_runtime->windowTitleFileName(); }

QUrl KiriImageDocument::displayedUrl() const { return m_runtime->displayedUrl(); }

bool KiriImageDocument::completeAuthoritativeDisplayAvailable() const
{
    return m_runtime->completeAuthoritativeDisplayAvailable();
}

kiriview::OpenedCollectionScopeLocation KiriImageDocument::displayedOpenedCollectionScope() const
{
    return m_runtime->displayedOpenedCollectionScope();
}

QSize KiriImageDocument::imageSize() const { return m_runtime->imageSize(); }

QSize KiriImageDocument::primaryImageSize() const { return m_runtime->primaryImageSize(); }

QSize KiriImageDocument::secondaryImageSize() const { return m_runtime->secondaryImageSize(); }

bool KiriImageDocument::viewportHorizontallyPannable() const
{
    return m_runtime->viewportHorizontallyPannable();
}

bool KiriImageDocument::viewportVerticallyPannable() const
{
    return m_runtime->viewportVerticallyPannable();
}

bool KiriImageDocument::viewportPannable() const { return m_runtime->viewportPannable(); }

double KiriImageDocument::horizontalScrollPosition() const
{
    return m_runtime->horizontalScrollPosition();
}

double KiriImageDocument::horizontalScrollPageSize() const
{
    return m_runtime->horizontalScrollPageSize();
}

double KiriImageDocument::verticalScrollPosition() const
{
    return m_runtime->verticalScrollPosition();
}

double KiriImageDocument::verticalScrollPageSize() const
{
    return m_runtime->verticalScrollPageSize();
}

bool KiriImageDocument::zoomPercentKnown() const { return m_runtime->zoomPercentKnown(); }

double KiriImageDocument::zoomPercent() const { return m_runtime->zoomPercent(); }

KiriImageDocument::ZoomMode KiriImageDocument::zoomMode() const
{
    return fromImageZoomMode(m_runtime->zoomMode());
}

KiriImageDocument::ZoomMode KiriImageDocument::fitModeSelection() const
{
    return fromImageZoomMode(m_runtime->fitModeSelection());
}

int KiriImageDocument::minimumManualZoomPercent() const
{
    return static_cast<int>(ImageViewportDisplayLimits::minimumManualZoomPercent());
}

int KiriImageDocument::maximumManualZoomPercent() const
{
    const qreal maximum = m_runtime->maximumManualZoomPercent();
    return std::isfinite(maximum) && maximum > 0.0 ? qCeil(maximum) : 0;
}

double KiriImageDocument::zoomStepFactor() const
{
    return ImageViewportDisplayLimits::manualZoomStepFactor();
}

int KiriImageDocument::currentPageNumber() const { return m_runtime->currentPageNumber(); }

int KiriImageDocument::currentLastPageNumber() const { return m_runtime->currentLastPageNumber(); }

int KiriImageDocument::pageCount() const { return m_runtime->pageCount(); }

kiriview::ImageDocumentPageNavigationSnapshot KiriImageDocument::pageNavigationSnapshot() const
{
    return m_runtime->pageNavigationSnapshot();
}

const kiriview::ImageDocumentPageCandidateListSnapshot&
KiriImageDocument::confirmedPageCandidateSnapshot() const
{
    return m_runtime->confirmedPageCandidateSnapshot();
}

kiriview::ImageDocumentPageActiveNavigationSnapshot
KiriImageDocument::activeNavigationSnapshot() const
{
    return m_runtime->activeNavigationSnapshot();
}

bool KiriImageDocument::containerNavigationAvailable() const
{
    return m_runtime->containerNavigationAvailable();
}

bool KiriImageDocument::ordinaryDirectMediaScopeActive() const
{
    return m_runtime->ordinaryDirectMediaScopeActive();
}

bool KiriImageDocument::openedCollectionScopeActive() const
{
    return m_runtime->openedCollectionScopeActive();
}

bool KiriImageDocument::fileDeletionInProgress() const
{
    return m_runtime->fileDeletionInProgress();
}

bool KiriImageDocument::twoPageModeEnabled() const { return m_runtime->twoPageModeEnabled(); }

void KiriImageDocument::setTwoPageModeEnabled(bool enabled)
{
    m_runtime->setTwoPageModeEnabled(enabled);
}

bool KiriImageDocument::twoPageModeAvailable() const { return m_runtime->twoPageModeAvailable(); }

bool KiriImageDocument::rightToLeftReadingEnabled() const
{
    return m_runtime->rightToLeftReadingEnabled();
}

void KiriImageDocument::setRightToLeftReadingEnabled(bool enabled)
{
    m_runtime->setRightToLeftReadingEnabled(enabled);
}

bool KiriImageDocument::rightToLeftReadingAvailable() const
{
    return m_runtime->rightToLeftReadingAvailable();
}

bool KiriImageDocument::secondaryPageVisible() const { return m_runtime->secondaryPageVisible(); }

bool KiriImageDocument::unsupportedOpenedCollectionVideo() const
{
    return m_runtime->unsupportedOpenedCollectionVideo();
}

std::optional<kiriview::DisplayedPredecodeImage>
KiriImageDocument::primaryDisplayedPredecodeImage() const
{
    return m_runtime->primaryDisplayedPredecodeImage();
}

kiriview::ImageFirstDisplayDecodeContext KiriImageDocument::firstDisplayDecodeContext() const
{
    return m_runtime->firstDisplayDecodeContext();
}

const kiriview::EmbeddedMetadata& KiriImageDocument::embeddedMetadata() const
{
    return m_runtime->embeddedMetadata();
}

void KiriImageDocument::attachImageViewport(ImageViewport* viewport)
{
    m_runtime->attachImageViewport(viewport);
}

void KiriImageDocument::detachImageViewport(ImageViewport* viewport)
{
    m_runtime->detachImageViewport(viewport);
}

void KiriImageDocument::openPreviousPage() { m_runtime->openPreviousPage(); }

void KiriImageDocument::openNextPage() { m_runtime->openNextPage(); }

void KiriImageDocument::openPreviousSinglePage() { m_runtime->openPreviousSinglePage(); }

void KiriImageDocument::openNextSinglePage() { m_runtime->openNextSinglePage(); }

void KiriImageDocument::openPreviousContainer() { m_runtime->openPreviousContainer(); }

void KiriImageDocument::openNextContainer() { m_runtime->openNextContainer(); }

void KiriImageDocument::deleteDisplayedFile(DeletionMode mode)
{
    m_runtime->deleteDisplayedFile(toFileDeletionMode(mode));
}

void KiriImageDocument::openImageAtPage(int pageNumber) { m_runtime->openImageAtPage(pageNumber); }

void KiriImageDocument::rotateClockwise() { m_runtime->rotateClockwise(); }

void KiriImageDocument::rotateCounterclockwise() { m_runtime->rotateCounterclockwise(); }

double KiriImageDocument::steppedManualZoomPercent(double stepCount) const
{
    return m_runtime->steppedManualZoomPercent(stepCount);
}

bool KiriImageDocument::requestManualZoomPercent(double zoomPercent)
{
    return m_runtime->requestManualZoomPercentAtCenter(zoomPercent);
}

bool KiriImageDocument::requestZoomByStep(double stepCount, QPointF viewportAnchorPoint)
{
    return m_runtime->requestZoomByStep(stepCount, viewportAnchorPoint);
}

bool KiriImageDocument::requestZoomByStepAtCenter(double stepCount)
{
    return m_runtime->requestZoomByStepAtCenter(stepCount);
}

bool KiriImageDocument::requestFitMode(ZoomMode zoomMode)
{
    if (status() != Status::Ready) {
        return false;
    }

    if (zoomMode == ZoomMode::Fit) {
        m_runtime->resetZoom();
        return true;
    }

    m_runtime->setFitMode(toImageZoomMode(zoomMode));
    return true;
}

bool KiriImageDocument::requestToggleFitOrActualSize(QPointF viewportPoint)
{
    return m_runtime->requestToggleFitOrActualSize(viewportPoint);
}

bool KiriImageDocument::requestViewportPanBy(double deltaX, double deltaY)
{
    return m_runtime->requestViewportPanBy(QPointF(deltaX, deltaY)) > 0;
}

bool KiriImageDocument::requestViewportPanToInitialScanPosition()
{
    return m_runtime->requestViewportPanToInitialScanPosition() > 0;
}

bool KiriImageDocument::requestViewportPanToFinalScanPosition()
{
    return m_runtime->requestViewportPanToFinalScanPosition() > 0;
}

bool KiriImageDocument::requestViewportScanForward()
{
    return m_runtime->requestViewportScanForward() > 0;
}

bool KiriImageDocument::requestViewportScanBackward()
{
    return m_runtime->requestViewportScanBackward() > 0;
}

void KiriImageDocument::requestNextViewportTargetAnchorAtEnd()
{
    m_runtime->requestNextViewportTargetAnchorAtEnd();
}

QPointF KiriImageDocument::nearestImageViewportPoint(QPointF viewportPoint) const
{
    return m_runtime->nearestImageViewportPoint(viewportPoint);
}

void KiriImageDocument::requestToggleTwoPageMode() { setTwoPageModeEnabled(!twoPageModeEnabled()); }

void KiriImageDocument::requestToggleRightToLeftReading()
{
    setRightToLeftReadingEnabled(!rightToLeftReadingEnabled());
}

bool KiriImageDocument::submitHorizontalScrollPosition(double position)
{
    return m_runtime->submitHorizontalScrollPosition(position);
}

bool KiriImageDocument::submitVerticalScrollPosition(double position)
{
    return m_runtime->submitVerticalScrollPosition(position);
}

void KiriImageDocument::handleDocumentChanges(const std::vector<ImageDocumentChange>& changes)
{
    kiriview::ImageDocumentPublicSignalOperations operations = publicSignalOperations(*this);
    operations.sessionSnapshotChanged = [this]() { Q_EMIT documentSessionSnapshotChanged(); };
    kiriview::ImageDocumentPublicSignalEmitter(std::move(operations)).emitChanges(changes);
}
