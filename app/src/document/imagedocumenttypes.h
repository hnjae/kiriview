// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTTYPES_H
#define KIRIVIEW_IMAGEDOCUMENTTYPES_H

namespace kiriview {
enum class ImageDocumentStatus {
    Null,
    Loading,
    Ready,
    Error,
};

enum class ImageZoomMode {
    Fit,
    FitHeight,
    FitWidth,
    Manual,
};

enum class ImageDocumentChange {
    SourceUrl,
    SelectedTargetScope,
    Status,
    Loading,
    LoadingTarget,
    ErrorString,
    WindowTitleFileName,
    DisplayedUrl,
    PageNavigation,
    ContainerNavigation,
    FileDeletionInProgress,
    TwoPageMode,
    RightToLeftReading,
    UnsupportedOpenedCollectionVideo,
    EmbeddedMetadata,
    ViewportProjection,
};
}

#endif
