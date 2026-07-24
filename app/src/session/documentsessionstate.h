// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONSTATE_H
#define KIRIVIEW_DOCUMENTSESSIONSTATE_H

#include "session/directmedianavigationcandidatesnapshot.h"
#include "session/documentsessionpublicprojection.h"
#include "session/documentsessiontypes.h"

#include <QUrl>
#include <functional>
#include <vector>

namespace kiriview {
class DocumentSessionState final
{
public:
    using ChangeCallback = std::function<void(const std::vector<DocumentSessionChange>&)>;

    explicit DocumentSessionState(ChangeCallback changeCallback = {});

    [[nodiscard]] const QUrl& sourceUrl() const;
    [[nodiscard]] DocumentSessionKind documentKind() const;
    [[nodiscard]] const QString& sessionErrorString() const;
    [[nodiscard]] const QString& windowTitleSubject() const;
    [[nodiscard]] bool fileDeletionInProgress() const;
    [[nodiscard]] bool openedCollectionVideoActive() const;
    [[nodiscard]] const ActiveZoomSnapshot& activeZoomSnapshot() const;
    [[nodiscard]] bool activeImageReady() const;
    [[nodiscard]] bool activeImageUnsupportedOpenedCollectionVideo() const;
    [[nodiscard]] const DirectMediaNavigationBoundaryState& directMediaNavigationState() const;
    [[nodiscard]] bool directMediaNavigationKnown() const;
    [[nodiscard]] const std::vector<DirectMediaNavigationCandidate>&
    directMediaNavigationCandidates() const;
    [[nodiscard]] const DirectMediaNavigationCandidateSnapshot&
    directMediaNavigationCandidateSnapshot() const;
    [[nodiscard]] const ActiveNavigationSnapshot& activeNavigationSnapshot() const;
    [[nodiscard]] ActiveNavigationRevealIntent activeNavigationRevealIntent() const;
    [[nodiscard]] ActiveNavigationRevealDirection activeNavigationRevealDirection() const;
    [[nodiscard]] ActiveNavigationSourceKind activeNavigationSourceKind() const;
    [[nodiscard]] ActiveNavigationBoundaryScope activeNavigationBoundaryScope() const;
    [[nodiscard]] bool displayedMediaOpenWithAvailable() const;
    [[nodiscard]] bool displayedFileDeletionAvailable() const;
    [[nodiscard]] const MediaInformationProjectionSnapshot& mediaInformationSnapshot() const;
    [[nodiscard]] const DocumentSessionPublicSnapshot& publicSnapshot() const;
    [[nodiscard]] const DirectMediaCursor& directMediaCursor() const;
    [[nodiscard]] QUrl directMediaCursorUrl() const;
    [[nodiscard]] std::optional<DirectMediaScope> directMediaScope() const;

    void setSourceIdentity(const QUrl& url);
    void setDocumentKind(DocumentSessionKind kind);
    void setDocumentKindAndActiveZoomSnapshot(
        DocumentSessionKind kind, ActiveZoomSnapshot activeZoomSnapshot);
    void setFileDeletionInProgress(bool inProgress);
    void setOpenedCollectionVideoActive(bool active);
    void setActiveNavigationRevealIntent(ActiveNavigationRevealIntent intent);
    void setActiveNavigationRevealDirection(ActiveNavigationRevealDirection direction);
    void setDirectMediaNavigation(DirectMediaNavigationBoundaryState state, bool known,
        std::vector<DirectMediaNavigationCandidate> candidates);
    bool updatePublicSnapshot(const DocumentSessionPublicSnapshotInput& input);
    bool updatePublicSnapshotForSourceKind(
        const DocumentSessionPublicSnapshotInput& input, ActiveNavigationSourceKind sourceKind);
    void setSessionErrorString(const QString& errorString);
    bool clearDirectMediaCursor();
    bool requestDirectImageCursor(ResolvedNavigationSource source);
    DirectMediaConfirmation confirmDirectImageCursor(const QUrl& url);
    bool restoreDirectImageCursorAfterFailure();
    bool setDirectVideoCursor(ResolvedNavigationSource source);

    void publish(DocumentSessionChange change);
    void publish(const std::vector<DocumentSessionChange>& changes);

private:
    bool applyPublicSnapshot(DocumentSessionPublicSnapshot snapshot);

    ChangeCallback m_changeCallback;
    QUrl m_sourceUrl;
    DocumentSessionKind m_documentKind = DocumentSessionKind::Empty;
    DirectMediaCursor m_directMediaCursor;
    DirectMediaNavigationBoundaryState m_directMediaNavigationState;
    DirectMediaNavigationCandidateSnapshot m_directMediaNavigationCandidateSnapshot;
    DocumentSessionPublicSnapshot m_publicSnapshot;
    ActiveZoomSnapshot m_activeZoomSnapshot;
    ActiveNavigationRevealIntent m_activeNavigationRevealIntent
        = ActiveNavigationRevealIntent::None;
    ActiveNavigationRevealDirection m_activeNavigationRevealDirection
        = ActiveNavigationRevealDirection::None;
    bool m_directMediaNavigationKnown = false;
    bool m_fileDeletionInProgress = false;
    bool m_openedCollectionVideoActive = false;
    QString m_sourceErrorString;
};
}

#endif
