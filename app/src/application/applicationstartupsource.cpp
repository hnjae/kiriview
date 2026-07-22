// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "applicationstartupsource.h"

#include <QDir>
#include <QFileInfo>
#include <QStringView>

namespace {
bool isVerboseOption(QStringView argument) { return argument == u"--verbose" || argument == u"-v"; }

bool isOption(QStringView argument) { return argument.size() > 1 && argument.front() == u'-'; }

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

ApplicationStartupParseResult parseApplicationStartupSource(int argumentCount, char* arguments[])
{
    ApplicationStartupParseResult result;
    bool parseOptions = true;
    bool sourceArgumentSeen = false;

    for (int index = 1; index < argumentCount; ++index) {
        const QString argument = QString::fromLocal8Bit(arguments[index]);
        if (parseOptions) {
            if (argument == QStringLiteral("--")) {
                parseOptions = false;
                continue;
            }
            if (isVerboseOption(argument)) {
                result.source.verbose = true;
                continue;
            }
            if (isOption(argument)) {
                result.errorString = QStringLiteral("unknown startup option '%1'").arg(argument);
                return result;
            }
        }

        if (sourceArgumentSeen) {
            continue;
        }
        sourceArgumentSeen = true;
        if (argument.isEmpty()) {
            continue;
        }

        if (hasUrlScheme(argument)) {
            const QUrl url(argument);
            if (url.isLocalFile()) {
                const QString path = url.toLocalFile();
                if (!path.isEmpty() && !QFileInfo::exists(path)) {
                    result.errorString = localPathError(path);
                    return result;
                }
            }
            result.source.kind = ApplicationStartupSourceKind::UrlText;
            result.source.text = argument;
            continue;
        }

        const QString path = QFileInfo(QDir::current(), argument).absoluteFilePath();
        if (!QFileInfo::exists(path)) {
            result.errorString = localPathError(path);
            return result;
        }
        result.source.kind = ApplicationStartupSourceKind::LocalFilePath;
        result.source.text = path;
    }

    return result;
}
}
