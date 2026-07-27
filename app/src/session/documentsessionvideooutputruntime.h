// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONVIDEOOUTPUTRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONVIDEOOUTPUTRUNTIME_H

#include <QMetaObject>
#include <QPointer>
#include <QRectF>
#include <QString>
#include <QtGlobal>
#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace kiriview {
struct DocumentSessionVideoOutputAttachmentPort
{
    std::function<void(QObject*, const QRectF&, const QRectF&)> setVideoOutputAttachment;
};

struct DocumentSessionVideoOutputClaimReport
{
    QString claimToken;
    QObject* surfaceOwner = nullptr;
    QObject* videoOutput = nullptr;
    bool active = false;
    QRectF contentRect;
    QRectF sourceRect;
    quint64 projectionRevision = 0;
};

struct DocumentSessionVideoOutputClaimAdmission
{
    quint64 currentProjectionRevision = 0;
    bool videoDocumentActive = false;
};

class DocumentSessionVideoOutputRuntime final
{
public:
    explicit DocumentSessionVideoOutputRuntime(
        DocumentSessionVideoOutputAttachmentPort attachmentPort);
    ~DocumentSessionVideoOutputRuntime();
    Q_DISABLE_COPY_MOVE(DocumentSessionVideoOutputRuntime)

    void activateSurfaceClaimEpoch();
    void retireSurfaceClaimEpoch();
    QString nextSurfaceClaimToken();
    bool reportSurfaceClaim(const DocumentSessionVideoOutputClaimReport& report,
        const DocumentSessionVideoOutputClaimAdmission& admission);
    void clearAttachment();

private:
    struct ActiveSurfaceClaim
    {
        quint64 endpointGeneration = 0;
        QPointer<QObject> surfaceOwner;
        QPointer<QObject> videoOutput;
    };

    std::optional<quint64> consumeSurfaceClaimToken(const QString& token);
    void replaceActiveClaim(QObject* surfaceOwner, QObject* videoOutput, const QRectF& contentRect,
        const QRectF& sourceRect);
    void revokeDestroyedEndpoint(quint64 endpointGeneration);
    void disconnectActiveEndpointObservers();
    void invalidateIssuedClaims();

    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    std::unique_ptr<QObject> m_connectionContext;
    std::function<void(QObject*, const QRectF&, const QRectF&)> m_applyAttachment;
    std::optional<ActiveSurfaceClaim> m_activeClaim;
    QMetaObject::Connection m_surfaceOwnerDestroyedConnection;
    QMetaObject::Connection m_videoOutputDestroyedConnection;
    bool m_closing = false;
    bool m_surfaceClaimEpochActive = false;
    quint64 m_lastIssuedSurfaceClaimRevision = 0;
    quint64 m_lastObservedSurfaceClaimRevision = 0;
    quint64 m_endpointGeneration = 0;
};
}

#endif
