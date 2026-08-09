// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_APPLICATIONDIAGNOSTICS_H
#define KIRIVIEW_APPLICATIONDIAGNOSTICS_H

#include <QByteArray>
#include <QStringView>
#include <QtGlobal>

namespace kiriview {
inline constexpr qsizetype maximumApplicationStartupDiagnosticBytes = 1024;

void configureApplicationDiagnosticLogging(bool verbose);
[[nodiscard]] QByteArray applicationStartupDiagnosticRecord(QStringView detail);
void writeApplicationStartupDiagnostic(QStringView detail) noexcept;
}

#endif
