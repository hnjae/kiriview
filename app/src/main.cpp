// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/applicationdiagnostics.h"
#include "application/applicationruntime.h"
#include "application/applicationstartupsource.h"

#include <QStringList>

int main(int argumentCount, char* arguments[])
{
    QStringList startupArguments;
    startupArguments.reserve(argumentCount);
    for (int index = 0; index < argumentCount; ++index) {
        startupArguments.push_back(QString::fromLocal8Bit(arguments[index]));
    }

    const auto startup = kiriview::parseApplicationStartupSource(startupArguments);
    if (!startup) {
        kiriview::writeApplicationStartupDiagnostic(startup.error());
        return 2;
    }

    return kiriview::runApplication(*startup);
}
