// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONRUNTIMEGRAPH_H
#define KIRIVIEW_DOCUMENTSESSIONRUNTIMEGRAPH_H

#include "async/imageasyncoperationstate.h"
#include "async/imageasyncticket.h"
#include "navigation/directmedianavigationcandidateprovider.h"
#include "navigation/directmedianavigationmodel.h"
#include "session/activenavigationprojection.h"
#include "session/documentsessionactivenavigationruntime.h"
#include "session/documentsessiondirectmediaactivityport.h"
#include "session/documentsessiondirectmedianavigationcoordinator.h"
#include "session/documentsessiondirectmedianavigationinputport.h"
#include "session/documentsessiondirectmediascopeport.h"
#include "session/documentsessiondocumentports.h"
#include "session/documentsessionimagedocumentcommandruntime.h"
#include "session/documentsessionimagedocumentsyncruntime.h"
#include "session/documentsessionmediadeletionruntime.h"
#include "session/documentsessionmediaopenwithplanport.h"
#include "session/documentsessionmediaopenwithruntime.h"
#include "session/documentsessionmediapredecodeinputport.h"
#include "session/documentsessionmediapredecoderuntime.h"
#include "session/documentsessionprojectionruntime.h"
#include "session/documentsessionpublicleafsnapshotbuilder.h"
#include "session/documentsessionpublicprojection.h"
#include "session/documentsessionpublicsnapshotinputport.h"
#include "session/documentsessionrouteruntime.h"
#include "session/documentsessionruntimedependencies.h"
#include "session/documentsessionstate.h"
#include "session/documentsessionthumbnailruntime.h"
#include "session/documentsessionvideodocumentcommandruntime.h"
#include "session/documentsessionvideodocumentsyncruntime.h"
#include "session/documentsessionvideooutputruntime.h"
#include "system/filedeletion.h"

#include <QMetaObject>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QAbstractListModel;
class QObject;
class QString;

namespace kiriview {
enum class DocumentSessionDirectMediaNavigationRevealAction;

struct DocumentSessionRuntimeGraphPorts
{
    DocumentSessionState& state;
    DocumentSessionImageDocumentSnapshotPort& imageDocument;
    const DocumentSessionVideoDocumentSnapshotPort& videoDocument;
    DocumentSessionPublicImageLeafSnapshot& imagePublicSnapshot;
    DocumentSessionPublicVideoLeafSnapshot& videoPublicSnapshot;
};

class DocumentSessionRuntimeGraph final
{
public:
    DocumentSessionRuntimeGraph(QObject* owner, DocumentSessionRuntimeGraphPorts ports,
        DocumentSessionImageDocumentCommandPort imageCommands,
        DocumentSessionVideoDocumentCommandPort videoCommands,
        DocumentSessionRuntimeDependencies dependencies = {});
    ~DocumentSessionRuntimeGraph();
    Q_DISABLE_COPY_MOVE(DocumentSessionRuntimeGraph)

    [[nodiscard]] QUrl sourceUrl() const;
    void setSourceUrl(const QUrl& sourceUrl);
    [[nodiscard]] DocumentSessionKind documentKind() const;
    [[nodiscard]] quint64 publicProjectionRevision() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] QString windowTitleSubject() const;
    [[nodiscard]] bool displayedFileDeletionAvailable() const;
    [[nodiscard]] bool displayedMediaOpenWithAvailable() const;
    [[nodiscard]] bool fileDeletionInProgress() const;
    [[nodiscard]] const MediaInformationProjectionSnapshot& mediaInformationSnapshot() const;
    [[nodiscard]] bool activeZoomPercentAvailable() const;
    [[nodiscard]] bool activeZoomPercentKnown() const;
    [[nodiscard]] qreal activeZoomPercent() const;
    [[nodiscard]] bool activeZoomEditable() const;
    [[nodiscard]] bool activeImageReady() const;
    [[nodiscard]] bool activeImageReplacementFallbackAvailable() const;
    [[nodiscard]] bool activeImageUnsupportedOpenedCollectionVideo() const;
    [[nodiscard]] bool activeImageOpenedCollectionScopeActive() const;
    [[nodiscard]] bool activeImageRightToLeftReadingActive() const;
    [[nodiscard]] bool activeVideoReady() const;
    [[nodiscard]] bool activeVideoControlsReady() const;
    [[nodiscard]] const DocumentSessionActionStateSnapshot& actionStateSnapshot() const;
    [[nodiscard]] const DocumentSessionActionAvailabilityFacts& actionAvailabilityFacts() const;
    [[nodiscard]] bool activeNavigationAvailable() const;
    [[nodiscard]] bool activeNavigationKnown() const;
    [[nodiscard]] bool activeNavigationEditable() const;
    [[nodiscard]] bool activeNavigationHasTargets() const;
    [[nodiscard]] bool activeNavigationDispatchAvailable() const;
    [[nodiscard]] int activeNavigationCurrentNumber() const;
    [[nodiscard]] int activeNavigationCount() const;
    [[nodiscard]] bool canOpenPreviousActiveNavigation() const;
    [[nodiscard]] bool canOpenNextActiveNavigation() const;
    [[nodiscard]] bool atKnownFirstActiveNavigation() const;
    [[nodiscard]] bool atKnownLastActiveNavigation() const;
    [[nodiscard]] bool directMediaNavigationBoundaryActive() const;
    [[nodiscard]] ActiveNavigationBoundaryScope activeNavigationBoundaryScope() const;
    [[nodiscard]] ActiveNavigationRevealIntent activeNavigationRevealIntent() const;
    [[nodiscard]] ActiveNavigationRevealDirection activeNavigationRevealDirection() const;
    [[nodiscard]] QAbstractListModel* activeNavigationThumbnailModel() const;
    bool replaceActiveNavigationThumbnailDemandSnapshot(
        const ActiveNavigationThumbnailDemandSnapshot& snapshot);
    QString nextVideoOutputSurfaceClaimToken();
    bool reportVideoOutputSurfaceClaim(const QString& claimToken, quint64 projectionRevision,
        QObject* surfaceOwner, QObject* videoOutput, bool active, const QRectF& contentRect,
        const QRectF& sourceRect);
    [[nodiscard]] std::optional<PredecodedImage> findPredecodedImage(
        const DisplayedImageLocation& location) const;

    void openPreviousActiveNavigation();
    void openNextActiveNavigation();
    void openFirstActiveNavigation();
    void openLastActiveNavigation();
    void openActiveNavigationAtNumber(int number);
    void openActiveNavigationThumbnailAtNumber(int number);
    ActiveNavigationDispatchOutcome requestPreviousActiveNavigation();
    ActiveNavigationDispatchOutcome requestNextActiveNavigation();
    void deleteDisplayedFile(FileDeletionMode mode);
    void openCurrentMediaWith(MediaOpenWithCallback callback);

private:
    struct CallbackState
    {
        bool routingSource = false;
        bool videoLeafSyncSuppressed = false;
    };

    void applyDirectMediaNavigationRevealAction(
        DocumentSessionDirectMediaNavigationRevealAction action);
    ActiveNavigationDispatchOutcome executeActiveNavigationDispatchRequest(
        ActiveNavigationDispatchRequest request, ActiveNavigationRevealContext context);
    void setPendingActiveNavigationRevealContext(ActiveNavigationRevealContext context);
    ActiveNavigationRevealContext takePendingActiveNavigationRevealContext(
        ActiveNavigationRevealIntent fallbackIntent);
    void setActiveNavigationRevealContext(ActiveNavigationRevealContext context);
    void applyActiveNavigationRevealContext(ActiveNavigationRevealContext context);
    void clearActiveNavigationRevealContextIfUnavailable();
    void executeWithRoutingSuppressed(const std::function<void()>& mutation);
    void executeWithVideoLeafSyncSuppressed(const std::function<void()>& mutation);
    void connectDocuments();
    void handleImageDocumentSnapshotChanged();
    void handleVideoDocumentSnapshotChanged();
    bool tryEnterOpenedCollectionVideoFromImageSnapshot();
    bool tryReturnToImageDocumentFromOpenedCollectionVideo();
    bool tryClearOpenedCollectionVideoAfterImageDocumentCleared();
    void enterOpenedCollectionVideoDocument(
        quint64 transitionRevision, const QUrl& sourceUrl, VideoPlaybackSourceDevice sourceDevice);
    [[nodiscard]] bool refreshImagePublicSnapshot();
    [[nodiscard]] bool refreshVideoPublicSnapshot();
    void refreshLeafPublicSnapshots();
    void syncImageDocumentFileDeletionProgress();
    void setDocumentKind(DocumentSessionKind kind);
    void publishActiveNavigationForImagePages();
    void recomputePublicProjection();
    void routeSourceUrl(const QUrl& sourceUrl);
    void openMediaUrl(const QUrl& url, std::function<bool()> originatingCurrent);
    void executeRoutePlan(const DocumentSessionRoutePlan& plan);
    [[nodiscard]] bool executeRoutePlan(
        const DocumentSessionRoutePlan& plan, const DocumentSessionRouteExecutionControl& control);
    [[nodiscard]] bool leaveVideoMode();
    void syncMediaPredecodeScope();
    void cacheDisplayedMediaPredecodeImages();
    void cancelMediaDeletion();
    void cancelMediaOpenWith();
    void finishMediaDeletion(const ImageAsyncScopedOperation<DirectMediaScope>& operation,
        const DocumentSessionMediaDeletionCompletion& completion);
    [[nodiscard]] ActiveZoomSnapshot activeZoomSnapshotForKind(DocumentSessionKind kind) const;
    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    std::shared_ptr<CallbackState> m_callbackState = std::make_shared<CallbackState>();
    QObject* m_owner = nullptr;
    DocumentSessionImageDocumentSnapshotPort& m_imageDocument;
    DocumentSessionImageDocumentCommandRuntime m_imageDocumentCommandRuntime;
    DocumentSessionImageDocumentSyncRuntime m_imageDocumentSyncRuntime;
    const DocumentSessionVideoDocumentSnapshotPort& m_videoDocument;
    DocumentSessionVideoDocumentCommandRuntime m_videoDocumentCommandRuntime;
    DocumentSessionState& m_state;
    NavigationSourceResolver m_navigationSourceResolver;
    DocumentSessionVideoDocumentSyncRuntime m_videoDocumentSyncRuntime;
    DocumentSessionDirectMediaScopePort m_directMediaScopePort;
    DocumentSessionDirectMediaActivityPort m_directMediaActivityPort;
    DocumentSessionDirectMediaNavigationInputPort m_directMediaNavigationInputPort;
    DocumentSessionProjectionRuntime m_projectionRuntime;
    DocumentSessionRouteRuntime m_routeRuntime;
    ImageAsyncTicket m_documentTransitionAdmission;
    ImageAsyncTicket m_directMediaOpenSupersessionAdmission;
    ImageAsyncTicket m_imageSnapshotRefreshAdmission;
    ImageAsyncTicket m_videoSnapshotRefreshAdmission;
    DocumentSessionActiveNavigationRuntime m_activeNavigationRuntime;
    DocumentSessionThumbnailRuntime m_activeNavigationThumbnailRuntime;
    DocumentSessionDirectMediaNavigationCoordinator m_directMediaNavigationCoordinator;
    ImageAsyncScopedOperationState<DirectMediaScope> m_mediaDeletionTransaction;
    DocumentSessionMediaDeletionRuntime m_mediaDeletionRuntime;
    std::function<void(const QString&)> m_fileDeletionFailed;
    DocumentSessionMediaOpenWithRuntime m_mediaOpenWithRuntime;
    DocumentSessionMediaPredecodeRuntime m_mediaPredecodeRuntime;
    std::vector<QMetaObject::Connection> m_documentConnections;
    DocumentSessionPublicImageLeafSnapshot& m_imagePublicSnapshot;
    DocumentSessionPublicVideoLeafSnapshot& m_videoPublicSnapshot;
    DocumentSessionPublicSnapshotInputPort m_publicSnapshotInputPort;
    DocumentSessionMediaPredecodeInputPort m_mediaPredecodeInputPort;
    DocumentSessionMediaOpenWithPlanPort m_mediaOpenWithPlanPort;
    DocumentSessionVideoOutputRuntime m_videoOutputRuntime;
};
}

#endif
