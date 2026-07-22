// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_APPLICATIONSTARTUPSOURCE_H
#define KIRIVIEW_APPLICATIONSTARTUPSOURCE_H

#include <QString>
#include <QUrl>

namespace kiriview {
enum class ApplicationStartupSourceKind {
    None,
    LocalFilePath,
    UrlText,
};

struct ApplicationStartupSource
{
    ApplicationStartupSourceKind kind = ApplicationStartupSourceKind::None;
    QString text;
    bool verbose = false;
};

struct ApplicationStartupParseResult
{
    ApplicationStartupSource source;
    QString errorString;

    [[nodiscard]] bool accepted() const { return errorString.isEmpty(); }
};

QUrl initialSourceUrlFromStartupSource(const ApplicationStartupSource& source);
ApplicationStartupParseResult parseApplicationStartupSource(int argumentCount, char* arguments[]);
}

#endif
