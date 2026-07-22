// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_APPLICATIONSTARTUPSOURCE_H
#define KIRIVIEW_APPLICATIONSTARTUPSOURCE_H

#include <QString>
#include <QStringList>
#include <QUrl>
#include <expected>

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

using ApplicationStartupParseResult = std::expected<ApplicationStartupSource, QString>;

QUrl initialSourceUrlFromStartupSource(const ApplicationStartupSource& source);
ApplicationStartupParseResult parseApplicationStartupSource(const QStringList& arguments);
}

#endif
