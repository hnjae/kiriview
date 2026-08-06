// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "applicationstartupsource.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>

namespace {
QString localPathError(const QString& path)
{
    return QStringLiteral("cannot open '%1': No such file or directory").arg(path);
}

bool hasUrlScheme(const QString& argument)
{
    const qsizetype colon = argument.indexOf(u':');
    if (colon <= 0 || !argument.front().isLetter()) {
        return false;
    }
    for (qsizetype index = 1; index < colon; ++index) {
        const QChar character = argument.at(index);
        if (!character.isLetterOrNumber() && character != u'+' && character != u'-'
            && character != u'.') {
            return false;
        }
    }
    return true;
}

QUrl validInitialSourceUrl(const QUrl& url)
{
    if (url.isEmpty() || !url.isValid()) {
        return QUrl();
    }

    return url;
}
}

namespace kiriview {
QUrl initialSourceUrlFromStartupSource(const ApplicationStartupSource& source)
{
    switch (source.kind) {
    case ApplicationStartupSourceKind::None:
        return QUrl();
    case ApplicationStartupSourceKind::LocalFilePath:
        return validInitialSourceUrl(QUrl::fromLocalFile(source.text));
    case ApplicationStartupSourceKind::UrlText:
        return validInitialSourceUrl(QUrl(source.text));
    }

    return QUrl();
}

ApplicationStartupParseResult parseApplicationStartupSource(const QStringList& arguments)
{
    QCommandLineParser parser;
    parser.addOption(QCommandLineOption(
        { QStringLiteral("v"), QStringLiteral("verbose") }, QStringLiteral("Verbose output.")));
    parser.addPositionalArgument(QStringLiteral("source"), QStringLiteral("Initial source."));
    if (!parser.parse(arguments)) {
        const QStringList unknownOptions = parser.unknownOptionNames();
        if (!unknownOptions.isEmpty()) {
            const QString& option = unknownOptions.front();
            const QString prefix = option.size() == 1 ? QStringLiteral("-") : QStringLiteral("--");
            return std::unexpected(
                QStringLiteral("unknown startup option '%1%2'").arg(prefix, option));
        }
        return std::unexpected(parser.errorText());
    }

    ApplicationStartupSource source;
    source.verbose = parser.isSet(QStringLiteral("verbose"));
    const QStringList positionalArguments = parser.positionalArguments();
    if (positionalArguments.isEmpty() || positionalArguments.front().isEmpty()) {
        return source;
    }

    const QString& argument = positionalArguments.front();
    const QString path = QFileInfo(QDir::current(), argument).absoluteFilePath();

    if (QFileInfo::exists(path)) {
        source.kind = ApplicationStartupSourceKind::LocalFilePath;
        source.text = path;
        return source;
    }

    if (hasUrlScheme(argument)) {
        const QUrl url(argument);
        if (url.isLocalFile()) {
            const QString path = url.toLocalFile();
            if (!path.isEmpty() && !QFileInfo::exists(path)) {
                return std::unexpected(localPathError(path));
            }
        }
        source.kind = ApplicationStartupSourceKind::UrlText;
        source.text = argument;
        return source;
    }

    return std::unexpected(localPathError(path));
}
}
