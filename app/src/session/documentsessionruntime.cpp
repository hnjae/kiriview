// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionruntime.h"

#include "documentsessionruntimegraph.h"

#include <utility>

namespace kiriview {
DocumentSessionRuntime::DocumentSessionRuntime(QObject* owner,
    DocumentSessionImageDocumentSnapshotPort imageDocument,
    DocumentSessionImageDocumentCommandPort imageCommands,
    DocumentSessionVideoDocumentSnapshotPort videoDocument,
    DocumentSessionVideoDocumentCommandPort videoCommands, ChangeCallback changeCallback,
    DocumentSessionRuntimeDependencies dependencies)
    : m_imageDocument(std::move(imageDocument))
    , m_imageCommands(std::move(imageCommands))
    , m_videoDocument(std::move(videoDocument))
    , m_videoCommands(std::move(videoCommands))
    , m_state(std::move(changeCallback))
{
    m_runtimeGraph = std::make_unique<DocumentSessionRuntimeGraph>(owner,
        DocumentSessionRuntimeGraphPorts {
            m_state,
            m_imageDocument,
            m_videoDocument,
            m_imagePublicSnapshot,
            m_videoPublicSnapshot,
        },
        m_imageCommands, m_videoCommands, std::move(dependencies));
}

DocumentSessionRuntime::~DocumentSessionRuntime() = default;

QUrl DocumentSessionRuntime::sourceUrl() const { return m_runtimeGraph->sourceUrl(); }

void DocumentSessionRuntime::setSourceUrl(const QUrl& sourceUrl)
{
    m_runtimeGraph->setSourceUrl(sourceUrl);
}

DocumentSessionKind DocumentSessionRuntime::documentKind() const
{
    return m_runtimeGraph->documentKind();
}

quint64 DocumentSessionRuntime::publicProjectionRevision() const
{
    return m_runtimeGraph->publicProjectionRevision();
}

QString DocumentSessionRuntime::errorString() const { return m_runtimeGraph->errorString(); }

QString DocumentSessionRuntime::windowTitleSubject() const
{
    return m_runtimeGraph->windowTitleSubject();
}

bool DocumentSessionRuntime::displayedFileDeletionAvailable() const
{
    return m_runtimeGraph->displayedFileDeletionAvailable();
}

bool DocumentSessionRuntime::displayedMediaOpenWithAvailable() const
{
    return m_runtimeGraph->displayedMediaOpenWithAvailable();
}

bool DocumentSessionRuntime::fileDeletionInProgress() const
{
    return m_runtimeGraph->fileDeletionInProgress();
}

const MediaInformationProjectionSnapshot& DocumentSessionRuntime::mediaInformationSnapshot() const
{
    return m_runtimeGraph->mediaInformationSnapshot();
}

bool DocumentSessionRuntime::activeZoomPercentAvailable() const
{
    return m_runtimeGraph->activeZoomPercentAvailable();
}

bool DocumentSessionRuntime::activeZoomPercentKnown() const
{
    return m_runtimeGraph->activeZoomPercentKnown();
}

qreal DocumentSessionRuntime::activeZoomPercent() const
{
    return m_runtimeGraph->activeZoomPercent();
}

bool DocumentSessionRuntime::activeZoomEditable() const
{
    return m_runtimeGraph->activeZoomEditable();
}

bool DocumentSessionRuntime::activeImageReady() const { return m_runtimeGraph->activeImageReady(); }

bool DocumentSessionRuntime::activeImageReplacementFallbackAvailable() const
{
    return m_runtimeGraph->activeImageReplacementFallbackAvailable();
}

bool DocumentSessionRuntime::activeImageUnsupportedOpenedCollectionVideo() const
{
    return m_runtimeGraph->activeImageUnsupportedOpenedCollectionVideo();
}

bool DocumentSessionRuntime::activeImageOpenedCollectionScopeActive() const
{
    return m_runtimeGraph->activeImageOpenedCollectionScopeActive();
}

bool DocumentSessionRuntime::activeImageRightToLeftReadingActive() const
{
    return m_runtimeGraph->activeImageRightToLeftReadingActive();
}

bool DocumentSessionRuntime::activeVideoReady() const { return m_runtimeGraph->activeVideoReady(); }

bool DocumentSessionRuntime::activeVideoControlsReady() const
{
    return m_runtimeGraph->activeVideoControlsReady();
}

const DocumentSessionActionStateSnapshot& DocumentSessionRuntime::actionStateSnapshot() const
{
    return m_runtimeGraph->actionStateSnapshot();
}

const DocumentSessionActionAvailabilityFacts&
DocumentSessionRuntime::actionAvailabilityFacts() const
{
    return m_runtimeGraph->actionAvailabilityFacts();
}

bool DocumentSessionRuntime::activeNavigationAvailable() const
{
    return m_runtimeGraph->activeNavigationAvailable();
}

bool DocumentSessionRuntime::activeNavigationKnown() const
{
    return m_runtimeGraph->activeNavigationKnown();
}

bool DocumentSessionRuntime::activeNavigationEditable() const
{
    return m_runtimeGraph->activeNavigationEditable();
}

bool DocumentSessionRuntime::activeNavigationHasTargets() const
{
    return m_runtimeGraph->activeNavigationHasTargets();
}

bool DocumentSessionRuntime::activeNavigationDispatchAvailable() const
{
    return m_runtimeGraph->activeNavigationDispatchAvailable();
}

int DocumentSessionRuntime::activeNavigationCurrentNumber() const
{
    return m_runtimeGraph->activeNavigationCurrentNumber();
}

int DocumentSessionRuntime::activeNavigationCount() const
{
    return m_runtimeGraph->activeNavigationCount();
}

bool DocumentSessionRuntime::canOpenPreviousActiveNavigation() const
{
    return m_runtimeGraph->canOpenPreviousActiveNavigation();
}

bool DocumentSessionRuntime::canOpenNextActiveNavigation() const
{
    return m_runtimeGraph->canOpenNextActiveNavigation();
}

bool DocumentSessionRuntime::atKnownFirstActiveNavigation() const
{
    return m_runtimeGraph->atKnownFirstActiveNavigation();
}

bool DocumentSessionRuntime::atKnownLastActiveNavigation() const
{
    return m_runtimeGraph->atKnownLastActiveNavigation();
}

bool DocumentSessionRuntime::directMediaNavigationBoundaryActive() const
{
    return m_runtimeGraph->directMediaNavigationBoundaryActive();
}

ActiveNavigationBoundaryScope DocumentSessionRuntime::activeNavigationBoundaryScope() const
{
    return m_runtimeGraph->activeNavigationBoundaryScope();
}

ActiveNavigationRevealIntent DocumentSessionRuntime::activeNavigationRevealIntent() const
{
    return m_runtimeGraph->activeNavigationRevealIntent();
}

ActiveNavigationRevealDirection DocumentSessionRuntime::activeNavigationRevealDirection() const
{
    return m_runtimeGraph->activeNavigationRevealDirection();
}

QAbstractListModel* DocumentSessionRuntime::activeNavigationThumbnailModel() const
{
    return m_runtimeGraph->activeNavigationThumbnailModel();
}

bool DocumentSessionRuntime::replaceActiveNavigationThumbnailDemandSnapshot(
    const ActiveNavigationThumbnailDemandSnapshot& snapshot)
{
    return m_runtimeGraph->replaceActiveNavigationThumbnailDemandSnapshot(snapshot);
}

QString DocumentSessionRuntime::nextVideoOutputSurfaceClaimToken()
{
    return m_runtimeGraph->nextVideoOutputSurfaceClaimToken();
}

bool DocumentSessionRuntime::reportVideoOutputSurfaceClaim(const QString& claimToken,
    quint64 projectionRevision, QObject* surfaceOwner, QObject* videoOutput, bool active,
    const QRectF& contentRect, const QRectF& sourceRect)
{
    return m_runtimeGraph->reportVideoOutputSurfaceClaim(
        claimToken, projectionRevision, surfaceOwner, videoOutput, active, contentRect, sourceRect);
}

std::optional<PredecodedImage> DocumentSessionRuntime::findPredecodedImage(
    const DisplayedImageLocation& location) const
{
    return m_runtimeGraph->findPredecodedImage(location);
}

void DocumentSessionRuntime::openPreviousActiveNavigation()
{
    m_runtimeGraph->openPreviousActiveNavigation();
}

void DocumentSessionRuntime::openNextActiveNavigation()
{
    m_runtimeGraph->openNextActiveNavigation();
}

void DocumentSessionRuntime::openFirstActiveNavigation()
{
    m_runtimeGraph->openFirstActiveNavigation();
}

void DocumentSessionRuntime::openLastActiveNavigation()
{
    m_runtimeGraph->openLastActiveNavigation();
}

void DocumentSessionRuntime::openActiveNavigationAtNumber(int number)
{
    m_runtimeGraph->openActiveNavigationAtNumber(number);
}

void DocumentSessionRuntime::openActiveNavigationThumbnailAtNumber(int number)
{
    m_runtimeGraph->openActiveNavigationThumbnailAtNumber(number);
}

ActiveNavigationDispatchOutcome DocumentSessionRuntime::requestPreviousActiveNavigation()
{
    return m_runtimeGraph->requestPreviousActiveNavigation();
}

ActiveNavigationDispatchOutcome DocumentSessionRuntime::requestNextActiveNavigation()
{
    return m_runtimeGraph->requestNextActiveNavigation();
}

void DocumentSessionRuntime::deleteDisplayedFile(FileDeletionMode mode)
{
    m_runtimeGraph->deleteDisplayedFile(mode);
}

void DocumentSessionRuntime::openCurrentMediaWith(MediaOpenWithCallback callback)
{
    m_runtimeGraph->openCurrentMediaWith(std::move(callback));
}
}
