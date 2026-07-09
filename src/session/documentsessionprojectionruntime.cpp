// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionprojectionruntime.h"

#include "session/thumbnaillogging.h"

#include <QDebug>

#include <utility>

namespace kiriview {
namespace {
    const DirectMediaNavigationCandidateSnapshot& emptyDirectMediaNavigationCandidateSnapshot()
    {
        static const DirectMediaNavigationCandidateSnapshot snapshot;
        return snapshot;
    }

    const char* activeNavigationSourceKindLogName(ActiveNavigationSourceKind sourceKind)
    {
        switch (sourceKind) {
        case ActiveNavigationSourceKind::OrdinaryDirectMedia:
            return "ordinary-direct-media";
        case ActiveNavigationSourceKind::ImageDocumentPages:
            return "image-document-pages";
        case ActiveNavigationSourceKind::None:
            return "none";
        }

        return "unknown";
    }

    const char* missingActiveNavigationThumbnailRowSetReason(ActiveNavigationSourceKind sourceKind,
        ActiveNavigationSnapshot navigation,
        const DirectMediaNavigationCandidateSnapshot& directMediaNavigationCandidateSnapshot,
        const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot)
    {
        if (!navigation.available) {
            return "navigation-unavailable";
        }
        if (!navigation.known) {
            return "navigation-unknown";
        }
        if (navigation.count <= 0) {
            return "navigation-empty";
        }

        switch (sourceKind) {
        case ActiveNavigationSourceKind::OrdinaryDirectMedia:
            if (!directMediaNavigationCandidateSnapshot.known) {
                return "direct-media-candidates-unknown";
            }
            if (static_cast<int>(
                    directMediaNavigationCandidateRows(directMediaNavigationCandidateSnapshot)
                        .size())
                != navigation.count) {
                return "direct-media-count-mismatch";
            }
            return "direct-media-identity-missing";
        case ActiveNavigationSourceKind::ImageDocumentPages:
            if (!imageDocumentPageCandidateSnapshot.known) {
                return "image-page-candidates-unknown";
            }
            if (!imageDocumentPageCandidateSnapshot.source.has_value()) {
                return "image-page-candidate-source-missing";
            }
            if (static_cast<int>(
                    imageDocumentPageCandidateRows(imageDocumentPageCandidateSnapshot).size())
                != navigation.count) {
                return "image-page-count-mismatch";
            }
            return "image-page-identity-missing";
        case ActiveNavigationSourceKind::None:
            return "source-kind-none";
        }

        return "unknown";
    }
}

DocumentSessionProjectionRuntime::DocumentSessionProjectionRuntime(
    DocumentSessionProjectionRuntimePorts ports)
    : m_ports(std::move(ports))
{
}

void DocumentSessionProjectionRuntime::publish(const DocumentSessionPublicSnapshotInput& input,
    const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot)
{
    if (m_ports.updatePublicSnapshot) {
        m_ports.updatePublicSnapshot(input);
    }
    syncActiveNavigationThumbnailRows(imageDocumentPageCandidateSnapshot);
    clearActiveNavigationRevealContextIfUnavailable();
}

void DocumentSessionProjectionRuntime::publishForSourceKind(
    const DocumentSessionPublicSnapshotInput& input, ActiveNavigationSourceKind sourceKind,
    const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot)
{
    const bool updated = m_ports.updatePublicSnapshotForSourceKind
        && m_ports.updatePublicSnapshotForSourceKind(input, sourceKind);
    if (updated) {
        syncActiveNavigationThumbnailRows(imageDocumentPageCandidateSnapshot);
    }
    clearActiveNavigationRevealContextIfUnavailable();
}

void DocumentSessionProjectionRuntime::syncActiveNavigationThumbnailRows(
    const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot)
{
    const ActiveNavigationSourceKind sourceKind = m_ports.activeNavigationSourceKind
        ? m_ports.activeNavigationSourceKind()
        : ActiveNavigationSourceKind::None;
    const ActiveNavigationSnapshot navigation = m_ports.activeNavigationSnapshot
        ? m_ports.activeNavigationSnapshot()
        : ActiveNavigationSnapshot {};
    const DirectMediaNavigationCandidateSnapshot& directMediaNavigationCandidateSnapshot
        = m_ports.directMediaNavigationCandidateSnapshot
        ? m_ports.directMediaNavigationCandidateSnapshot()
        : emptyDirectMediaNavigationCandidateSnapshot();
    const std::optional<ActiveNavigationThumbnailRowSetIdentity> rowSetIdentity
        = activeNavigationThumbnailRowSetIdentity(sourceKind, navigation,
            directMediaNavigationCandidateSnapshot, imageDocumentPageCandidateSnapshot);
    if (!rowSetIdentity.has_value()) {
        qCDebug(kiriviewThumbnailLog)
            << "Active navigation thumbnail row-set unavailable"
            << "reason"
            << missingActiveNavigationThumbnailRowSetReason(sourceKind, navigation,
                   directMediaNavigationCandidateSnapshot, imageDocumentPageCandidateSnapshot)
            << "sourceKind" << activeNavigationSourceKindLogName(sourceKind)
            << "navigationAvailable" << navigation.available << "navigationKnown"
            << navigation.known << "navigationCurrent" << navigation.currentNumber
            << "navigationCount" << navigation.count << "directCandidatesKnown"
            << directMediaNavigationCandidateSnapshot.known << "directCandidateRevision"
            << directMediaNavigationCandidateSnapshot.revision << "directCandidateRows"
            << directMediaNavigationCandidateRows(directMediaNavigationCandidateSnapshot).size()
            << "imagePageCandidatesKnown" << imageDocumentPageCandidateSnapshot.known
            << "imagePageCandidateHasSource"
            << imageDocumentPageCandidateSnapshot.source.has_value() << "imagePageCandidateRevision"
            << imageDocumentPageCandidateSnapshot.revision << "imagePageCandidateRows"
            << imageDocumentPageCandidateRows(imageDocumentPageCandidateSnapshot).size()
            << "hadPreviousIdentity" << m_activeNavigationThumbnailIdentity.has_value();
        m_activeNavigationThumbnailIdentity.reset();
        if (m_ports.setActiveNavigationThumbnailRows) {
            m_ports.setActiveNavigationThumbnailRows({});
        }
        return;
    }

    if (m_activeNavigationThumbnailIdentity.has_value()
        && sameActiveNavigationThumbnailRowSetIdentity(
            *m_activeNavigationThumbnailIdentity, *rowSetIdentity)
        && m_ports.setActiveNavigationThumbnailCurrentNumber) {
        m_ports.setActiveNavigationThumbnailCurrentNumber(navigation.currentNumber);
        return;
    }

    std::vector<ActiveNavigationThumbnailRow> rows
        = projectActiveNavigationThumbnailRows(sourceKind, navigation,
            directMediaNavigationCandidateSnapshot, imageDocumentPageCandidateSnapshot);
    m_activeNavigationThumbnailIdentity = rowSetIdentity;
    if (m_ports.setActiveNavigationThumbnailRows) {
        m_ports.setActiveNavigationThumbnailRows(std::move(rows));
    }
}

void DocumentSessionProjectionRuntime::clearActiveNavigationRevealContextIfUnavailable()
{
    if (m_ports.clearActiveNavigationRevealContextIfUnavailable) {
        m_ports.clearActiveNavigationRevealContextIfUnavailable();
    }
}
}
