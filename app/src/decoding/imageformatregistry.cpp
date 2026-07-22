// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageformatregistry.h"

#include "archive/archiveformat.h"
#include "format/supportedmediaformats.h"

#include <KLocalizedString>

namespace kiriview {
QStringList supportedOpenExtensions()
{
    QStringList extensions = SupportedMediaFormats::imageExtensions();
    extensions.append(supportedComicBookArchiveExtensions());
    extensions.sort();
    extensions.removeDuplicates();
    return extensions;
}

bool isSupportedImageFileName(const QString& name)
{
    return SupportedMediaFormats::isSupportedImageFileName(name);
}

QStringList openDialogNameFilters()
{
    const QString extensionFilter
        = QStringLiteral("*.") + supportedOpenExtensions().join(QStringLiteral(" *."));
    return {
        ki18nc("@item:inlistbox", "Image and comic book files (%1)")
            .subs(extensionFilter)
            .toString(),
        i18nc("@item:inlistbox", "All files (*)"),
    };
}
}
