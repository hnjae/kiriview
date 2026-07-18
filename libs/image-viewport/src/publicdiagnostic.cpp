// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "publicdiagnostic_p.h"

#include <ImageViewport/imagesequence.h>

#include <QtCore/QRegularExpression>

#include <utility>

namespace ImageViewportInternal {

namespace {

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

PublicDiagnosticText PublicDiagnosticText::fromTrusted(QString diagnostic)
{
    diagnostic = plainText(std::move(diagnostic));

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

PublicDiagnosticText PublicDiagnosticText::withTrustedFallback(QString fallback) const
{
    return m_text.isEmpty() ? fromTrusted(std::move(fallback)) : *this;
}

} // namespace ImageViewportInternal
