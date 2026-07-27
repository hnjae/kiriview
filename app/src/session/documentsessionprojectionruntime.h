// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONPROJECTIONRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONPROJECTIONRUNTIME_H

#include "async/imageasyncticket.h"
#include "session/activenavigationprojection.h"
#include "session/activenavigationthumbnailprojection.h"
#include "session/documentsessionpublicprojection.h"

#include <QtGlobal>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace kiriview {
struct DocumentSessionProjectionRuntimePorts
{
    std::function<bool(const DocumentSessionPublicSnapshotInput&)> updatePublicSnapshot;
    std::function<bool(const DocumentSessionPublicSnapshotInput&, ActiveNavigationSourceKind)>
        updatePublicSnapshotForSourceKind;
    std::function<ActiveNavigationSourceKind()> activeNavigationSourceKind;
    std::function<ActiveNavigationSnapshot()> activeNavigationSnapshot;
    std::function<const DirectMediaNavigationCandidateSnapshot&()>
        directMediaNavigationCandidateSnapshot;
    std::function<void(std::vector<ActiveNavigationThumbnailRow>)> setActiveNavigationThumbnailRows;
    std::function<void(int)> setActiveNavigationThumbnailCurrentNumber;
    std::function<void()> clearActiveNavigationRevealContextIfUnavailable;
};

class DocumentSessionProjectionRuntime final
{
public:
    explicit DocumentSessionProjectionRuntime(DocumentSessionProjectionRuntimePorts ports = {});
    ~DocumentSessionProjectionRuntime();
    Q_DISABLE_COPY_MOVE(DocumentSessionProjectionRuntime)

    void publish(const DocumentSessionPublicSnapshotInput& input,
        const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot);
    void publishForSourceKind(const DocumentSessionPublicSnapshotInput& input,
        ActiveNavigationSourceKind sourceKind,
        const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot);

private:
    bool syncActiveNavigationThumbnailRows(
        const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot,
        const DocumentSessionProjectionRuntimePorts& ports,
        const std::weak_ptr<void>& callbackLifetime, quint64 publicationRevision);

    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    DocumentSessionProjectionRuntimePorts m_ports;
    ImageAsyncTicket m_publicationAdmission;
    std::optional<DocumentSessionPublicSnapshotInput> m_publicDependencyInput;
    std::optional<ActiveNavigationThumbnailRowSetIdentity> m_activeNavigationThumbnailIdentity;
    int m_activeNavigationThumbnailCurrentNumber = 0;
    bool m_activeNavigationThumbnailProjectionInitialized = false;
};
}

#endif
