// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DIAGNOSTICLOGPROJECTION_H
#define KIRIVIEW_DIAGNOSTICLOGPROJECTION_H

#include <QString>
#include <QStringView>
#include <QUrl>

class QDebug;

namespace kiriview {
inline constexpr qsizetype maximumDiagnosticLogProjectionCharacters = 48;

class DiagnosticLogProjection final
{
public:
    DiagnosticLogProjection(const DiagnosticLogProjection&) = default;
    DiagnosticLogProjection(DiagnosticLogProjection&&) noexcept = default;
    DiagnosticLogProjection& operator=(const DiagnosticLogProjection&) = default;
    DiagnosticLogProjection& operator=(DiagnosticLogProjection&&) noexcept = default;
    ~DiagnosticLogProjection() = default;

private:
    explicit DiagnosticLogProjection(QString serialized);

    QString m_serialized;

    friend DiagnosticLogProjection diagnosticSourceReference(const QUrl& sourceUrl);
    friend DiagnosticLogProjection diagnosticPathReference(QStringView path);
    friend DiagnosticLogProjection diagnosticDetailReference(QStringView detail);
    friend QDebug operator<<(QDebug debug, const DiagnosticLogProjection& projection);
};

[[nodiscard]] DiagnosticLogProjection diagnosticSourceReference(const QUrl& sourceUrl);
[[nodiscard]] DiagnosticLogProjection diagnosticPathReference(QStringView path);
[[nodiscard]] DiagnosticLogProjection diagnosticDetailReference(QStringView detail);

QDebug operator<<(QDebug debug, const DiagnosticLogProjection& projection);
}

#endif
