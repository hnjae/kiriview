// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONVIDEOOUTPUTRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONVIDEOOUTPUTRUNTIME_H

#include <QPointer>
#include <QRectF>
#include <QString>
#include <QtGlobal>
#include <functional>
#include <optional>

class QObject;

namespace kiriview {
struct DocumentSessionVideoOutputAttachmentPort
{
    std::function<void(QObject*)> setVideoOutput;
    std::function<void(const QRectF&, const QRectF&)> setVideoOutputGeometry;
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
    QString nextSurfaceClaimToken();
    bool reportSurfaceClaim(const DocumentSessionVideoOutputClaimReport& report,
        const DocumentSessionVideoOutputClaimAdmission& admission,
        const DocumentSessionVideoOutputAttachmentPort& attachmentPort);
    void clearAttachment(const DocumentSessionVideoOutputAttachmentPort& attachmentPort);
    void clear();

private:
    std::optional<quint64> consumeSurfaceClaimToken(const QString& token);

    QPointer<QObject> m_surfaceClaimOwner;
    quint64 m_lastIssuedSurfaceClaimRevision = 0;
    quint64 m_lastObservedSurfaceClaimRevision = 0;
};
}

#endif
