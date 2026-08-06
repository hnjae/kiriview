// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageformatregistry.h"

#include "archive/archiveformat.h"
#include "format/supportedmediaformats.h"

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

}
