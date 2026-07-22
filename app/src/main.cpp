// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/applicationruntime.h"
#include "application/applicationstartupsource.h"

#include <QByteArray>
#include <QStringList>
#include <cstdio>
#include <print>

namespace {
void printStartupError(const QByteArray& error) noexcept
{
    try {
        std::println(stderr, "KiriView: {}", error.constData());
    } catch (...) {
        std::fputs("KiriView: startup failed\n", stderr);
    }
}
}

int main(int argumentCount, char* arguments[])
{
    QStringList startupArguments;
    startupArguments.reserve(argumentCount);
    for (int index = 0; index < argumentCount; ++index) {
        startupArguments.push_back(QString::fromLocal8Bit(arguments[index]));
    }

    const auto startup = kiriview::parseApplicationStartupSource(startupArguments);
    if (!startup) {
        const QByteArray error = startup.error().toLocal8Bit();
        printStartupError(error);
        return 2;
    }

    return kiriview::runApplication(*startup);
}
