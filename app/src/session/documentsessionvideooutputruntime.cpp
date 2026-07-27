// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionvideooutputruntime.h"

#include <algorithm>
#include <optional>

namespace {
QString surfaceClaimToken(quint64 revision) { return QString::number(revision); }

std::optional<quint64> surfaceClaimRevisionFromToken(const QString& token)
{
    bool ok = false;
    const quint64 revision = token.toULongLong(&ok);
    if (!ok || revision == 0 || token != surfaceClaimToken(revision)) {
        return std::nullopt;
    }

    return revision;
}
}

namespace kiriview {
QString DocumentSessionVideoOutputRuntime::nextSurfaceClaimToken()
{
    ++m_lastIssuedSurfaceClaimRevision;
    if (m_lastIssuedSurfaceClaimRevision == 0) {
        ++m_lastIssuedSurfaceClaimRevision;
    }
    return surfaceClaimToken(m_lastIssuedSurfaceClaimRevision);
}

bool DocumentSessionVideoOutputRuntime::reportSurfaceClaim(
    const DocumentSessionVideoOutputClaimReport& report,
    const DocumentSessionVideoOutputClaimAdmission& admission,
    const DocumentSessionVideoOutputAttachmentPort& attachmentPort)
{
    const std::optional<quint64> claimRevision = consumeSurfaceClaimToken(report.claimToken);
    if (!claimRevision.has_value()
        || report.projectionRevision != admission.currentProjectionRevision
        || report.surfaceOwner == nullptr) {
        return false;
    }

    const bool sameOwner = m_surfaceClaimOwner == report.surfaceOwner;
    if (!report.active || !admission.videoDocumentActive) {
        if (!sameOwner) {
            return false;
        }
        clearAttachment(attachmentPort);
        return true;
    }

    if (report.videoOutput == nullptr) {
        return false;
    }

    m_surfaceClaimOwner = report.surfaceOwner;
    if (attachmentPort.setVideoOutput) {
        attachmentPort.setVideoOutput(report.videoOutput);
    }
    if (attachmentPort.setVideoOutputGeometry) {
        attachmentPort.setVideoOutputGeometry(report.contentRect, report.sourceRect);
    }
    return true;
}

void DocumentSessionVideoOutputRuntime::clearAttachment(
    const DocumentSessionVideoOutputAttachmentPort& attachmentPort)
{
    clear();
    if (attachmentPort.setVideoOutput) {
        attachmentPort.setVideoOutput(nullptr);
    }
}

void DocumentSessionVideoOutputRuntime::clear()
{
    m_lastObservedSurfaceClaimRevision
        = std::max(m_lastObservedSurfaceClaimRevision, m_lastIssuedSurfaceClaimRevision);
    m_surfaceClaimOwner.clear();
}

std::optional<quint64> DocumentSessionVideoOutputRuntime::consumeSurfaceClaimToken(
    const QString& token)
{
    const std::optional<quint64> revision = surfaceClaimRevisionFromToken(token);
    if (!revision.has_value() || *revision > m_lastIssuedSurfaceClaimRevision
        || *revision <= m_lastObservedSurfaceClaimRevision) {
        return std::nullopt;
    }

    m_lastObservedSurfaceClaimRevision = *revision;
    return revision;
}
}
