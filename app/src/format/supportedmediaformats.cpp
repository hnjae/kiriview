// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "format/supportedmediaformats.h"

#include <QStringView>
#include <optional>

namespace {
const QStringList& rawImageExtensions()
{
    static const QStringList extensions { QStringLiteral("3fr"), QStringLiteral("arw"),
        QStringLiteral("bay"), QStringLiteral("bmq"), QStringLiteral("cr2"), QStringLiteral("cr3"),
        QStringLiteral("crw"), QStringLiteral("cs1"), QStringLiteral("cs2"), QStringLiteral("dcr"),
        QStringLiteral("dng"), QStringLiteral("erf"), QStringLiteral("fff"), QStringLiteral("iiq"),
        QStringLiteral("k25"), QStringLiteral("kdc"), QStringLiteral("mdc"), QStringLiteral("mef"),
        QStringLiteral("mos"), QStringLiteral("mrw"), QStringLiteral("nef"), QStringLiteral("nrw"),
        QStringLiteral("orf"), QStringLiteral("pef"), QStringLiteral("raf"), QStringLiteral("raw"),
        QStringLiteral("rdc"), QStringLiteral("rw2"), QStringLiteral("rwl"), QStringLiteral("sr2"),
        QStringLiteral("srf"), QStringLiteral("srw"), QStringLiteral("x3f") };
    return extensions;
}

std::optional<QString> fileNameExtension(QStringView name)
{
    const qsizetype dot = name.lastIndexOf(u'.');
    if (dot <= 0 || dot == name.size() - 1) {
        return std::nullopt;
    }
    return name.sliced(dot + 1).toString().toLower();
}

bool fileNameHasExtension(const QString& name, const QStringList& extensions)
{
    const std::optional<QString> extension = fileNameExtension(name);
    return extension.has_value() && extensions.contains(*extension);
}
}

namespace kiriview::SupportedMediaFormats {
QStringList imageExtensions()
{
    QStringList extensions { QStringLiteral("png"), QStringLiteral("jpeg"), QStringLiteral("jpg"),
        QStringLiteral("jp2"), QStringLiteral("jxl"), QStringLiteral("gif"), QStringLiteral("webp"),
        QStringLiteral("avif"), QStringLiteral("avifs"), QStringLiteral("avci"),
        QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("hif"),
        QStringLiteral("heics"), QStringLiteral("heifs"), QStringLiteral("hej2"),
        QStringLiteral("bmp"), QStringLiteral("tif"), QStringLiteral("tiff"),
        QStringLiteral("svg") };
    extensions.append(rawImageExtensions());
    extensions.sort();
    extensions.removeDuplicates();
    return extensions;
}

QStringList directVideoExtensions()
{
    return { QStringLiteral("m4v"), QStringLiteral("mov"), QStringLiteral("mp4") };
}

QStringList ordinaryMediaExtensions()
{
    QStringList extensions = imageExtensions();
    extensions.append(directVideoExtensions());
    extensions.sort();
    extensions.removeDuplicates();
    return extensions;
}

QStringList imageMimeTypes()
{
    QStringList mimeTypes {
#define KIRIVIEW_IMAGE_MIME_TYPE(mimeType) QStringLiteral(mimeType),
#define KIRIVIEW_DIRECT_VIDEO_MIME_TYPE(mimeType)
#define KIRIVIEW_COMIC_ARCHIVE_MIME_TYPE(scheme, mimeType)
#include "format/supportedmediamimetypes.inc"
#undef KIRIVIEW_COMIC_ARCHIVE_MIME_TYPE
#undef KIRIVIEW_DIRECT_VIDEO_MIME_TYPE
#undef KIRIVIEW_IMAGE_MIME_TYPE
    };
    mimeTypes.sort();
    mimeTypes.removeDuplicates();
    return mimeTypes;
}

QStringList directVideoMimeTypes()
{
    QStringList mimeTypes {
#define KIRIVIEW_IMAGE_MIME_TYPE(mimeType)
#define KIRIVIEW_DIRECT_VIDEO_MIME_TYPE(mimeType) QStringLiteral(mimeType),
#define KIRIVIEW_COMIC_ARCHIVE_MIME_TYPE(scheme, mimeType)
#include "format/supportedmediamimetypes.inc"
#undef KIRIVIEW_COMIC_ARCHIVE_MIME_TYPE
#undef KIRIVIEW_DIRECT_VIDEO_MIME_TYPE
#undef KIRIVIEW_IMAGE_MIME_TYPE
    };
    mimeTypes.sort();
    mimeTypes.removeDuplicates();
    return mimeTypes;
}

bool isSupportedImageFileName(const QString& name)
{
    return fileNameHasExtension(name, imageExtensions());
}

bool isSupportedDirectVideoFileName(const QString& name)
{
    return fileNameHasExtension(name, directVideoExtensions());
}

bool isSupportedOrdinaryMediaFileName(const QString& name)
{
    return isSupportedImageFileName(name) || isSupportedDirectVideoFileName(name);
}

bool isRawImageExtension(const QString& extension)
{
    return rawImageExtensions().contains(extension.toLower());
}
}
