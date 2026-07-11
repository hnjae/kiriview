// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpublicsignals.h"

#include <algorithm>
#include <utility>

namespace {
template <typename Operation> void run(const Operation& operation)
{
    if (operation) {
        operation();
    }
}

bool affectsSessionSnapshot(kiriview::ImageDocumentPublicSignal signal)
{
    using Signal = kiriview::ImageDocumentPublicSignal;
    switch (signal) {
    case Signal::SourceUrl:
    case Signal::Status:
    case Signal::ErrorString:
    case Signal::WindowTitleFileName:
    case Signal::ImageSize:
    case Signal::ZoomPercentKnown:
    case Signal::ZoomPercent:
    case Signal::ZoomMode:
    case Signal::PageNavigation:
    case Signal::ContainerNavigation:
    case Signal::FileDeletionInProgress:
    case Signal::TwoPageMode:
    case Signal::RightToLeftReading:
    case Signal::ImageDocumentSourceScope:
    case Signal::UnsupportedOpenedCollectionVideo:
    case Signal::EmbeddedMetadata:
        return true;
    case Signal::Loading:
    case Signal::DisplayedUrl:
    case Signal::ViewportSize:
    case Signal::ViewportFrame:
    case Signal::VisibleItemRect:
    case Signal::DisplaySize:
    case Signal::MaximumManualZoomPercent:
    case Signal::PresentationTransitionState:
    case Signal::RotationDegrees:
    case Signal::DisplaySource:
        return false;
    }

    return false;
}
}

namespace kiriview {
ImageDocumentPublicSignalEmitter::ImageDocumentPublicSignalEmitter(
    ImageDocumentPublicSignalOperations operations)
    : m_operations(std::move(operations))
{
}

void ImageDocumentPublicSignalEmitter::emitChange(ImageDocumentChange change) const
{
    emitChanges({ change });
}

void ImageDocumentPublicSignalEmitter::emitChanges(
    const std::vector<ImageDocumentChange>& changes) const
{
    const std::vector<ImageDocumentPublicSignal> signals
        = imageDocumentPublicSignalsForChanges(changes);
    if (std::any_of(signals.cbegin(), signals.cend(), affectsSessionSnapshot)) {
        run(m_operations.sessionSnapshotChanged);
    }
    for (ImageDocumentPublicSignal signal : signals) {
        emitSignal(signal);
    }
}

void ImageDocumentPublicSignalEmitter::emitSignal(ImageDocumentPublicSignal signal) const
{
    switch (signal) {
    case ImageDocumentPublicSignal::SourceUrl:
        run(m_operations.sourceUrlChanged);
        return;
    case ImageDocumentPublicSignal::Status:
        run(m_operations.statusChanged);
        return;
    case ImageDocumentPublicSignal::Loading:
        run(m_operations.loadingChanged);
        return;
    case ImageDocumentPublicSignal::ErrorString:
        run(m_operations.errorStringChanged);
        return;
    case ImageDocumentPublicSignal::WindowTitleFileName:
        run(m_operations.windowTitleFileNameChanged);
        return;
    case ImageDocumentPublicSignal::DisplayedUrl:
        run(m_operations.displayedUrlChanged);
        return;
    case ImageDocumentPublicSignal::ImageSize:
        run(m_operations.imageSizeChanged);
        return;
    case ImageDocumentPublicSignal::ViewportSize:
        run(m_operations.viewportSizeChanged);
        return;
    case ImageDocumentPublicSignal::ViewportFrame:
        run(m_operations.viewportFrameChanged);
        return;
    case ImageDocumentPublicSignal::VisibleItemRect:
        run(m_operations.visibleItemRectChanged);
        return;
    case ImageDocumentPublicSignal::DisplaySize:
        run(m_operations.displaySizeChanged);
        return;
    case ImageDocumentPublicSignal::ZoomPercentKnown:
        run(m_operations.zoomPercentKnownChanged);
        return;
    case ImageDocumentPublicSignal::ZoomPercent:
        run(m_operations.zoomPercentChanged);
        return;
    case ImageDocumentPublicSignal::ZoomMode:
        run(m_operations.zoomModeChanged);
        return;
    case ImageDocumentPublicSignal::MaximumManualZoomPercent:
        run(m_operations.maximumManualZoomPercentChanged);
        return;
    case ImageDocumentPublicSignal::PageNavigation:
        run(m_operations.pageNavigationChanged);
        return;
    case ImageDocumentPublicSignal::ContainerNavigation:
        run(m_operations.containerNavigationChanged);
        return;
    case ImageDocumentPublicSignal::FileDeletionInProgress:
        run(m_operations.fileDeletionInProgressChanged);
        return;
    case ImageDocumentPublicSignal::TwoPageMode:
        run(m_operations.twoPageModeChanged);
        return;
    case ImageDocumentPublicSignal::RightToLeftReading:
        run(m_operations.rightToLeftReadingChanged);
        return;
    case ImageDocumentPublicSignal::PresentationTransitionState:
        run(m_operations.presentationTransitionStateChanged);
        return;
    case ImageDocumentPublicSignal::RotationDegrees:
        run(m_operations.rotationDegreesChanged);
        return;
    case ImageDocumentPublicSignal::ImageDocumentSourceScope:
        run(m_operations.imageDocumentSourceScopeChanged);
        return;
    case ImageDocumentPublicSignal::UnsupportedOpenedCollectionVideo:
        run(m_operations.unsupportedOpenedCollectionVideoChanged);
        return;
    case ImageDocumentPublicSignal::EmbeddedMetadata:
        run(m_operations.embeddedMetadataChanged);
        return;
    case ImageDocumentPublicSignal::DisplaySource:
        run(m_operations.displaySourceChanged);
        return;
    }
}

std::vector<ImageDocumentPublicSignal> imageDocumentPublicSignals(ImageDocumentChange change)
{
    switch (change) {
    case ImageDocumentChange::SourceUrl:
        return { ImageDocumentPublicSignal::SourceUrl };
    case ImageDocumentChange::Status:
        return { ImageDocumentPublicSignal::Status, ImageDocumentPublicSignal::ZoomPercentKnown };
    case ImageDocumentChange::Loading:
        return { ImageDocumentPublicSignal::Loading };
    case ImageDocumentChange::ErrorString:
        return { ImageDocumentPublicSignal::ErrorString };
    case ImageDocumentChange::WindowTitleFileName:
        return { ImageDocumentPublicSignal::WindowTitleFileName };
    case ImageDocumentChange::DisplayedUrl:
        return { ImageDocumentPublicSignal::DisplayedUrl };
    case ImageDocumentChange::ImageSize:
        return { ImageDocumentPublicSignal::ImageSize,
            ImageDocumentPublicSignal::ZoomPercentKnown };
    case ImageDocumentChange::ViewportSize:
        return { ImageDocumentPublicSignal::ViewportSize };
    case ImageDocumentChange::ViewportFrame:
        return { ImageDocumentPublicSignal::ViewportFrame };
    case ImageDocumentChange::VisibleItemRect:
        return { ImageDocumentPublicSignal::VisibleItemRect };
    case ImageDocumentChange::DisplaySize:
        return { ImageDocumentPublicSignal::DisplaySize,
            ImageDocumentPublicSignal::ZoomPercentKnown };
    case ImageDocumentChange::ZoomPercent:
        return { ImageDocumentPublicSignal::ZoomPercent };
    case ImageDocumentChange::ZoomMode:
        return { ImageDocumentPublicSignal::ZoomMode };
    case ImageDocumentChange::MaximumManualZoomPercent:
        return { ImageDocumentPublicSignal::MaximumManualZoomPercent };
    case ImageDocumentChange::PageNavigation:
        return { ImageDocumentPublicSignal::PageNavigation };
    case ImageDocumentChange::ContainerNavigation:
        return { ImageDocumentPublicSignal::ContainerNavigation };
    case ImageDocumentChange::FileDeletionInProgress:
        return { ImageDocumentPublicSignal::FileDeletionInProgress };
    case ImageDocumentChange::TwoPageMode:
        return { ImageDocumentPublicSignal::TwoPageMode,
            ImageDocumentPublicSignal::PageNavigation };
    case ImageDocumentChange::RightToLeftReading:
        return { ImageDocumentPublicSignal::RightToLeftReading };
    case ImageDocumentChange::PresentationTransitionState:
        return { ImageDocumentPublicSignal::PresentationTransitionState };
    case ImageDocumentChange::Rotation:
        return { ImageDocumentPublicSignal::RotationDegrees };
    case ImageDocumentChange::UnsupportedOpenedCollectionVideo:
        return { ImageDocumentPublicSignal::UnsupportedOpenedCollectionVideo,
            ImageDocumentPublicSignal::ZoomPercentKnown };
    case ImageDocumentChange::EmbeddedMetadata:
        return { ImageDocumentPublicSignal::EmbeddedMetadata };
    case ImageDocumentChange::DisplaySource:
        return { ImageDocumentPublicSignal::DisplaySource };
    }

    return {};
}

std::vector<ImageDocumentPublicSignal> imageDocumentPublicSignalsForChanges(
    const std::vector<ImageDocumentChange>& changes)
{
    std::vector<ImageDocumentPublicSignal> plannedSignals;
    bool imageDocumentSourceScopeChanged = false;
    for (ImageDocumentChange change : changes) {
        imageDocumentSourceScopeChanged = imageDocumentSourceScopeChanged
            || change == ImageDocumentChange::DisplayedUrl || change == ImageDocumentChange::Status;
        for (ImageDocumentPublicSignal signal : imageDocumentPublicSignals(change)) {
            const bool alreadyPlanned
                = std::find(plannedSignals.cbegin(), plannedSignals.cend(), signal)
                != plannedSignals.cend();
            if (!alreadyPlanned) {
                plannedSignals.push_back(signal);
            }
        }
    }
    if (imageDocumentSourceScopeChanged) {
        plannedSignals.push_back(ImageDocumentPublicSignal::ImageDocumentSourceScope);
    }
    return plannedSignals;
}
}
