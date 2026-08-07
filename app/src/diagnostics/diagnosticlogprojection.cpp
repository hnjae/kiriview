// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "diagnosticlogprojection.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDebug>
#include <QRandomGenerator>

#include <utility>

namespace {
enum class ProjectionDomain {
    Source,
    Path,
    Detail,
};

const QByteArray& processDiagnosticSalt()
{
    static const QByteArray salt = [] {
        QByteArray generated;
        generated.reserve(64);
        for (int index = 0; index < 4; ++index) {
            generated += QByteArray::number(QRandomGenerator::system()->generate64(), 16)
                             .rightJustified(16, '0');
        }
        return generated;
    }();
    return salt;
}

QByteArrayView domainBytes(ProjectionDomain domain)
{
    switch (domain) {
    case ProjectionDomain::Source:
        return QByteArrayView("kiriview-diagnostic-source");
    case ProjectionDomain::Path:
        return QByteArrayView("kiriview-diagnostic-path");
    case ProjectionDomain::Detail:
        return QByteArrayView("kiriview-diagnostic-detail");
    }

    return {};
}

const char* domainLabel(ProjectionDomain domain)
{
    switch (domain) {
    case ProjectionDomain::Source:
        return "source@";
    case ProjectionDomain::Path:
        return "path@";
    case ProjectionDomain::Detail:
        return "detail@";
    }

    return "unknown@";
}

QString projectionToken(ProjectionDomain domain, QByteArrayView value)
{
    const QByteArray& salt = processDiagnosticSalt();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayView(salt));
    hash.addData(domainBytes(domain));
    hash.addData(value);
    return QString::fromLatin1(domainLabel(domain))
        + QString::fromLatin1(hash.result().first(16).toHex());
}
}

namespace kiriview {
DiagnosticLogProjection::DiagnosticLogProjection(QString serialized)
    : m_serialized(std::move(serialized))
{
}

DiagnosticLogProjection diagnosticSourceReference(const QUrl& sourceUrl)
{
    const QByteArray encoded = sourceUrl.toEncoded(QUrl::FullyEncoded);
    return DiagnosticLogProjection(
        projectionToken(ProjectionDomain::Source, QByteArrayView(encoded)));
}

DiagnosticLogProjection diagnosticPathReference(QStringView path)
{
    const QByteArray encoded = path.toString().toUtf8();
    return DiagnosticLogProjection(
        projectionToken(ProjectionDomain::Path, QByteArrayView(encoded)));
}

DiagnosticLogProjection diagnosticDetailReference(QStringView detail)
{
    const QByteArray encoded = detail.toString().toUtf8();
    return DiagnosticLogProjection(
        projectionToken(ProjectionDomain::Detail, QByteArrayView(encoded)));
}

QDebug operator<<(QDebug debug, const DiagnosticLogProjection& projection)
{
    QDebugStateSaver stateSaver(debug);
    debug.noquote().nospace() << projection.m_serialized;
    return debug;
}
}
