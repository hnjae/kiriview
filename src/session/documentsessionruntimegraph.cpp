// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionruntimegraph.h"

#include "navigation/navigationlogging.h"
#include "session/documentsessionactivezoom.h"

#include <QAbstractListModel>
#include <QDebug>
#include <QObject>
#include <QScopedValueRollback>
#include <QString>
#include <utility>
#include <variant>

namespace {
const char* documentKindName(kiriview::DocumentSessionKind kind)
{
    switch (kind) {
    case kiriview::DocumentSessionKind::Empty:
        return "Empty";
    case kiriview::DocumentSessionKind::Image:
        return "Image";
    case kiriview::DocumentSessionKind::Video:
        return "Video";
    }

    return "Unknown";
}

const char* routeKindName(kiriview::DocumentSessionRouteKind kind)
{
    switch (kind) {
    case kiriview::DocumentSessionRouteKind::Empty:
        return "Empty";
    case kiriview::DocumentSessionRouteKind::DirectVideo:
        return "DirectVideo";
    case kiriview::DocumentSessionRouteKind::DirectImage:
        return "DirectImage";
    case kiriview::DocumentSessionRouteKind::ImageDocument:
        return "ImageDocument";
    }

    return "Unknown";
}

void logDirectMediaScope(const char* message, const kiriview::DirectMediaScope& scope)
{
    qCDebug(kiriviewNavigationLog) << message << "currentUrl" << scope.currentUrl << "parentUrl"
                                   << scope.parentUrl << "generation" << scope.generation;
}

void appendConnection(std::vector<QMetaObject::Connection>& connections,
    const kiriview::DocumentSessionSnapshotConnector& connector, QObject* owner,
    kiriview::DocumentSessionSnapshotChangeHandler handler)
{
    if (connector) {
        std::vector<QMetaObject::Connection> nextConnections = connector(owner, std::move(handler));
        connections.insert(connections.end(), nextConnections.begin(), nextConnections.end());
    }
}

kiriview::VideoPlaybackSourceDevice videoPlaybackSourceDeviceFromMediaEntryDevice(
    kiriview::MediaEntrySourceVideoPlaybackDevice device)
{
    return kiriview::VideoPlaybackSourceDevice {
        std::move(device.sourceOwner),
        std::move(device.device),
    };
}

}

namespace kiriview {
DocumentSessionRuntimeGraph::DocumentSessionRuntimeGraph(QObject* owner,
    DocumentSessionRuntimeGraphPorts ports, DocumentSessionImageDocumentCommandPort imageCommands,
    DocumentSessionVideoDocumentCommandPort videoCommands,
    DocumentSessionRuntimeDependencies dependencies)
    : m_owner(owner)
    , m_imageDocument(ports.imageDocument)
    , m_imageDocumentCommandRuntime(std::move(imageCommands))
    , m_imageDocumentSyncRuntime(DocumentSessionImageDocumentSyncRuntimePorts {
          [this](const QUrl& url) {
              const bool changed = m_state.confirmDirectImageCursor(url);
              if (changed) {
                  syncMediaPredecodeScope();
              }
              return changed;
          },
          [this]() {
              const bool changed = m_state.restoreDirectImageCursorAfterFailure();
              if (changed) {
                  syncMediaPredecodeScope();
              }
              return changed;
          },
          [this](const QUrl& url) { m_state.setSourceIdentity(url); },
          [this](bool inProgress) { m_state.setFileDeletionInProgress(inProgress); },
          [this]() { m_directMediaNavigationCoordinator.refresh(m_owner); },
          [this]() { cacheDisplayedMediaPredecodeImages(); },
          [this]() { publishActiveNavigationForImagePages(); },
          [this]() { recomputePublicProjection(); },
      })
    , m_videoDocument(ports.videoDocument)
    , m_videoDocumentCommandRuntime(std::move(videoCommands),
          [this](const DocumentSessionVideoOutputAttachmentPort& attachmentPort) {
              m_videoOutputRuntime.clearAttachment(attachmentPort);
          })
    , m_state(ports.state)
    , m_navigationSourceFacts(std::move(dependencies.navigationSourceFacts))
    , m_videoDocumentSyncRuntime(DocumentSessionVideoDocumentSyncRuntimePorts {
          [this]() {
              const bool changed = m_state.clearDirectMediaCursor();
              if (changed) {
                  syncMediaPredecodeScope();
              }
          },
          [this](const QUrl& url) { m_state.setSourceIdentity(url); },
          [this](DocumentSessionKind kind) { setDocumentKind(kind); },
          [this]() { m_state.setDirectMediaNavigation({}, false, {}); },
          [this](const QUrl& url) {
              const bool changed = m_state.setDirectVideoCursor(
                  NavigationSourceResolver(m_navigationSourceFacts).resolve(url));
              if (changed) {
                  syncMediaPredecodeScope();
              }
              return changed;
          },
          [this]() { m_directMediaNavigationCoordinator.refresh(m_owner); },
          [this]() { recomputePublicProjection(); },
      })
    , m_directMediaScopePort(&m_state)
    , m_directMediaActivityPort(&m_state, &m_directMediaScopePort)
    , m_directMediaNavigationInputPort(&m_state)
    , m_projectionRuntime(DocumentSessionProjectionRuntimePorts {
          [this](const DocumentSessionPublicSnapshotInput& input) {
              return m_state.updatePublicSnapshot(input);
          },
          [this](const DocumentSessionPublicSnapshotInput& input,
              ActiveNavigationSourceKind sourceKind) {
              return m_state.updatePublicSnapshotForSourceKind(input, sourceKind);
          },
          [this]() { return m_state.activeNavigationSourceKind(); },
          [this]() { return m_state.activeNavigationSnapshot(); },
          [this]() -> const DirectMediaNavigationCandidateSnapshot& {
              return m_state.directMediaNavigationCandidateSnapshot();
          },
          [this](std::vector<ActiveNavigationThumbnailRow> rows) {
              m_activeNavigationThumbnailRuntime.setRows(std::move(rows));
          },
          [this](int currentNumber) {
              m_activeNavigationThumbnailRuntime.setCurrentNumber(currentNumber);
          },
          [this]() { clearActiveNavigationRevealContextIfUnavailable(); },
      })
    , m_routeRuntime(DocumentSessionRouteRuntimePorts {
          DocumentSessionRouteSessionPorts {
              [this]() { cancelMediaOpenWith(); },
              [this]() { m_state.setSessionErrorString(QString()); },
              [this](const std::function<void()>& mutation) {
                  QScopedValueRollback<bool> routingSource(m_routingSource, true);
                  mutation();
              },
              [this]() { clearActiveNavigationRevealContextIfUnavailable(); },
          },
          DocumentSessionRouteDirectMediaPorts {
              [this]() { m_directMediaNavigationCoordinator.cancel(); },
              [this]() { cancelMediaDeletion(); },
              [this]() { m_state.setDirectMediaNavigation({}, false, {}); },
              [this]() {
                  const bool changed = m_state.clearDirectMediaCursor();
                  logDirectMediaScope("direct media cursor cleared", m_state.directMediaScope());
                  return changed;
              },
              [this](const QUrl&) {
                  const bool changed = m_state.setDirectVideoCursor(m_routeNavigationSource);
                  logDirectMediaScope("direct video cursor set", m_state.directMediaScope());
                  return changed;
              },
              [this](const QUrl&) {
                  const bool changed = m_state.requestDirectImageCursor(m_routeNavigationSource);
                  logDirectMediaScope("direct image cursor requested", m_state.directMediaScope());
                  return changed;
              },
              [this]() {
                  const bool changed = m_imageDocumentSyncRuntime.syncDirectImageCursor(
                      m_state.documentKind(), m_state.directMediaCursor(), m_imagePublicSnapshot);
                  logDirectMediaScope(
                      "direct image cursor synced from document", m_state.directMediaScope());
                  return changed;
              },
              [this]() { return m_directMediaActivityPort.navigationActive(); },
              [this]() { m_directMediaNavigationCoordinator.refresh(m_owner); },
          },
          DocumentSessionRouteDocumentPorts {
              [this]() {
                  m_state.setOpenedCollectionVideoActive(false);
                  m_imageDocumentCommandRuntime.clearSourceUrl();
                  refreshImagePublicSnapshot();
              },
              [this]() {
                  leaveVideoMode();
                  refreshVideoPublicSnapshot();
              },
              [this]() {
                  m_state.setOpenedCollectionVideoActive(false);
                  setDocumentKind(DocumentSessionKind::Empty);
              },
              [this](const QUrl&) {
                  m_state.setOpenedCollectionVideoActive(false);
                  m_imageDocumentCommandRuntime.setSource(m_routeNavigationSource);
                  refreshImagePublicSnapshot();
                  setDocumentKind(DocumentSessionKind::Image);
              },
              [this](const QUrl&) {
                  m_state.setOpenedCollectionVideoActive(false);
                  m_imageDocumentCommandRuntime.setSameScopeImageNavigationSource(
                      m_routeNavigationSource);
                  refreshImagePublicSnapshot();
                  setDocumentKind(DocumentSessionKind::Image);
              },
              [this](const QUrl& url) {
                  m_state.setOpenedCollectionVideoActive(false);
                  m_videoDocumentCommandRuntime.setSourceUrl(url);
                  refreshVideoPublicSnapshot();
                  setDocumentKind(DocumentSessionKind::Video);
              },
          },
          DocumentSessionRouteSourceIdentityPorts {
              [this]() { m_state.setSourceIdentity(QUrl()); },
              [this](const QUrl& url) { m_state.setSourceIdentity(url); },
              [this]() { m_state.setSourceIdentity(m_imagePublicSnapshot.sourceUrl); },
          },
          DocumentSessionRouteFollowUpPorts {
              [this]() { recomputePublicProjection(); },
              [this]() { syncMediaPredecodeScope(); },
          },
      })
    , m_activeNavigationRuntime(DocumentSessionActiveNavigationRuntimePorts {
          [this](ActiveNavigationRevealContext context) {
              applyActiveNavigationRevealContext(context);
          },
          [this]() { recomputePublicProjection(); },
          [this]() { m_directMediaNavigationCoordinator.openPrevious(m_owner); },
          [this]() { m_directMediaNavigationCoordinator.openNext(m_owner); },
          [this](int number) { m_directMediaNavigationCoordinator.openAtNumber(m_owner, number); },
          [this]() { m_imageDocumentCommandRuntime.openPreviousPage(); },
          [this]() { m_imageDocumentCommandRuntime.openNextPage(); },
          [this](int number) { m_imageDocumentCommandRuntime.openImageAtPage(number); },
      })
    , m_activeNavigationThumbnailRuntime(
          owner, &m_imageDocument, std::move(dependencies.activeNavigationThumbnails))
    , m_directMediaNavigationCoordinator(dependencies.directMediaNavigationCandidateProvider,
          DocumentSessionDirectMediaNavigationCoordinatorPorts {
              [this]() { return m_directMediaActivityPort.navigationActive(); },
              [this]() { return m_directMediaActivityPort.directImageSourceScopeEligible(); },
              [this]() { return m_directMediaScopePort.currentScope(); },
              [this](const DirectMediaScope& scope) {
                  return m_directMediaScopePort.cursorMatches(scope);
              },
              [this]() { return m_directMediaScopePort.activeCursorUrl(); },
              [this]() { return m_state.activeNavigationSourceKind(); },
              [this]() { return m_state.activeNavigationSnapshot(); },
              [this](DirectMediaNavigationBoundaryState state, bool known,
                  std::vector<DirectMediaNavigationCandidate> candidates) {
                  m_state.setDirectMediaNavigation(state, known, std::move(candidates));
              },
              [this](DocumentSessionDirectMediaNavigationRevealAction action) {
                  applyDirectMediaNavigationRevealAction(action);
              },
              [this]() { recomputePublicProjection(); },
              [this](const QUrl& targetUrl) {
                  m_mediaPredecodeRuntime.schedule(m_mediaPredecodeInputPort.currentInput(),
                      targetUrl, m_state.directMediaNavigationCandidateSnapshot());
              },
              [this](const QUrl& url) { openMediaUrl(url); },
          })
    , m_mediaDeletionRuntime(std::move(dependencies.fileDeletionProvider),
          std::move(dependencies.directMediaNavigationCandidateProvider))
    , m_mediaDeletionCompletionRuntime(DocumentSessionMediaDeletionCompletionRuntimePorts {
          [this](bool inProgress) { m_state.setFileDeletionInProgress(inProgress); },
          [this](const QString& errorString) { m_state.setSessionErrorString(errorString); },
          [this]() { recomputePublicProjection(); },
          [this](const DocumentSessionRoutePlan& plan) { executeRoutePlan(plan); },
      })
    , m_mediaOpenWithRuntime(std::move(dependencies.mediaOpenWithProvider))
    , m_mediaPredecodeRuntime(owner, std::move(dependencies.directMediaPredecodeDependencies))
    , m_imagePublicSnapshot(ports.imagePublicSnapshot)
    , m_videoPublicSnapshot(ports.videoPublicSnapshot)
    , m_publicSnapshotInputPort(&m_state, &m_directMediaActivityPort,
          &m_directMediaNavigationInputPort, &m_imagePublicSnapshot, &m_videoPublicSnapshot)
    , m_mediaPredecodeInputPort(
          &m_state, &m_directMediaActivityPort, &m_directMediaScopePort, &m_imagePublicSnapshot)
    , m_mediaOpenWithPlanPort(&m_state, &m_imagePublicSnapshot, &m_videoPublicSnapshot)
{
    refreshLeafPublicSnapshots();
    connectDocuments();
}

DocumentSessionRuntimeGraph::~DocumentSessionRuntimeGraph()
{
    for (const QMetaObject::Connection& connection : m_documentConnections) {
        QObject::disconnect(connection);
    }
    m_mediaDeletionRuntime.cancel();
    cancelMediaOpenWith();
    m_mediaPredecodeRuntime.cancel();
}

QUrl DocumentSessionRuntimeGraph::sourceUrl() const { return m_state.publicSnapshot().sourceUrl; }

void DocumentSessionRuntimeGraph::setSourceUrl(const QUrl& sourceUrl) { routeSourceUrl(sourceUrl); }

DocumentSessionKind DocumentSessionRuntimeGraph::documentKind() const
{
    return m_state.publicSnapshot().documentKind;
}

quint64 DocumentSessionRuntimeGraph::publicProjectionRevision() const
{
    return m_state.publicSnapshot().revision;
}

QString DocumentSessionRuntimeGraph::errorString() const
{
    return m_state.publicSnapshot().errorString;
}

QString DocumentSessionRuntimeGraph::windowTitleSubject() const
{
    return m_state.windowTitleSubject();
}

bool DocumentSessionRuntimeGraph::displayedFileDeletionAvailable() const
{
    return m_state.publicSnapshot().projection.displayedFileDeletionAvailable;
}

bool DocumentSessionRuntimeGraph::displayedMediaOpenWithAvailable() const
{
    return m_state.publicSnapshot().projection.displayedMediaOpenWithAvailable;
}

bool DocumentSessionRuntimeGraph::fileDeletionInProgress() const
{
    return m_state.publicSnapshot().fileDeletionInProgress;
}

const MediaInformationProjectionSnapshot&
DocumentSessionRuntimeGraph::mediaInformationSnapshot() const
{
    return m_state.mediaInformationSnapshot();
}

bool DocumentSessionRuntimeGraph::activeZoomPercentAvailable() const
{
    return m_state.publicSnapshot().activeZoom.available;
}

bool DocumentSessionRuntimeGraph::activeZoomPercentKnown() const
{
    return m_state.publicSnapshot().activeZoom.known;
}

qreal DocumentSessionRuntimeGraph::activeZoomPercent() const
{
    return m_state.publicSnapshot().activeZoom.percent;
}

bool DocumentSessionRuntimeGraph::activeZoomEditable() const
{
    return m_state.publicSnapshot().activeZoom.editable;
}

bool DocumentSessionRuntimeGraph::activeImageReady() const
{
    return m_state.publicSnapshot().activeImageReady;
}

bool DocumentSessionRuntimeGraph::activeImageUnsupportedOpenedCollectionVideo() const
{
    return m_state.publicSnapshot().activeImageUnsupportedOpenedCollectionVideo;
}

bool DocumentSessionRuntimeGraph::activeImageOpenedCollectionScopeActive() const
{
    return m_state.publicSnapshot().activeImageOpenedCollectionScopeActive;
}

bool DocumentSessionRuntimeGraph::activeImageRightToLeftReadingActive() const
{
    return m_state.publicSnapshot().activeImageRightToLeftReadingActive;
}

bool DocumentSessionRuntimeGraph::activeVideoReady() const
{
    return m_state.publicSnapshot().activeVideoReady;
}

bool DocumentSessionRuntimeGraph::activeVideoControlsReady() const
{
    return m_state.publicSnapshot().activeVideoControlsReady;
}

const DocumentSessionActionAvailabilityFacts&
DocumentSessionRuntimeGraph::actionAvailabilityFacts() const
{
    return m_state.publicSnapshot().actionAvailability;
}

bool DocumentSessionRuntimeGraph::activeNavigationAvailable() const
{
    return m_state.activeNavigationSnapshot().available;
}

bool DocumentSessionRuntimeGraph::activeNavigationKnown() const
{
    return m_state.activeNavigationSnapshot().known;
}

bool DocumentSessionRuntimeGraph::activeNavigationEditable() const
{
    return m_state.activeNavigationSnapshot().editable;
}

bool DocumentSessionRuntimeGraph::activeNavigationHasTargets() const
{
    return m_state.activeNavigationSnapshot().count > 0;
}

bool DocumentSessionRuntimeGraph::activeNavigationDispatchAvailable() const
{
    const ActiveNavigationSnapshot& snapshot = m_state.activeNavigationSnapshot();
    return snapshot.available && snapshot.known && snapshot.count > 0
        && !m_state.fileDeletionInProgress();
}

int DocumentSessionRuntimeGraph::activeNavigationCurrentNumber() const
{
    return m_state.activeNavigationSnapshot().currentNumber;
}

int DocumentSessionRuntimeGraph::activeNavigationCount() const
{
    return m_state.activeNavigationSnapshot().count;
}

bool DocumentSessionRuntimeGraph::canOpenPreviousActiveNavigation() const
{
    return m_state.activeNavigationSnapshot().canOpenPrevious;
}

bool DocumentSessionRuntimeGraph::canOpenNextActiveNavigation() const
{
    return m_state.activeNavigationSnapshot().canOpenNext;
}

bool DocumentSessionRuntimeGraph::atKnownFirstActiveNavigation() const
{
    return m_state.activeNavigationSnapshot().atKnownFirst;
}

bool DocumentSessionRuntimeGraph::atKnownLastActiveNavigation() const
{
    return m_state.activeNavigationSnapshot().atKnownLast;
}

bool DocumentSessionRuntimeGraph::directMediaNavigationBoundaryActive() const
{
    return m_state.activeNavigationBoundaryScope() == ActiveNavigationBoundaryScope::DirectMedia;
}

ActiveNavigationBoundaryScope DocumentSessionRuntimeGraph::activeNavigationBoundaryScope() const
{
    return m_state.activeNavigationBoundaryScope();
}

ActiveNavigationRevealIntent DocumentSessionRuntimeGraph::activeNavigationRevealIntent() const
{
    return m_state.publicSnapshot().activeNavigationRevealIntent;
}

ActiveNavigationRevealDirection DocumentSessionRuntimeGraph::activeNavigationRevealDirection() const
{
    return m_state.publicSnapshot().activeNavigationRevealDirection;
}

QAbstractListModel* DocumentSessionRuntimeGraph::activeNavigationThumbnailModel() const
{
    return m_activeNavigationThumbnailRuntime.model();
}

bool DocumentSessionRuntimeGraph::replaceActiveNavigationThumbnailDemandSnapshot(
    ActiveNavigationThumbnailDemandSnapshot snapshot)
{
    return m_activeNavigationThumbnailRuntime.replaceDemandSnapshot(std::move(snapshot));
}

QString DocumentSessionRuntimeGraph::nextVideoOutputSurfaceClaimToken()
{
    return m_videoOutputRuntime.nextSurfaceClaimToken();
}

bool DocumentSessionRuntimeGraph::reportVideoOutputSurfaceClaim(const QString& claimToken,
    quint64 projectionRevision, QObject* surfaceOwner, QObject* videoOutput, bool active,
    const QRectF& contentRect, const QRectF& sourceRect)
{
    const bool currentProjection = projectionRevision == m_state.publicSnapshot().revision;
    if (!currentProjection) {
        return false;
    }

    const bool attach = active
        && m_state.publicSnapshot().documentKind == DocumentSessionKind::Video
        && videoOutput != nullptr;

    return m_videoOutputRuntime.reportSurfaceClaim(
        { claimToken, surfaceOwner, videoOutput, attach, contentRect, sourceRect },
        videoOutputAttachmentPort());
}

std::optional<PredecodedImage> DocumentSessionRuntimeGraph::findPredecodedImage(
    const QUrl& url) const
{
    return m_mediaPredecodeRuntime.findPredecodedImage(url);
}

void DocumentSessionRuntimeGraph::applyDirectMediaNavigationRevealAction(
    DocumentSessionDirectMediaNavigationRevealAction action)
{
    switch (action) {
    case DocumentSessionDirectMediaNavigationRevealAction::None:
        return;
    case DocumentSessionDirectMediaNavigationRevealAction::Clear:
        takePendingActiveNavigationRevealContext(ActiveNavigationRevealIntent::None);
        setActiveNavigationRevealContext({});
        return;
    case DocumentSessionDirectMediaNavigationRevealAction::ProgrammaticSync:
        setActiveNavigationRevealContext(
            ActiveNavigationRevealContext { ActiveNavigationRevealIntent::ProgrammaticSync });
        return;
    case DocumentSessionDirectMediaNavigationRevealAction::UsePendingOrProgrammaticSync:
        setActiveNavigationRevealContext(takePendingActiveNavigationRevealContext(
            ActiveNavigationRevealIntent::ProgrammaticSync));
        return;
    case DocumentSessionDirectMediaNavigationRevealAction::
        UsePendingOrProgrammaticSyncAndKeepPending: {
        const ActiveNavigationRevealContext context = takePendingActiveNavigationRevealContext(
            ActiveNavigationRevealIntent::ProgrammaticSync);
        setActiveNavigationRevealContext(context);
        setPendingActiveNavigationRevealContext(context);
        return;
    }
    }
}

void DocumentSessionRuntimeGraph::openPreviousActiveNavigation()
{
    requestPreviousActiveNavigation();
}

void DocumentSessionRuntimeGraph::openNextActiveNavigation() { requestNextActiveNavigation(); }

void DocumentSessionRuntimeGraph::openFirstActiveNavigation()
{
    executeActiveNavigationDispatchRequest(firstActiveNavigationDispatchRequest(),
        ActiveNavigationRevealContext { ActiveNavigationRevealIntent::LargeJump });
}

void DocumentSessionRuntimeGraph::openLastActiveNavigation()
{
    executeActiveNavigationDispatchRequest(lastActiveNavigationDispatchRequest(),
        ActiveNavigationRevealContext { ActiveNavigationRevealIntent::LargeJump });
}

void DocumentSessionRuntimeGraph::openActiveNavigationAtNumber(int number)
{
    executeActiveNavigationDispatchRequest(numberedActiveNavigationDispatchRequest(number),
        ActiveNavigationRevealContext { ActiveNavigationRevealIntent::LargeJump });
}

void DocumentSessionRuntimeGraph::openActiveNavigationThumbnailAtNumber(int number)
{
    executeActiveNavigationDispatchRequest(numberedActiveNavigationDispatchRequest(number),
        ActiveNavigationRevealContext { ActiveNavigationRevealIntent::ThumbnailActivation });
}

ActiveNavigationDispatchOutcome DocumentSessionRuntimeGraph::requestPreviousActiveNavigation()
{
    return executeActiveNavigationDispatchRequest(previousActiveNavigationDispatchRequest(),
        ActiveNavigationRevealContext { ActiveNavigationRevealIntent::AdjacentNavigation,
            ActiveNavigationRevealDirection::Previous });
}

ActiveNavigationDispatchOutcome DocumentSessionRuntimeGraph::requestNextActiveNavigation()
{
    return executeActiveNavigationDispatchRequest(nextActiveNavigationDispatchRequest(),
        ActiveNavigationRevealContext { ActiveNavigationRevealIntent::AdjacentNavigation,
            ActiveNavigationRevealDirection::Next });
}

ActiveNavigationDispatchOutcome DocumentSessionRuntimeGraph::executeActiveNavigationDispatchRequest(
    ActiveNavigationDispatchRequest request, ActiveNavigationRevealContext context)
{
    return m_activeNavigationRuntime.dispatch(
        m_state.activeNavigationSourceKind(), m_state.activeNavigationSnapshot(), request, context);
}

void DocumentSessionRuntimeGraph::setPendingActiveNavigationRevealContext(
    ActiveNavigationRevealContext context)
{
    m_activeNavigationRuntime.setPendingRevealContext(context);
}

ActiveNavigationRevealContext DocumentSessionRuntimeGraph::takePendingActiveNavigationRevealContext(
    ActiveNavigationRevealIntent fallbackIntent)
{
    return m_activeNavigationRuntime.takePendingRevealContext(fallbackIntent);
}

void DocumentSessionRuntimeGraph::setActiveNavigationRevealContext(
    ActiveNavigationRevealContext context)
{
    m_activeNavigationRuntime.setRevealContext(context);
}

void DocumentSessionRuntimeGraph::applyActiveNavigationRevealContext(
    ActiveNavigationRevealContext context)
{
    m_state.setActiveNavigationRevealIntent(context.intent);
    m_state.setActiveNavigationRevealDirection(context.direction);
}

void DocumentSessionRuntimeGraph::clearActiveNavigationRevealContextIfUnavailable()
{
    m_activeNavigationRuntime.clearRevealContextIfUnavailable(m_state.activeNavigationSnapshot());
}

void DocumentSessionRuntimeGraph::deleteDisplayedFile(FileDeletionMode mode)
{
    if (m_state.documentKind() == DocumentSessionKind::Video
        && m_state.openedCollectionVideoActive()) {
        if (!displayedFileDeletionAvailable()) {
            return;
        }

        m_imageDocumentCommandRuntime.deleteDisplayedFile(mode);
        syncImageDocumentFileDeletionProgress();
        return;
    }

    if (m_state.documentKind() == DocumentSessionKind::Image
        && !m_directMediaActivityPort.directImageSourceScopeEligible()) {
        m_imageDocumentCommandRuntime.deleteDisplayedFile(mode);
        syncImageDocumentFileDeletionProgress();
        return;
    }

    if (!displayedFileDeletionAvailable() || !m_directMediaActivityPort.navigationActive()) {
        return;
    }

    m_state.setFileDeletionInProgress(true);
    recomputePublicProjection();
    const bool started = m_mediaDeletionRuntime.startForDirectMedia(
        m_owner, mode, m_directMediaScopePort.currentScope(),
        [this](
            const DirectMediaScope& scope) { return m_directMediaScopePort.cursorMatches(scope); },
        m_state.documentKind(),
        [this](DocumentSessionMediaDeletionCompletion completion) {
            finishMediaDeletion(std::move(completion));
        });
    if (!started) {
        m_state.setFileDeletionInProgress(false);
        recomputePublicProjection();
    }
}

void DocumentSessionRuntimeGraph::openCurrentMediaWith(MediaOpenWithCallback callback)
{
    m_mediaOpenWithRuntime.open(
        m_owner, m_mediaOpenWithPlanPort.currentPlan(), std::move(callback));
}

void DocumentSessionRuntimeGraph::connectDocuments()
{
    appendConnection(m_documentConnections, m_imageDocument.snapshotChanged, m_owner,
        [this]() { handleImageDocumentSnapshotChanged(); });
    appendConnection(m_documentConnections, m_videoDocument.snapshotChanged, m_owner,
        [this]() { handleVideoDocumentSnapshotChanged(); });
}

void DocumentSessionRuntimeGraph::handleImageDocumentSnapshotChanged()
{
    const ImageDocumentPageActiveNavigationSnapshot previousPageNavigation
        = m_imagePublicSnapshot.pageNavigation;
    refreshImagePublicSnapshot();
    if (tryEnterOpenedCollectionVideoFromImageSnapshot()) {
        return;
    }
    if (tryReturnToImageDocumentFromOpenedCollectionVideo()) {
        return;
    }
    if (tryClearOpenedCollectionVideoAfterImageDocumentCleared()) {
        return;
    }
    syncImageDocumentFileDeletionProgress();
    m_imageDocumentSyncRuntime.sync(DocumentSessionImageDocumentSyncRuntimeInput {
        m_routingSource,
        m_state.documentKind(),
        m_directMediaActivityPort.directImageSourceScopeEligible(),
        m_directMediaActivityPort.navigationActive(),
        m_state.directMediaNavigationKnown(),
        m_state.directMediaCursor(),
        previousPageNavigation,
        m_imagePublicSnapshot,
    });
}

void DocumentSessionRuntimeGraph::handleVideoDocumentSnapshotChanged()
{
    refreshVideoPublicSnapshot();
    if (m_routingSource) {
        return;
    }

    m_videoDocumentSyncRuntime.sync(DocumentSessionVideoDocumentSyncRuntimeInput {
        m_state.documentKind(),
        m_videoPublicSnapshot,
        m_state.openedCollectionVideoActive(),
    });
}

bool DocumentSessionRuntimeGraph::tryEnterOpenedCollectionVideoFromImageSnapshot()
{
    if (m_routingSource || m_imagePublicSnapshot.sourceKind != ImageDocumentPageKind::Video
        || m_imagePublicSnapshot.unsupportedOpenedCollectionVideo
        || !m_imagePublicSnapshot.readyForInformation
        || m_imagePublicSnapshot.displayedOpenedCollectionScope.isEmpty()
        || m_imagePublicSnapshot.sourceUrl.isEmpty()
        || m_imagePublicSnapshot.displayedUrl != m_imagePublicSnapshot.sourceUrl) {
        return false;
    }

    if (m_state.openedCollectionVideoActive()
        && m_state.documentKind() == DocumentSessionKind::Video
        && m_state.sourceUrl() == m_imagePublicSnapshot.sourceUrl) {
        return false;
    }

    MediaEntrySourceVideoPlaybackDeviceResult result
        = m_imageDocumentCommandRuntime.loadOpenedCollectionVideoPlaybackDevice(
            m_imagePublicSnapshot.displayedOpenedCollectionScope, m_imagePublicSnapshot.sourceUrl);
    auto* playbackDevice = std::get_if<MediaEntrySourceVideoPlaybackDevice>(&result);
    if (playbackDevice == nullptr || playbackDevice->device == nullptr) {
        return false;
    }

    enterOpenedCollectionVideoDocument(m_imagePublicSnapshot.sourceUrl,
        videoPlaybackSourceDeviceFromMediaEntryDevice(std::move(*playbackDevice)));
    return true;
}

bool DocumentSessionRuntimeGraph::tryReturnToImageDocumentFromOpenedCollectionVideo()
{
    if (m_routingSource || !m_state.openedCollectionVideoActive()
        || m_state.documentKind() != DocumentSessionKind::Video
        || m_imagePublicSnapshot.sourceKind == ImageDocumentPageKind::Video
        || !m_imagePublicSnapshot.readyForInformation
        || m_imagePublicSnapshot.sourceUrl.isEmpty()) {
        return false;
    }

    leaveVideoMode();
    refreshVideoPublicSnapshot();
    m_state.setOpenedCollectionVideoActive(false);
    m_state.setSourceIdentity(m_imagePublicSnapshot.sourceUrl);
    m_state.setFileDeletionInProgress(m_imagePublicSnapshot.fileDeletionInProgress);
    setDocumentKind(DocumentSessionKind::Image);
    publishActiveNavigationForImagePages();
    return true;
}

bool DocumentSessionRuntimeGraph::tryClearOpenedCollectionVideoAfterImageDocumentCleared()
{
    if (m_routingSource || !m_state.openedCollectionVideoActive()
        || m_state.documentKind() != DocumentSessionKind::Video
        || !m_imagePublicSnapshot.sourceUrl.isEmpty() || m_imagePublicSnapshot.readyForInformation
        || m_imagePublicSnapshot.error || m_imagePublicSnapshot.fileDeletionInProgress) {
        return false;
    }

    leaveVideoMode();
    refreshVideoPublicSnapshot();
    m_state.setOpenedCollectionVideoActive(false);
    m_state.setSourceIdentity({});
    m_state.setFileDeletionInProgress(false);
    setDocumentKind(DocumentSessionKind::Empty);
    recomputePublicProjection();
    clearActiveNavigationRevealContextIfUnavailable();
    return true;
}

void DocumentSessionRuntimeGraph::enterOpenedCollectionVideoDocument(
    const QUrl& sourceUrl, VideoPlaybackSourceDevice sourceDevice)
{
    cancelMediaOpenWith();
    cancelMediaDeletion();
    m_state.setDirectMediaNavigation({}, false, {});
    const bool directMediaScopeChanged = m_state.clearDirectMediaCursor();
    if (directMediaScopeChanged) {
        syncMediaPredecodeScope();
    }
    leaveVideoMode();
    refreshVideoPublicSnapshot();

    m_state.setOpenedCollectionVideoActive(true);
    m_state.setSourceIdentity(sourceUrl);
    m_videoDocumentCommandRuntime.setSourceDevice(sourceUrl, std::move(sourceDevice));
    refreshVideoPublicSnapshot();
    setDocumentKind(DocumentSessionKind::Video);
    recomputePublicProjection();
    clearActiveNavigationRevealContextIfUnavailable();
}

void DocumentSessionRuntimeGraph::refreshImagePublicSnapshot()
{
    m_imagePublicSnapshot = buildDocumentSessionPublicImageLeafSnapshot(m_imageDocument.snapshot());
}

void DocumentSessionRuntimeGraph::refreshVideoPublicSnapshot()
{
    m_videoPublicSnapshot = buildDocumentSessionPublicVideoLeafSnapshot(m_videoDocument.snapshot());
}

void DocumentSessionRuntimeGraph::refreshLeafPublicSnapshots()
{
    refreshImagePublicSnapshot();
    refreshVideoPublicSnapshot();
}

void DocumentSessionRuntimeGraph::syncImageDocumentFileDeletionProgress()
{
    const bool imageDeletionOwnsProgress
        = (m_state.documentKind() == DocumentSessionKind::Image
              && !m_directMediaActivityPort.directImageSourceScopeEligible())
        || (m_state.documentKind() == DocumentSessionKind::Video
            && m_state.openedCollectionVideoActive());
    if (!imageDeletionOwnsProgress) {
        return;
    }

    m_state.setFileDeletionInProgress(m_imagePublicSnapshot.fileDeletionInProgress);
    recomputePublicProjection();
}

void DocumentSessionRuntimeGraph::setDocumentKind(DocumentSessionKind kind)
{
    m_state.setDocumentKindAndActiveZoomSnapshot(kind, activeZoomSnapshotForKind(kind));
}

void DocumentSessionRuntimeGraph::publishActiveNavigationForImagePages()
{
    setActiveNavigationRevealContext(
        takePendingActiveNavigationRevealContext(ActiveNavigationRevealIntent::ProgrammaticSync));
    m_projectionRuntime.publishForSourceKind(m_publicSnapshotInputPort.nextInput(),
        ActiveNavigationSourceKind::ImageDocumentPages,
        m_imagePublicSnapshot.pageCandidateSnapshot);
}

void DocumentSessionRuntimeGraph::recomputePublicProjection()
{
    m_projectionRuntime.publish(
        m_publicSnapshotInputPort.nextInput(), m_imagePublicSnapshot.pageCandidateSnapshot);
}

void DocumentSessionRuntimeGraph::routeSourceUrl(const QUrl& sourceUrl)
{
    setPendingActiveNavigationRevealContext(
        ActiveNavigationRevealContext { ActiveNavigationRevealIntent::LoadOrOpen });
    executeRoutePlan(documentSessionRoutePlanForSourceUrl(sourceUrl, m_state.documentKind()));
}

void DocumentSessionRuntimeGraph::openMediaUrl(const QUrl& url)
{
    executeRoutePlan(documentSessionRoutePlanForMediaUrl(url, m_state.documentKind()));
}

void DocumentSessionRuntimeGraph::executeRoutePlan(const DocumentSessionRoutePlan& plan)
{
    m_routeNavigationSource = plan.sourceUrl.isEmpty()
        ? ResolvedNavigationSource {}
        : NavigationSourceResolver(m_navigationSourceFacts).resolve(plan.sourceUrl);
    qCDebug(kiriviewNavigationLog)
        << "execute route plan"
        << "routeKind" << routeKindName(plan.kind) << "sourceUrl" << plan.sourceUrl
        << "documentKindBefore" << documentKindName(m_state.documentKind());
    m_routeRuntime.execute(plan);
    qCDebug(kiriviewNavigationLog)
        << "execute route plan complete"
        << "routeKind" << routeKindName(plan.kind) << "documentKindAfter"
        << documentKindName(m_state.documentKind()) << "sourceUrl" << m_state.sourceUrl()
        << "activeNavigationAvailable" << m_state.activeNavigationSnapshot().available
        << "activeNavigationKnown" << m_state.activeNavigationSnapshot().known
        << "activeNavigationCurrent" << m_state.activeNavigationSnapshot().currentNumber
        << "activeNavigationCount" << m_state.activeNavigationSnapshot().count;
}

void DocumentSessionRuntimeGraph::leaveVideoMode()
{
    m_videoDocumentCommandRuntime.leaveMode(m_videoPublicSnapshot.sourceUrl);
}

void DocumentSessionRuntimeGraph::syncMediaPredecodeScope()
{
    m_mediaPredecodeRuntime.syncScope(m_mediaPredecodeInputPort.currentInput());
}

void DocumentSessionRuntimeGraph::cacheDisplayedMediaPredecodeImages()
{
    m_mediaPredecodeRuntime.cacheDisplayedImages(m_mediaPredecodeInputPort.currentInput());
}

void DocumentSessionRuntimeGraph::cancelMediaDeletion()
{
    const bool sessionMediaDeletionInProgress
        = m_state.fileDeletionInProgress() && m_directMediaActivityPort.navigationActive();
    if (!m_mediaDeletionRuntime.active() && !sessionMediaDeletionInProgress) {
        return;
    }

    m_mediaDeletionRuntime.cancel();
    m_state.setFileDeletionInProgress(false);
    recomputePublicProjection();
}

void DocumentSessionRuntimeGraph::cancelMediaOpenWith() { m_mediaOpenWithRuntime.cancel(); }

DocumentSessionVideoOutputAttachmentPort DocumentSessionRuntimeGraph::videoOutputAttachmentPort()
{
    return m_videoDocumentCommandRuntime.outputAttachmentPort();
}

void DocumentSessionRuntimeGraph::finishMediaDeletion(
    DocumentSessionMediaDeletionCompletion completion)
{
    m_mediaDeletionCompletionRuntime.apply(completion);
}

ActiveZoomSnapshot DocumentSessionRuntimeGraph::activeZoomSnapshotForKind(
    DocumentSessionKind kind) const
{
    return documentSessionActiveZoomSnapshot(kind, m_imagePublicSnapshot, m_videoPublicSnapshot);
}

}
