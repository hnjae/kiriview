#pragma once

#include "imageviewportstate_p.h"
#include "internalobservation_p.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QString>

namespace ImageViewportInternal {

inline QString redactDiagnosticDetails(QString diagnostic)
{
    static const QRegularExpression credentialPattern(
        QStringLiteral("\\b(?:password|passwd|pwd|token|api[_-]?key|secret)\\s*[:=]\\s*\\S+"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression urlPattern(QStringLiteral("\\b[A-Za-z][A-Za-z0-9+.-]*://\\S+"));
    static const QRegularExpression windowsPathPattern(
        QStringLiteral("\\b[A-Za-z]:[\\\\/][^\\s]+"));
    static const QRegularExpression unixPathPattern(
        QStringLiteral("(?<!\\w)/(?:[^\\s/]+/)+[^\\s]+"));

    diagnostic.replace(credentialPattern, QStringLiteral("[redacted-credential]"));
    diagnostic.replace(urlPattern, QStringLiteral("[redacted-url]"));
    diagnostic.replace(windowsPathPattern, QStringLiteral("[redacted-path]"));
    diagnostic.replace(unixPathPattern, QStringLiteral("[redacted-path]"));
    return diagnostic;
}

inline QString plainTextDiagnostic(QString diagnostic)
{
    static const QRegularExpression markupPattern(QStringLiteral("<[^>]*>"));
    diagnostic.replace(markupPattern, QStringLiteral(" "));

    QString plain;
    plain.reserve(diagnostic.size());
    bool pendingSpace = false;
    for (QChar character : diagnostic) {
        const ushort codeUnit = character.unicode();
        if (character.isSpace() || codeUnit < 0x20 || codeUnit == 0x7f) {
            pendingSpace = true;
            continue;
        }
        if (pendingSpace && !plain.isEmpty()) {
            plain += QLatin1Char(' ');
        }
        plain += character;
        pendingSpace = false;
    }
    return plain.trimmed();
}

class InternalObservability
{
public:
    void recordProviderCleanupFailure(const ProviderTransportDiagnostic& diagnostic);
    void recordProviderSchedulerFailure(const ProviderSchedulerDiagnostic& diagnostic);
    void recordRenderFailure(const RenderFailureDiagnostic& diagnostic);
    void record(InternalObservation observation);
    void record(const InternalObservationBatch& observations);

    ProviderTransportDiagnostic lastProviderCleanupFailure() const;
    ProviderSchedulerDiagnostic lastProviderSchedulerFailure() const;
    RenderFailureDiagnostic lastRenderFailure() const;
    QVector<InternalObservation> observations() const;

private:
    ProviderTransportDiagnostic m_lastProviderCleanupFailure;
    ProviderSchedulerDiagnostic m_lastProviderSchedulerFailure;
    RenderFailureDiagnostic m_lastRenderFailure;
    QVector<InternalObservation> m_observations;
    quint64 m_nextObservationSequence = 0;
};

} // namespace ImageViewportInternal
