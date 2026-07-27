// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionvideooutputruntime.h"

#include <QObject>
#include <algorithm>
#include <optional>
#include <utility>

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
DocumentSessionVideoOutputRuntime::DocumentSessionVideoOutputRuntime(
    DocumentSessionVideoOutputAttachmentPort attachmentPort)
    : m_connectionContext(std::make_unique<QObject>())
    , m_applyAttachment(std::move(attachmentPort.setVideoOutputAttachment))
{
}

DocumentSessionVideoOutputRuntime::~DocumentSessionVideoOutputRuntime()
{
    m_closing = true;
    const bool detachActiveClaim = m_activeClaim.has_value();
    const auto applyAttachment = std::move(m_applyAttachment);
    m_surfaceClaimEpochActive = false;
    invalidateIssuedClaims();
    m_callbackLifetime.reset();
    disconnectActiveEndpointObservers();
    ++m_endpointGeneration;
    m_activeClaim.reset();
    m_connectionContext.reset();
    if (detachActiveClaim && applyAttachment) {
        applyAttachment(nullptr, {}, {});
    }
}

void DocumentSessionVideoOutputRuntime::activateSurfaceClaimEpoch()
{
    if (m_closing || m_surfaceClaimEpochActive) {
        return;
    }

    invalidateIssuedClaims();
    m_surfaceClaimEpochActive = true;
}

void DocumentSessionVideoOutputRuntime::retireSurfaceClaimEpoch()
{
    if (m_closing) {
        return;
    }
    m_surfaceClaimEpochActive = false;
    invalidateIssuedClaims();
    if (!m_activeClaim.has_value()) {
        return;
    }
    clearAttachment();
}

QString DocumentSessionVideoOutputRuntime::nextSurfaceClaimToken()
{
    if (m_closing) {
        return {};
    }
    ++m_lastIssuedSurfaceClaimRevision;
    if (m_lastIssuedSurfaceClaimRevision == 0) {
        ++m_lastIssuedSurfaceClaimRevision;
    }
    return surfaceClaimToken(m_lastIssuedSurfaceClaimRevision);
}

bool DocumentSessionVideoOutputRuntime::reportSurfaceClaim(
    const DocumentSessionVideoOutputClaimReport& report,
    const DocumentSessionVideoOutputClaimAdmission& admission)
{
    if (m_closing) {
        return false;
    }
    const std::optional<quint64> claimRevision = consumeSurfaceClaimToken(report.claimToken);
    if (!claimRevision.has_value()) {
        return false;
    }
    if (!m_surfaceClaimEpochActive) {
        return false;
    }

    const QPointer<QObject> surfaceOwner(report.surfaceOwner);
    const QPointer<QObject> videoOutput(report.videoOutput);
    if (report.projectionRevision != admission.currentProjectionRevision || surfaceOwner.isNull()) {
        return false;
    }

    const bool sameOwner = m_activeClaim.has_value() && m_activeClaim->surfaceOwner == surfaceOwner;
    if (!report.active || !admission.videoDocumentActive) {
        if (!sameOwner) {
            return false;
        }
        clearAttachment();
        return true;
    }

    if (videoOutput.isNull()) {
        return false;
    }

    replaceActiveClaim(
        surfaceOwner.data(), videoOutput.data(), report.contentRect, report.sourceRect);
    return true;
}

void DocumentSessionVideoOutputRuntime::clearAttachment()
{
    if (m_closing) {
        return;
    }
    const auto applyAttachment = m_applyAttachment;
    invalidateIssuedClaims();
    disconnectActiveEndpointObservers();
    ++m_endpointGeneration;
    m_activeClaim.reset();
    if (applyAttachment) {
        applyAttachment(nullptr, {}, {});
    }
}

void DocumentSessionVideoOutputRuntime::replaceActiveClaim(QObject* surfaceOwner,
    QObject* videoOutput, const QRectF& contentRect, const QRectF& sourceRect)
{
    const bool endpointsChanged = !m_activeClaim.has_value()
        || m_activeClaim->surfaceOwner != surfaceOwner || m_activeClaim->videoOutput != videoOutput;
    if (endpointsChanged) {
        disconnectActiveEndpointObservers();
        ++m_endpointGeneration;
    }

    m_activeClaim = ActiveSurfaceClaim {
        m_endpointGeneration,
        surfaceOwner,
        videoOutput,
    };

    if (endpointsChanged) {
        const quint64 endpointGeneration = m_endpointGeneration;
        const std::weak_ptr<void> lifetime = m_callbackLifetime;
        m_surfaceOwnerDestroyedConnection = QObject::connect(surfaceOwner, &QObject::destroyed,
            m_connectionContext.get(), [this, lifetime, endpointGeneration]() {
                if (!lifetime.expired()) {
                    revokeDestroyedEndpoint(endpointGeneration);
                }
            });
        m_videoOutputDestroyedConnection = QObject::connect(videoOutput, &QObject::destroyed,
            m_connectionContext.get(), [this, lifetime, endpointGeneration]() {
                if (!lifetime.expired()) {
                    revokeDestroyedEndpoint(endpointGeneration);
                }
            });
    }

    const auto applyAttachment = m_applyAttachment;
    if (applyAttachment) {
        applyAttachment(videoOutput, contentRect, sourceRect);
    }
}

void DocumentSessionVideoOutputRuntime::revokeDestroyedEndpoint(quint64 endpointGeneration)
{
    if (!m_activeClaim.has_value() || m_activeClaim->endpointGeneration != endpointGeneration) {
        return;
    }

    const auto applyAttachment = m_applyAttachment;
    invalidateIssuedClaims();
    disconnectActiveEndpointObservers();
    ++m_endpointGeneration;
    m_activeClaim.reset();
    if (applyAttachment) {
        applyAttachment(nullptr, {}, {});
    }
}

void DocumentSessionVideoOutputRuntime::disconnectActiveEndpointObservers()
{
    if (m_surfaceOwnerDestroyedConnection) {
        QObject::disconnect(m_surfaceOwnerDestroyedConnection);
        m_surfaceOwnerDestroyedConnection = {};
    }
    if (m_videoOutputDestroyedConnection) {
        QObject::disconnect(m_videoOutputDestroyedConnection);
        m_videoOutputDestroyedConnection = {};
    }
}

void DocumentSessionVideoOutputRuntime::invalidateIssuedClaims()
{
    m_lastObservedSurfaceClaimRevision
        = std::max(m_lastObservedSurfaceClaimRevision, m_lastIssuedSurfaceClaimRevision);
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
