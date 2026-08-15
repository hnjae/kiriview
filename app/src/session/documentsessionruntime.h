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
    Q_DISABLE_COPY_MOVE(DocumentSessionRuntime)

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
    void reclaimDisplayOutputAliases();

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
