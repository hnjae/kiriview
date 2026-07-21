// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPUBLICSIGNALS_H
#define KIRIVIEW_IMAGEDOCUMENTPUBLICSIGNALS_H

#include "document/imagedocumenttypes.h"

#include <functional>
#include <vector>

namespace kiriview {
enum class ImageDocumentPublicSignal {
    SourceUrl,
    Status,
    Loading,
    ErrorString,
    WindowTitleFileName,
    DisplayedUrl,
    ImageSize,
    ViewportFrame,
    ZoomPercentKnown,
    ZoomPercent,
    ZoomMode,
    MaximumManualZoomPercent,
    PageNavigation,
    ContainerNavigation,
    FileDeletionInProgress,
    TwoPageMode,
    RightToLeftReading,
    ImageDocumentSourceScope,
    UnsupportedOpenedCollectionVideo,
    EmbeddedMetadata,
};

struct ImageDocumentPublicSignalOperations
{
    std::function<void()> sessionSnapshotChanged;
    std::function<void()> sourceUrlChanged;
    std::function<void()> statusChanged;
    std::function<void()> loadingChanged;
    std::function<void()> errorStringChanged;
    std::function<void()> windowTitleFileNameChanged;
    std::function<void()> displayedUrlChanged;
    std::function<void()> imageSizeChanged;
    std::function<void()> viewportFrameChanged;
    std::function<void()> zoomPercentKnownChanged;
    std::function<void()> zoomPercentChanged;
    std::function<void()> zoomModeChanged;
    std::function<void()> maximumManualZoomPercentChanged;
    std::function<void()> pageNavigationChanged;
    std::function<void()> containerNavigationChanged;
    std::function<void()> fileDeletionInProgressChanged;
    std::function<void()> twoPageModeChanged;
    std::function<void()> rightToLeftReadingChanged;
    std::function<void()> imageDocumentSourceScopeChanged;
    std::function<void()> unsupportedOpenedCollectionVideoChanged;
    std::function<void()> embeddedMetadataChanged;
};

class ImageDocumentPublicSignalEmitter final
{
public:
    explicit ImageDocumentPublicSignalEmitter(ImageDocumentPublicSignalOperations operations);

    void emitChanges(const std::vector<ImageDocumentChange>& changes) const;
    void emitSignal(ImageDocumentPublicSignal signal) const;

private:
    ImageDocumentPublicSignalOperations m_operations;
};

std::vector<ImageDocumentPublicSignal> imageDocumentPublicSignalsForChanges(
    const std::vector<ImageDocumentChange>& changes);
std::vector<ImageDocumentPublicSignal> imageDocumentPublicSignals(ImageDocumentChange change);
}

#endif
