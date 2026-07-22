// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/applicationruntime.h"
#include "application/applicationstartupsource.h"

#include <QByteArray>
#include <cstdio>

int main(int argumentCount, char* arguments[])
{
    const kiriview::ApplicationStartupParseResult startup
        = kiriview::parseApplicationStartupSource(argumentCount, arguments);
    if (!startup.accepted()) {
        const QByteArray error = startup.errorString.toLocal8Bit();
        std::fprintf(stderr, "KiriView: %s\n", error.constData());
        return 2;
    }

    return kiriview::runApplication(startup.source);
}
