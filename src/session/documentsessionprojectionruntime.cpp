// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionprojectionruntime.h"

#include <utility>

namespace kiriview {
namespace {
    const DirectMediaNavigationCandidateSnapshot& emptyDirectMediaNavigationCandidateSnapshot()
    {
        static const DirectMediaNavigationCandidateSnapshot snapshot;
        return snapshot;
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
