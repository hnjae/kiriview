// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionruntimegraph.h"

#include "localization/imageerrortext.h"
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

void logDirectMediaScope(
    const char* message, const std::optional<kiriview::DirectMediaScope>& scope)
{
    qCDebug(kiriviewNavigationLog)
        << message << "currentUrl" << (scope.has_value() ? scope->currentUrl() : QUrl())
        << "parentUrl" << (scope.has_value() ? scope->parentUrl() : QUrl()) << "generation"
        << (scope.has_value() ? scope->generation() : 0);
}

void logMediaEntrySourceError(const char* message, const kiriview::MediaEntrySourceError& error)
{
    qCWarning(kiriviewNavigationLog).noquote() << message << error;
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

QString fileDeletionErrorMessage(const kiriview::KioOperationFailure& failure)
{
    return failure.userMessage.isEmpty()
        ? kiriview::imageErrorText(kiriview::ImageErrorTextId::DeleteFile)
        : failure.userMessage;
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
          [this](const QUrl& url) { return m_state.confirmDirectImageCursor(url); },
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
          [this, lifetime = std::weak_ptr<void>(m_callbackLifetime)]() {
              if (!lifetime.expired()) {
                  m_videoOutputRuntime.retireSurfaceClaimEpoch();
              }
          })
    , m_state(ports.state)
    , m_navigationSourceResolver(dependencies.navigationSourceResolver.has_value()
              ? std::move(*dependencies.navigationSourceResolver)
              : NavigationSourceResolver())
    , m_videoDocumentSyncRuntime(DocumentSessionVideoDocumentSyncRuntimePorts {
          [this]() {
              const bool changed = m_state.clearDirectMediaCursor();
              if (changed) {
                  syncMediaPredecodeScope();
              }
          },
          [this](const QUrl& url) { m_state.setSourceIdentity(url); },
          [this, lifetime = std::weak_ptr<void>(m_callbackLifetime)](DocumentSessionKind kind) {
              if (lifetime.expired()) {
                  return false;
              }
              setDocumentKind(kind);
              return !lifetime.expired() && m_state.documentKind() == kind;
          },
          [this]() { m_state.setDirectMediaNavigation({}, false, {}); },
          [this](const QUrl& url) {
              return confirmDirectVideoCursor(m_state.directMediaCursor(), url);
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
                  executeWithRoutingSuppressed(mutation);
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
              [this](const ResolvedNavigationSource& source) {
                  const bool changed = m_state.setDirectVideoCursor(source);
                  logDirectMediaScope("direct video cursor set", m_state.directMediaScope());
                  return changed;
              },
              [this](const ResolvedNavigationSource& source) {
                  const bool changed = m_state.requestDirectImageCursor(source);
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
              [this, lifetime = std::weak_ptr<void>(m_callbackLifetime)]() {
                  m_state.setOpenedCollectionVideoActive(false);
                  if (!m_imageDocumentCommandRuntime.clearSourceUrl() || lifetime.expired()) {
                      return;
                  }
                  static_cast<void>(refreshImagePublicSnapshot());
              },
              [this, lifetime = std::weak_ptr<void>(m_callbackLifetime)]() {
                  if (!leaveVideoMode() || lifetime.expired()) {
                      return;
                  }
                  static_cast<void>(refreshVideoPublicSnapshot());
              },
              [this]() {
                  m_state.setOpenedCollectionVideoActive(false);
                  setDocumentKind(DocumentSessionKind::Empty);
              },
              [this, lifetime = std::weak_ptr<void>(m_callbackLifetime)](
                  const ResolvedNavigationSource& source) {
                  m_state.setOpenedCollectionVideoActive(false);
                  if (!m_imageDocumentCommandRuntime.setSource(source) || lifetime.expired()) {
                      return;
                  }
                  if (!refreshImagePublicSnapshot()) {
                      return;
                  }
                  setDocumentKind(DocumentSessionKind::Image);
              },
              [this, lifetime = std::weak_ptr<void>(m_callbackLifetime)](
                  const ResolvedNavigationSource& source) {
                  m_state.setOpenedCollectionVideoActive(false);
                  if (!m_imageDocumentCommandRuntime.setSource(source) || lifetime.expired()) {
                      return;
                  }
                  if (!refreshImagePublicSnapshot()) {
                      return;
                  }
                  setDocumentKind(DocumentSessionKind::Image);
              },
              [this, lifetime = std::weak_ptr<void>(m_callbackLifetime)](
                  const ResolvedNavigationSource& source) {
                  m_state.setOpenedCollectionVideoActive(false);
                  m_videoOutputRuntime.activateSurfaceClaimEpoch();
                  if (!m_videoDocumentCommandRuntime.setSource(source) || lifetime.expired()) {
                      return;
                  }
                  if (!refreshVideoPublicSnapshot()) {
                      return;
                  }
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
              [this]() { return m_directMediaScopePort.currentScope(); },
              [this](const DirectMediaScope& scope) {
                  return m_directMediaScopePort.cursorMatches(scope);
              },
              [this, lifetime = std::weak_ptr<void>(m_callbackLifetime)]() {
                  const quint64 transitionRevision = m_documentTransitionAdmission.current();
                  return [this, lifetime, transitionRevision]() {
                      return !lifetime.expired()
                          && m_documentTransitionAdmission.current() == transitionRevision;
                  };
              },
              [this, lifetime = std::weak_ptr<void>(m_callbackLifetime)]() {
                  const quint64 supersessionRevision
                      = m_directMediaOpenSupersessionAdmission.current();
                  return [this, lifetime, supersessionRevision]() {
                      return !lifetime.expired()
                          && m_directMediaOpenSupersessionAdmission.current()
                          == supersessionRevision;
                  };
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
              [this](const QUrl& url, std::function<bool()> originatingCurrent) {
                  openMediaUrl(url, std::move(originatingCurrent));
              },
          })
    , m_mediaDeletionRuntime(std::move(dependencies.fileDeletionProvider),
          std::move(dependencies.directMediaNavigationCandidateProvider))
    , m_fileDeletionFailed(std::move(dependencies.fileDeletionFailed))
    , m_mediaOpenWithRuntime(std::move(dependencies.mediaOpenWithProvider))
    , m_mediaPredecodeRuntime(std::move(dependencies.directMediaPredecodeDependencies))
    , m_imagePublicSnapshot(ports.imagePublicSnapshot)
    , m_videoPublicSnapshot(ports.videoPublicSnapshot)
    , m_publicSnapshotInputPort(&m_state, &m_directMediaActivityPort,
          &m_directMediaNavigationInputPort, &m_imagePublicSnapshot, &m_videoPublicSnapshot)
    , m_mediaPredecodeInputPort(
          &m_state, &m_directMediaActivityPort, &m_directMediaScopePort, &m_imagePublicSnapshot)
    , m_mediaOpenWithPlanPort(&m_state, &m_imagePublicSnapshot, &m_videoPublicSnapshot)
    , m_videoOutputRuntime(m_videoDocumentCommandRuntime.outputAttachmentPort())
{
    refreshLeafPublicSnapshots();
    connectDocuments();
}

DocumentSessionRuntimeGraph::~DocumentSessionRuntimeGraph()
{
    m_callbackLifetime.reset();
    for (const QMetaObject::Connection& connection : m_documentConnections) {
        QObject::disconnect(connection);
    }
    m_mediaDeletionTransaction.cancel();
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

const DocumentSessionActionStateSnapshot& DocumentSessionRuntimeGraph::actionStateSnapshot() const
{
    return m_state.publicSnapshot().actionState;
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
    const ActiveNavigationThumbnailDemandSnapshot& snapshot)
{
    return m_activeNavigationThumbnailRuntime.replaceDemandSnapshot(snapshot);
}

QString DocumentSessionRuntimeGraph::nextVideoOutputSurfaceClaimToken()
{
    return m_videoOutputRuntime.nextSurfaceClaimToken();
}

bool DocumentSessionRuntimeGraph::reportVideoOutputSurfaceClaim(const QString& claimToken,
    quint64 projectionRevision, QObject* surfaceOwner, QObject* videoOutput, bool active,
    const QRectF& contentRect, const QRectF& sourceRect)
{
    return m_videoOutputRuntime.reportSurfaceClaim(
        { claimToken, surfaceOwner, videoOutput, active, contentRect, sourceRect,
            projectionRevision },
        { m_state.publicSnapshot().revision,
            m_state.publicSnapshot().documentKind == DocumentSessionKind::Video });
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

void DocumentSessionRuntimeGraph::executeWithRoutingSuppressed(
    const std::function<void()>& mutation)
{
    const std::shared_ptr<CallbackState> callbackState = m_callbackState;
    QScopedValueRollback<bool> suppression(callbackState->routingSource, true);
    if (mutation) {
        mutation();
    }
}

void DocumentSessionRuntimeGraph::executeWithVideoLeafSyncSuppressed(
    const std::function<void()>& mutation)
{
    const std::shared_ptr<CallbackState> callbackState = m_callbackState;
    QScopedValueRollback<bool> suppression(callbackState->videoLeafSyncSuppressed, true);
    if (mutation) {
        mutation();
    }
}

void DocumentSessionRuntimeGraph::deleteDisplayedFile(FileDeletionMode mode)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    if (m_state.documentKind() == DocumentSessionKind::Video
        && m_state.openedCollectionVideoActive()) {
        if (!displayedFileDeletionAvailable()) {
            return;
        }

        m_imageDocumentCommandRuntime.deleteDisplayedFile(mode);
        if (lifetime.expired()) {
            return;
        }
        syncImageDocumentFileDeletionProgress();
        return;
    }

    if (m_state.documentKind() == DocumentSessionKind::Image
        && !m_directMediaActivityPort.directImageSourceScopeEligible()) {
        m_imageDocumentCommandRuntime.deleteDisplayedFile(mode);
        if (lifetime.expired()) {
            return;
        }
        syncImageDocumentFileDeletionProgress();
        return;
    }

    if (!displayedFileDeletionAvailable() || !m_directMediaActivityPort.navigationActive()) {
        return;
    }

    const std::optional<DirectMediaScope> scope = m_directMediaScopePort.currentScope();
    if (!scope.has_value() || m_mediaDeletionTransaction.active()
        || m_mediaDeletionRuntime.active()) {
        return;
    }
    const DocumentSessionKind documentKind = m_state.documentKind();
    const ImageAsyncScopedOperation<DirectMediaScope> operation
        = m_mediaDeletionTransaction.start(*scope);
    const std::function<bool()> navigationCancellationCurrent
        = m_directMediaNavigationCoordinator.cancelAndCaptureCurrent();
    const auto current = [this, lifetime, operation, navigationCancellationCurrent]() {
        return !lifetime.expired() && navigationCancellationCurrent()
            && m_mediaDeletionTransaction.accepts(operation)
            && m_directMediaScopePort.cursorMatches(operation.scope);
    };
    if (!current()) {
        if (!lifetime.expired()) {
            static_cast<void>(m_mediaDeletionTransaction.finish(operation));
        }
        return;
    }

    m_state.setFileDeletionInProgress(true);
    recomputePublicProjection();
    if (lifetime.expired()) {
        return;
    }

    if (!current()) {
        if (m_mediaDeletionTransaction.finish(operation)) {
            m_state.setFileDeletionInProgress(false);
            recomputePublicProjection();
        }
        return;
    }

    const bool started = m_mediaDeletionRuntime.startForDirectMedia(
        m_owner, mode, operation.scope,
        [this, operation](const DirectMediaScope& acceptedScope) {
            return acceptedScope == operation.scope && m_mediaDeletionTransaction.accepts(operation)
                && m_directMediaScopePort.cursorMatches(acceptedScope);
        },
        documentKind,
        [this, operation](const DocumentSessionMediaDeletionCompletion& completion) {
            finishMediaDeletion(operation, completion);
        });
    if (lifetime.expired()) {
        return;
    }
    if (!started && m_mediaDeletionTransaction.finish(operation)) {
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
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    appendConnection(
        m_documentConnections, m_imageDocument.snapshotChanged, m_owner, [this, lifetime]() {
            if (!lifetime.expired()) {
                handleImageDocumentSnapshotChanged();
            }
        });
    appendConnection(
        m_documentConnections, m_videoDocument.snapshotChanged, m_owner, [this, lifetime]() {
            if (!lifetime.expired()) {
                handleVideoDocumentSnapshotChanged();
            }
        });
}

void DocumentSessionRuntimeGraph::handleImageDocumentSnapshotChanged()
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const ImageDocumentPageActiveNavigationSnapshot previousPageNavigation
        = m_imagePublicSnapshot.pageNavigation;
    if (!refreshImagePublicSnapshot()) {
        return;
    }
    if (tryEnterOpenedCollectionVideoFromImageSnapshot()) {
        return;
    }
    if (lifetime.expired()) {
        return;
    }
    if (tryReturnToImageDocumentFromOpenedCollectionVideo()) {
        return;
    }
    if (lifetime.expired()) {
        return;
    }
    if (tryClearOpenedCollectionVideoAfterImageDocumentCleared()) {
        return;
    }
    if (lifetime.expired()) {
        return;
    }
    syncImageDocumentFileDeletionProgress();
    if (lifetime.expired()) {
        return;
    }
    m_imageDocumentSyncRuntime.sync(DocumentSessionImageDocumentSyncRuntimeInput {
        m_callbackState->routingSource,
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
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    if (!refreshVideoPublicSnapshot()) {
        return;
    }
    if (m_callbackState->routingSource || m_callbackState->videoLeafSyncSuppressed) {
        return;
    }

    const quint64 transitionRevision = m_documentTransitionAdmission.current();
    m_videoDocumentSyncRuntime.sync(
        DocumentSessionVideoDocumentSyncRuntimeInput {
            m_state.documentKind(),
            m_videoPublicSnapshot,
            m_state.openedCollectionVideoActive(),
        },
        DocumentSessionVideoDocumentSyncRuntimeControl {
            [this, lifetime, transitionRevision]() {
                return !lifetime.expired()
                    && m_documentTransitionAdmission.current() == transitionRevision;
            },
        });
}

bool DocumentSessionRuntimeGraph::tryEnterOpenedCollectionVideoFromImageSnapshot()
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    if (m_callbackState->routingSource
        || m_imagePublicSnapshot.sourceKind != ImageDocumentPageKind::Video
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

    const ImageDocumentPageKind requestedSourceKind = m_imagePublicSnapshot.sourceKind;
    const QUrl requestedSourceUrl = m_imagePublicSnapshot.sourceUrl;
    const QUrl requestedDisplayedUrl = m_imagePublicSnapshot.displayedUrl;
    const OpenedCollectionScopeLocation requestedScope
        = m_imagePublicSnapshot.displayedOpenedCollectionScope;
    m_directMediaOpenSupersessionAdmission.next();
    const quint64 transitionRevision = m_documentTransitionAdmission.next();
    std::optional<MediaEntrySourceVideoPlaybackDeviceResult> result
        = m_imageDocumentCommandRuntime.loadOpenedCollectionVideoPlaybackDevice(
            requestedScope, requestedSourceUrl);
    if (lifetime.expired() || !m_documentTransitionAdmission.accepts(transitionRevision)
        || !result.has_value() || m_imagePublicSnapshot.sourceKind != requestedSourceKind
        || m_imagePublicSnapshot.sourceUrl != requestedSourceUrl
        || m_imagePublicSnapshot.displayedUrl != requestedDisplayedUrl
        || m_imagePublicSnapshot.displayedOpenedCollectionScope != requestedScope
        || m_imagePublicSnapshot.unsupportedOpenedCollectionVideo
        || !m_imagePublicSnapshot.readyForInformation) {
        return false;
    }
    if (const auto* error = kiriview::mediaEntrySourceResultError(*result)) {
        logMediaEntrySourceError("opened collection video loading failed", *error);
        return false;
    }

    auto* playbackDevice = kiriview::mediaEntrySourceResultValue(*result);
    if (playbackDevice == nullptr || playbackDevice->device == nullptr) {
        return false;
    }

    enterOpenedCollectionVideoDocument(transitionRevision, requestedSourceUrl,
        videoPlaybackSourceDeviceFromMediaEntryDevice(std::move(*playbackDevice)));
    return true;
}

bool DocumentSessionRuntimeGraph::tryReturnToImageDocumentFromOpenedCollectionVideo()
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    if (m_callbackState->routingSource || !m_state.openedCollectionVideoActive()
        || m_state.documentKind() != DocumentSessionKind::Video
        || m_imagePublicSnapshot.sourceKind == ImageDocumentPageKind::Video
        || !m_imagePublicSnapshot.readyForInformation
        || m_imagePublicSnapshot.sourceUrl.isEmpty()) {
        return false;
    }

    const quint64 transitionRevision = m_documentTransitionAdmission.next();
    const auto current = [this, lifetime, transitionRevision]() {
        return !lifetime.expired() && m_documentTransitionAdmission.accepts(transitionRevision);
    };
    executeWithVideoLeafSyncSuppressed([this, current]() {
        if (!current() || !leaveVideoMode() || !current()) {
            return;
        }
        if (!refreshVideoPublicSnapshot() || !current()) {
            return;
        }
        m_state.setOpenedCollectionVideoActive(false);
        m_state.setSourceIdentity(m_imagePublicSnapshot.sourceUrl);
        m_state.setFileDeletionInProgress(m_imagePublicSnapshot.fileDeletionInProgress);
        setDocumentKind(DocumentSessionKind::Image);
        if (!current()) {
            return;
        }
        publishActiveNavigationForImagePages();
    });
    return true;
}

bool DocumentSessionRuntimeGraph::tryClearOpenedCollectionVideoAfterImageDocumentCleared()
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    if (m_callbackState->routingSource || !m_state.openedCollectionVideoActive()
        || m_state.documentKind() != DocumentSessionKind::Video
        || !m_imagePublicSnapshot.sourceUrl.isEmpty() || m_imagePublicSnapshot.readyForInformation
        || m_imagePublicSnapshot.error || m_imagePublicSnapshot.fileDeletionInProgress) {
        return false;
    }

    const quint64 transitionRevision = m_documentTransitionAdmission.next();
    const auto current = [this, lifetime, transitionRevision]() {
        return !lifetime.expired() && m_documentTransitionAdmission.accepts(transitionRevision);
    };
    executeWithVideoLeafSyncSuppressed([this, current]() {
        if (!current() || !leaveVideoMode() || !current()) {
            return;
        }
        if (!refreshVideoPublicSnapshot() || !current()) {
            return;
        }
        m_state.setOpenedCollectionVideoActive(false);
        m_state.setSourceIdentity({});
        m_state.setFileDeletionInProgress(false);
        setDocumentKind(DocumentSessionKind::Empty);
        if (!current()) {
            return;
        }
        recomputePublicProjection();
    });
    return true;
}

void DocumentSessionRuntimeGraph::enterOpenedCollectionVideoDocument(
    quint64 transitionRevision, const QUrl& sourceUrl, VideoPlaybackSourceDevice sourceDevice)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const auto current = [this, lifetime, transitionRevision]() {
        return !lifetime.expired() && m_documentTransitionAdmission.accepts(transitionRevision);
    };
    auto sourceDeviceOwner = std::make_shared<VideoPlaybackSourceDevice>(std::move(sourceDevice));
    executeWithVideoLeafSyncSuppressed([this, current, sourceUrl, sourceDeviceOwner]() {
        if (!current()) {
            return;
        }
        cancelMediaOpenWith();
        if (!current()) {
            return;
        }
        cancelMediaDeletion();
        if (!current()) {
            return;
        }
        m_state.setDirectMediaNavigation({}, false, {});
        if (!current()) {
            return;
        }
        const bool directMediaScopeChanged = m_state.clearDirectMediaCursor();
        if (!current()) {
            return;
        }
        if (directMediaScopeChanged) {
            syncMediaPredecodeScope();
            if (!current()) {
                return;
            }
        }
        if (!leaveVideoMode() || !current()) {
            return;
        }
        if (!refreshVideoPublicSnapshot() || !current()) {
            return;
        }

        m_state.setOpenedCollectionVideoActive(true);
        if (!current()) {
            return;
        }
        m_state.setSourceIdentity(sourceUrl);
        if (!current()) {
            return;
        }
        m_videoOutputRuntime.activateSurfaceClaimEpoch();
        if (!current()) {
            return;
        }
        if (!m_videoDocumentCommandRuntime.setSourceDevice(sourceUrl, std::move(*sourceDeviceOwner))
            || !current()) {
            return;
        }
        if (!refreshVideoPublicSnapshot() || !current()) {
            return;
        }
        setDocumentKind(DocumentSessionKind::Video);
        if (!current()) {
            return;
        }
        recomputePublicProjection();
    });
}

bool DocumentSessionRuntimeGraph::refreshImagePublicSnapshot()
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const quint64 transitionRevision = m_documentTransitionAdmission.current();
    const quint64 refreshRevision = m_imageSnapshotRefreshAdmission.next();
    const auto snapshot = m_imageDocument.snapshot;
    DocumentSessionPublicImageLeafSnapshot nextSnapshot
        = buildDocumentSessionPublicImageLeafSnapshot(snapshot());
    if (lifetime.expired() || m_documentTransitionAdmission.current() != transitionRevision
        || !m_imageSnapshotRefreshAdmission.accepts(refreshRevision)) {
        return false;
    }

    m_imagePublicSnapshot = std::move(nextSnapshot);
    return true;
}

bool DocumentSessionRuntimeGraph::refreshVideoPublicSnapshot()
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const quint64 transitionRevision = m_documentTransitionAdmission.current();
    const quint64 refreshRevision = m_videoSnapshotRefreshAdmission.next();
    const auto snapshot = m_videoDocument.snapshot;
    DocumentSessionPublicVideoLeafSnapshot nextSnapshot
        = buildDocumentSessionPublicVideoLeafSnapshot(snapshot());
    if (lifetime.expired() || m_documentTransitionAdmission.current() != transitionRevision
        || !m_videoSnapshotRefreshAdmission.accepts(refreshRevision)) {
        return false;
    }

    m_videoPublicSnapshot = std::move(nextSnapshot);
    return true;
}

void DocumentSessionRuntimeGraph::refreshLeafPublicSnapshots()
{
    if (!refreshImagePublicSnapshot()) {
        return;
    }
    static_cast<void>(refreshVideoPublicSnapshot());
}

void DocumentSessionRuntimeGraph::syncImageDocumentFileDeletionProgress()
{
    if (m_callbackState->routingSource) {
        return;
    }

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
    if (kind == DocumentSessionKind::Video) {
        m_videoOutputRuntime.activateSurfaceClaimEpoch();
    } else {
        m_videoOutputRuntime.retireSurfaceClaimEpoch();
    }
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
    m_directMediaOpenSupersessionAdmission.next();
    setPendingActiveNavigationRevealContext(
        ActiveNavigationRevealContext { ActiveNavigationRevealIntent::LoadOrOpen });
    executeRoutePlan(documentSessionRoutePlanForSourceUrl(sourceUrl, m_state.documentKind()));
}

void DocumentSessionRuntimeGraph::openMediaUrl(
    const QUrl& url, std::function<bool()> originatingCurrent)
{
    static_cast<void>(
        executeRoutePlan(documentSessionRoutePlanForMediaUrl(url, m_state.documentKind()),
            DocumentSessionRouteExecutionControl { std::move(originatingCurrent), {} }));
}

void DocumentSessionRuntimeGraph::executeRoutePlan(const DocumentSessionRoutePlan& plan)
{
    static_cast<void>(executeRoutePlan(plan, {}));
}

bool DocumentSessionRuntimeGraph::executeRoutePlan(
    const DocumentSessionRoutePlan& plan, const DocumentSessionRouteExecutionControl& control)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const NavigationSourceResolver sourceResolver = m_navigationSourceResolver;
    const std::function<bool()> externallyCurrent = control.isCurrent;
    const quint64 observedTransitionRevision = m_documentTransitionAdmission.current();
    const bool externalAccepted = !externallyCurrent || externallyCurrent();
    if (lifetime.expired() || m_documentTransitionAdmission.current() != observedTransitionRevision
        || !externalAccepted) {
        return false;
    }
    const quint64 transitionRevision = m_documentTransitionAdmission.next();
    const DocumentSessionRouteExecutionControl executionControl {
        [this, lifetime, transitionRevision, externallyCurrent]() {
            if (lifetime.expired() || !m_documentTransitionAdmission.accepts(transitionRevision)) {
                return false;
            }
            const bool externalAccepted = !externallyCurrent || externallyCurrent();
            return externalAccepted && !lifetime.expired()
                && m_documentTransitionAdmission.accepts(transitionRevision);
        },
        control.beforePublicProjection,
    };
    qCDebug(kiriviewNavigationLog)
        << "execute route plan"
        << "routeKind" << routeKindName(plan.kind) << "sourceUrl" << plan.sourceUrl
        << "documentKindBefore" << documentKindName(m_state.documentKind());
    const bool completed = m_routeRuntime.executeWithSourceResolver(
        plan,
        [sourceResolver](
            const QUrl& sourceUrl) { return sourceResolver.resolveExternalSource(sourceUrl); },
        executionControl);
    if (lifetime.expired()) {
        return completed;
    }
    qCDebug(kiriviewNavigationLog)
        << "execute route plan complete"
        << "routeKind" << routeKindName(plan.kind) << "completed" << completed
        << "documentKindAfter" << documentKindName(m_state.documentKind()) << "sourceUrl"
        << m_state.sourceUrl() << "activeNavigationAvailable"
        << m_state.activeNavigationSnapshot().available << "activeNavigationKnown"
        << m_state.activeNavigationSnapshot().known << "activeNavigationCurrent"
        << m_state.activeNavigationSnapshot().currentNumber << "activeNavigationCount"
        << m_state.activeNavigationSnapshot().count;
    return completed;
}

bool DocumentSessionRuntimeGraph::leaveVideoMode()
{
    return m_videoDocumentCommandRuntime.leaveMode(m_videoPublicSnapshot.sourceUrl);
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
    if (!m_mediaDeletionTransaction.active() && !m_mediaDeletionRuntime.active()) {
        return;
    }

    const bool clearProgress = m_state.fileDeletionInProgress();
    m_mediaDeletionTransaction.cancel();
    if (clearProgress) {
        m_state.setFileDeletionInProgress(false);
    }
    m_mediaDeletionRuntime.cancel();
}

void DocumentSessionRuntimeGraph::cancelMediaOpenWith() { m_mediaOpenWithRuntime.cancel(); }

void DocumentSessionRuntimeGraph::finishMediaDeletion(
    const ImageAsyncScopedOperation<DirectMediaScope>& operation,
    const DocumentSessionMediaDeletionCompletion& completion)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    if (!m_mediaDeletionTransaction.accepts(operation)) {
        return;
    }
    if (!m_directMediaScopePort.cursorMatches(operation.scope)) {
        if (m_mediaDeletionTransaction.finish(operation)) {
            m_state.setFileDeletionInProgress(false);
            recomputePublicProjection();
        }
        return;
    }

    if (completion.plan.reportFailure) {
        const QString message = fileDeletionErrorMessage(completion.failure);
        const std::function<void(const QString&)> fileDeletionFailed = m_fileDeletionFailed;
        if (!m_mediaDeletionTransaction.finish(operation)) {
            return;
        }
        m_state.setSessionErrorString(message);
        m_state.setFileDeletionInProgress(false);
        recomputePublicProjection();
        if (lifetime.expired()) {
            return;
        }
        if (fileDeletionFailed) {
            fileDeletionFailed(message);
        }
        return;
    }

    if (completion.plan.hasRoutePlan()) {
        bool committed = false;
        const DocumentSessionRouteExecutionControl control {
            [this, operation, &committed]() {
                return committed || m_mediaDeletionTransaction.accepts(operation);
            },
            [this, operation, &committed]() {
                if (!m_mediaDeletionTransaction.finish(operation)) {
                    return;
                }
                committed = true;
                m_state.setFileDeletionInProgress(false);
            },
        };
        static_cast<void>(executeRoutePlan(completion.plan.routePlan, control));
        if (lifetime.expired()) {
            return;
        }
        if (!committed && m_mediaDeletionTransaction.finish(operation)) {
            m_state.setFileDeletionInProgress(false);
            recomputePublicProjection();
        }
        return;
    }

    if (!m_mediaDeletionTransaction.finish(operation)) {
        return;
    }
    m_state.setFileDeletionInProgress(false);
    recomputePublicProjection();
}

ActiveZoomSnapshot DocumentSessionRuntimeGraph::activeZoomSnapshotForKind(
    DocumentSessionKind kind) const
{
    return documentSessionActiveZoomSnapshot(kind, m_imagePublicSnapshot, m_videoPublicSnapshot);
}

}
