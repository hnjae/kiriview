// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONRUNTIME_H

#include "session/activenavigationprojection.h"
#include "session/documentsessiondocumentports.h"
#include "session/documentsessionpublicprojection.h"
#include "session/documentsessionruntimedependencies.h"
#include "session/documentsessionstate.h"
#include "system/filedeletion.h"

#include <QRectF>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <functional>
#include <memory>
#include <optional>

class QAbstractListModel;
class QObject;

namespace kiriview {
class DocumentSessionRuntimeGraph;

class DocumentSessionRuntime final
{
public:
    using ChangeCallback = DocumentSessionState::ChangeCallback;

    DocumentSessionRuntime(QObject* owner, DocumentSessionImageDocumentSnapshotPort imageDocument,
        DocumentSessionImageDocumentCommandPort imageCommands,
        DocumentSessionVideoDocumentSnapshotPort videoDocument,
        DocumentSessionVideoDocumentCommandPort videoCommands, ChangeCallback changeCallback = {},
        DocumentSessionRuntimeDependencies dependencies = {});
    ~DocumentSessionRuntime();

    QUrl sourceUrl() const;
    void setSourceUrl(const QUrl& sourceUrl);
    DocumentSessionKind documentKind() const;
    quint64 publicProjectionRevision() const;
    QString errorString() const;
    QString windowTitleSubject() const;
    bool displayedFileDeletionAvailable() const;
    bool displayedMediaOpenWithAvailable() const;
    bool fileDeletionInProgress() const;
    const MediaInformationProjectionSnapshot& mediaInformationSnapshot() const;
    bool activeZoomPercentAvailable() const;
    bool activeZoomPercentKnown() const;
    qreal activeZoomPercent() const;
    bool activeZoomEditable() const;
    bool activeImageReady() const;
    bool activeImageUnsupportedOpenedCollectionVideo() const;
    bool activeImageOpenedCollectionScopeActive() const;
    bool activeImageRightToLeftReadingActive() const;
    bool activeVideoReady() const;
    bool activeVideoControlsReady() const;
    const DocumentSessionActionStateSnapshot& actionStateSnapshot() const;
    const DocumentSessionActionAvailabilityFacts& actionAvailabilityFacts() const;
    bool activeNavigationAvailable() const;
    bool activeNavigationKnown() const;
    bool activeNavigationEditable() const;
    bool activeNavigationHasTargets() const;
    bool activeNavigationDispatchAvailable() const;
    int activeNavigationCurrentNumber() const;
    int activeNavigationCount() const;
    bool canOpenPreviousActiveNavigation() const;
    bool canOpenNextActiveNavigation() const;
    bool atKnownFirstActiveNavigation() const;
    bool atKnownLastActiveNavigation() const;
    bool directMediaNavigationBoundaryActive() const;
    ActiveNavigationBoundaryScope activeNavigationBoundaryScope() const;
    ActiveNavigationRevealIntent activeNavigationRevealIntent() const;
    ActiveNavigationRevealDirection activeNavigationRevealDirection() const;
    QAbstractListModel* activeNavigationThumbnailModel() const;
    bool replaceActiveNavigationThumbnailDemandSnapshot(
        ActiveNavigationThumbnailDemandSnapshot snapshot);
    QString nextVideoOutputSurfaceClaimToken();
    bool reportVideoOutputSurfaceClaim(const QString& claimToken, quint64 projectionRevision,
        QObject* surfaceOwner, QObject* videoOutput, bool active, const QRectF& contentRect,
        const QRectF& sourceRect);
    std::optional<PredecodedImage> findPredecodedImage(const QUrl& url) const;

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
    DocumentSessionImageDocumentSnapshotPort m_imageDocument;
    DocumentSessionImageDocumentCommandPort m_imageCommands;
    DocumentSessionVideoDocumentSnapshotPort m_videoDocument;
    DocumentSessionVideoDocumentCommandPort m_videoCommands;
    DocumentSessionState m_state;
    DocumentSessionPublicImageLeafSnapshot m_imagePublicSnapshot;
    DocumentSessionPublicVideoLeafSnapshot m_videoPublicSnapshot;
    std::unique_ptr<DocumentSessionRuntimeGraph> m_runtimeGraph;
};
}

#endif
