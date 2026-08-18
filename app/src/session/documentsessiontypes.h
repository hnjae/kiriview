// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONTYPES_H
#define KIRIVIEW_DOCUMENTSESSIONTYPES_H

#include "session/activenavigationprojection.h"
#include "session/mediainformationprojection.h"

#include <QString>
#include <QUrl>
#include <QtGlobal>

namespace kiriview {
enum class DocumentSessionKind {
    Empty,
    Image,
    Video,
};

enum class DocumentSessionChange {
    PublicProjectionRevision,
    SourceUrl,
    DocumentKind,
    ErrorString,
    WindowTitleSubject,
    ActiveZoomReadout,
    ActiveMediaReadiness,
    OpenWithAvailability,
    FileDeletionAvailability,
    FileDeletionInProgress,
    ActiveNavigation,
    ActiveNavigationRevealIntent,
    ActiveNavigationRevealDirection,
};

enum class ActiveNavigationRevealIntent {
    None,
    ThumbnailActivation,
    AdjacentNavigation,
    LargeJump,
    LoadOrOpen,
    ProgrammaticSync,
};

enum class ActiveNavigationRevealDirection {
    None,
    Previous,
    Next,
};

struct ActiveNavigationRevealContext
{
    ActiveNavigationRevealIntent intent = ActiveNavigationRevealIntent::None;
    ActiveNavigationRevealDirection direction = ActiveNavigationRevealDirection::None;
};

struct ActiveZoomSnapshot
{
    bool available = false;
    bool known = false;
    qreal percent = 0.0;
    bool editable = false;
    int minimumManualPercent = 0;
    int maximumManualPercent = 0;
};

enum class ImagePresentationPhase {
    Unavailable,
    CurrentAuthoritative,
    RetainedPreviousAuthoritative,
};

enum class ImageFitModeSelection {
    Fit,
    FitHeight,
    FitWidth,
};

struct DocumentSessionActionAvailabilityFacts
{
    bool imageReady = false;
    bool containerNavigationAvailable = false;
    bool twoPageModeActive = false;
    bool twoPageModeAvailable = false;
    bool rightToLeftReadingActive = false;
    bool rightToLeftReadingAvailable = false;
    bool fitModeSelected = false;
    bool fitHeightModeSelected = false;
    bool fitWidthModeSelected = false;
};

struct DocumentSessionPublicProjection
{
    ActiveNavigationSourceKind sourceKind = ActiveNavigationSourceKind::None;
    ActiveNavigationBoundaryScope boundaryScope = ActiveNavigationBoundaryScope::None;
    ActiveNavigationSnapshot activeNavigation;
    QString windowTitleSubject;
    bool displayedMediaOpenWithAvailable = false;
    bool displayedFileDeletionAvailable = false;
};

struct DocumentSessionActionStateSnapshot
{
    DocumentSessionActionAvailabilityFacts availability;
    bool displayedMediaOpenWithAvailable = false;
    bool displayedFileDeletionAvailable = false;
    bool fileDeletionInProgress = false;
    ActiveNavigationSnapshot activeNavigation;
    ActiveNavigationBoundaryScope activeNavigationBoundaryScope
        = ActiveNavigationBoundaryScope::None;
    bool imagePannable = false;
    bool videoMode = false;
    bool videoSeekable = false;
    qint64 videoDuration = 0;
    ImagePresentationPhase imagePresentationPhase = ImagePresentationPhase::Unavailable;
    ImageFitModeSelection imageFitModeSelection = ImageFitModeSelection::Fit;
    ActiveZoomSnapshot activeZoom;
    bool imageCollectionControlsVisible = false;
};

struct DocumentSessionPublicSnapshot
{
    quint64 revision = 0;
    quint64 inputRevision = 0;
    QUrl sourceUrl;
    DocumentSessionKind documentKind = DocumentSessionKind::Empty;
    QString errorString;
    bool fileDeletionInProgress = false;
    ActiveZoomSnapshot activeZoom;
    bool activeImageReady = false;
    bool activeImageReplacementFallbackAvailable = false;
    bool activeImageUnsupportedOpenedCollectionVideo = false;
    bool activeImageOpenedCollectionScopeActive = false;
    bool activeImageRightToLeftReadingActive = false;
    bool activeVideoReady = false;
    DocumentSessionActionAvailabilityFacts actionAvailability;
    DocumentSessionPublicProjection projection;
    DocumentSessionActionStateSnapshot actionState;
    MediaInformationProjectionSnapshot mediaInformation;
    ActiveNavigationRevealIntent activeNavigationRevealIntent = ActiveNavigationRevealIntent::None;
    ActiveNavigationRevealDirection activeNavigationRevealDirection
        = ActiveNavigationRevealDirection::None;
};
}

#endif
