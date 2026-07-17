#include "publicdiagnostic_p.h"

#include <ImageViewport/imagesequence.h>

#include <QtCore/QRegularExpression>

#include <utility>

namespace ImageViewportInternal {

namespace {

QString redactDetails(QString diagnostic)
{
    static const QRegularExpression credentialPattern(
        QStringLiteral("\\b(?:password|passwd|pwd|token|api[_-]?key|secret)\\s*[:=]\\s*\\S+"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression urlPattern(
        QStringLiteral("\\b[A-Za-z][A-Za-z0-9+.-]*://\\S+"));
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

QString plainText(QString diagnostic)
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

} // namespace

PublicDiagnosticText::PublicDiagnosticText(QString text)
    : m_text(std::move(text))
{
}

PublicDiagnosticText PublicDiagnosticText::fromUntrusted(QString diagnostic)
{
    diagnostic = plainText(redactDetails(std::move(diagnostic)));

    const auto scalars = diagnostic.toUcs4();
    const int maximumLength = ImageSequenceLimits::maximumDiagnosticCharacters();
    if (scalars.size() <= qsizetype(maximumLength)) {
        return PublicDiagnosticText(std::move(diagnostic));
    }

    QString bounded;
    bounded.reserve(diagnostic.size());
    for (int i = 0; i < maximumLength; ++i) {
        const char32_t scalar = static_cast<char32_t>(scalars.at(i));
        bounded += QString::fromUcs4(&scalar, 1);
    }
    return PublicDiagnosticText(std::move(bounded));
}

PublicDiagnosticText PublicDiagnosticText::withFallback(QString fallback) const
{
    return m_text.isEmpty() ? fromUntrusted(std::move(fallback)) : *this;
}

} // namespace ImageViewportInternal
