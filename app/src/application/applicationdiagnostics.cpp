// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "applicationdiagnostics.h"

#include <QByteArray>
#include <QChar>
#include <QLoggingCategory>
#include <QString>
#include <QStringList>
#include <cstdio>

namespace {
QStringList verboseDiagnosticCategoryNames()
{
    return {
        QStringLiteral("org.hnjae.kiriview.decode"),
        QStringLiteral("org.hnjae.kiriview.navigation"),
        QStringLiteral("org.hnjae.kiriview.predecode"),
        QStringLiteral("org.hnjae.kiriview.thumbnail"),
        QStringLiteral("org.hnjae.kiriview.display.provider"),
        QStringLiteral("org.hnjae.kiriview.animation"),
        QStringLiteral("org.hnjae.kiriview.video"),
    };
}

QString verboseDiagnosticFilterRules()
{
    const QStringList categoryNames = verboseDiagnosticCategoryNames();
    QStringList rules;
    rules.reserve(categoryNames.size());
    for (const QString& categoryName : categoryNames) {
        rules.append(QStringLiteral("%1.debug=true").arg(categoryName));
    }
    return rules.join(QLatin1Char('\n'));
}

bool mustEscapeStartupDiagnosticCharacter(QChar character)
{
    switch (character.category()) {
    case QChar::Other_Control:
    case QChar::Other_Format:
    case QChar::Other_Surrogate:
    case QChar::Other_NotAssigned:
    case QChar::Separator_Line:
    case QChar::Separator_Paragraph:
        return true;
    default:
        return false;
    }
}

QByteArray escapedStartupDiagnosticCharacter(QChar character)
{
    switch (character.unicode()) {
    case '\\':
        return QByteArrayLiteral("\\\\");
    case '\n':
        return QByteArrayLiteral("\\n");
    case '\r':
        return QByteArrayLiteral("\\r");
    case '\t':
        return QByteArrayLiteral("\\t");
    default:
        break;
    }

    if (mustEscapeStartupDiagnosticCharacter(character)) {
        return QByteArrayLiteral("\\u")
            + QByteArray::number(character.unicode(), 16).rightJustified(4, '0').toUpper();
    }

    QString text;
    text.append(character);
    return text.toUtf8();
}
}

namespace kiriview {
void configureApplicationDiagnosticLogging(bool verbose)
{
    if (!verbose) {
        return;
    }

    QLoggingCategory::setFilterRules(verboseDiagnosticFilterRules());
}

QByteArray applicationStartupDiagnosticRecord(QStringView detail)
{
    constexpr char prefix[] = "KiriView: ";
    constexpr char fallbackDetail[] = "startup failed";
    constexpr char truncationMarker[] = " [truncated]";

    QByteArray record(prefix);
    if (detail.isEmpty()) {
        record.append(fallbackDetail);
        record.append('\n');
        return record;
    }

    const qsizetype contentLimit = maximumApplicationStartupDiagnosticBytes
        - static_cast<qsizetype>(sizeof(truncationMarker) - 1) - 1;
    bool truncated = false;
    for (QChar character : detail) {
        const QByteArray escaped = escapedStartupDiagnosticCharacter(character);
        if (record.size() > contentLimit - escaped.size()) {
            truncated = true;
            break;
        }
        record.append(escaped);
    }

    if (truncated) {
        record.append(truncationMarker);
    }
    record.append('\n');
    return record;
}

void writeApplicationStartupDiagnostic(QStringView detail) noexcept
{
    try {
        const QByteArray record = applicationStartupDiagnosticRecord(detail);
        static_cast<void>(std::fwrite(
            record.constData(), sizeof(char), static_cast<std::size_t>(record.size()), stderr));
    } catch (...) {
        std::fputs("KiriView: startup failed\n", stderr);
    }
}
}
