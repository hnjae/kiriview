// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "archiveformat.h"

#include <QMimeDatabase>
#include <QMimeType>
#include <QStringView>
#include <algorithm>
#include <array>
#include <optional>

namespace {
using ArchiveMatchResolver = std::optional<kiriview::ArchiveOpenMatch> (*)(const QString&);

struct ArchiveFormat
{
    QStringView scheme;
    QStringView comicExtension;
    std::array<QStringView, 2> comicMimeTypes;
    QStringView archiveExtension;
    std::array<QStringView, 3> archiveMimeTypes;
    kiriview::ArchiveStorageBackend backend;
};

const std::array<ArchiveFormat, 4>& archiveFormats()
{
    static const std::array formats {
        ArchiveFormat { u"zip", u"cbz", { u"application/vnd.comicbook+zip", u"" }, u"zip",
            { u"application/zip", u"", u"" }, kiriview::ArchiveStorageBackend::KZip },
        ArchiveFormat { u"tar", u"cbt", { u"application/x-cbt", u"" }, u"tar",
            { u"application/x-tar", u"", u"" }, kiriview::ArchiveStorageBackend::KTar },
        ArchiveFormat { u"sevenz", u"cb7", { u"application/x-cb7", u"" }, u"7z",
            { u"application/x-7z-compressed", u"", u"" }, kiriview::ArchiveStorageBackend::K7Zip },
        ArchiveFormat { u"rar", u"cbr", { u"application/vnd.comicbook-rar", u"application/x-cbr" },
            u"rar",
            { u"application/vnd.rar", u"application/x-rar", u"application/x-rar-compressed" },
            kiriview::ArchiveStorageBackend::LibArchive },
    };
    return formats;
}

std::optional<QString> fileNameExtension(QStringView name)
{
    const qsizetype dot = name.lastIndexOf(u'.');
    if (dot <= 0 || dot == name.size() - 1) {
        return std::nullopt;
    }
    return name.sliced(dot + 1).toString().toLower();
}

bool containsMimeType(const std::array<QStringView, 3>& values, QStringView value)
{
    return std::ranges::find(values, value) != values.end();
}

std::optional<kiriview::ArchiveOpenMatch> archiveMatchForUrl(const QUrl& url,
    QMimeDatabase::MatchMode mimeMatchMode, ArchiveMatchResolver matchForFileName,
    ArchiveMatchResolver matchForMimeTypeName)
{
    if (!url.isLocalFile()) {
        return std::nullopt;
    }

    std::optional<kiriview::ArchiveOpenMatch> extensionMatch = matchForFileName(url.fileName());
    if (extensionMatch.has_value()) {
        return extensionMatch;
    }

    const QMimeType mimeType = QMimeDatabase().mimeTypeForFile(url.toLocalFile(), mimeMatchMode);
    return matchForMimeTypeName(mimeType.name());
}

}

namespace kiriview {
ArchiveStorageBackend archiveStorageBackendForRootScheme(const QString& scheme)
{
    const auto found = std::ranges::find_if(archiveFormats(),
        [&scheme](const ArchiveFormat& format) { return format.scheme == scheme; });
    return found == archiveFormats().end() ? ArchiveStorageBackend::None : found->backend;
}

bool archiveRootSchemeUsesKioFuse(const QString& scheme)
{
    const ArchiveStorageBackend backend = archiveStorageBackendForRootScheme(scheme);
    return backend == ArchiveStorageBackend::KZip || backend == ArchiveStorageBackend::KTar
        || backend == ArchiveStorageBackend::K7Zip;
}

QStringList supportedComicBookArchiveExtensions()
{
    return { QStringLiteral("cbz"), QStringLiteral("cbt"), QStringLiteral("cb7"),
        QStringLiteral("cbr") };
}

bool isComicBookArchiveFileName(const QString& name)
{
    return comicBookArchiveMatchForFileName(name).has_value();
}

std::optional<ArchiveOpenMatch> comicBookArchiveMatchForFileName(const QString& fileName)
{
    const std::optional<QString> extension = fileNameExtension(fileName);
    if (!extension.has_value()) {
        return std::nullopt;
    }
    const auto found = std::ranges::find_if(archiveFormats(),
        [&extension](const ArchiveFormat& format) { return format.comicExtension == *extension; });
    return found == archiveFormats().end()
        ? std::nullopt
        : std::optional<ArchiveOpenMatch>(
              ArchiveOpenMatch { found->scheme.toString(), ArchiveOpenMatchKind::ComicBook });
}

std::optional<ArchiveOpenMatch> directArchiveOpenMatchForFileName(const QString& fileName)
{
    if (const std::optional<ArchiveOpenMatch> comic = comicBookArchiveMatchForFileName(fileName)) {
        return comic;
    }
    const std::optional<QString> extension = fileNameExtension(fileName);
    if (!extension.has_value()) {
        return std::nullopt;
    }
    const auto found
        = std::ranges::find_if(archiveFormats(), [&extension](const ArchiveFormat& format) {
              return format.archiveExtension == *extension;
          });
    return found == archiveFormats().end()
        ? std::nullopt
        : std::optional<ArchiveOpenMatch>(
              ArchiveOpenMatch { found->scheme.toString(), ArchiveOpenMatchKind::GeneralArchive });
}

std::optional<ArchiveOpenMatch> directArchiveOpenMatchForMimeTypeName(const QString& mimeTypeName)
{
    const auto comic
        = std::ranges::find_if(archiveFormats(), [&mimeTypeName](const ArchiveFormat& format) {
              return std::ranges::find(format.comicMimeTypes, mimeTypeName)
                  != format.comicMimeTypes.end();
          });
    if (comic != archiveFormats().end()) {
        return ArchiveOpenMatch { comic->scheme.toString(), ArchiveOpenMatchKind::ComicBook };
    }
    const auto archive
        = std::ranges::find_if(archiveFormats(), [&mimeTypeName](const ArchiveFormat& format) {
              return containsMimeType(format.archiveMimeTypes, mimeTypeName);
          });
    return archive == archiveFormats().end()
        ? std::nullopt
        : std::optional<ArchiveOpenMatch>(ArchiveOpenMatch {
              archive->scheme.toString(), ArchiveOpenMatchKind::GeneralArchive });
}

std::optional<ArchiveOpenMatch> directArchiveOpenMatchForUrl(const QUrl& url)
{
    return archiveMatchForUrl(url, QMimeDatabase::MatchDefault,
        kiriview::directArchiveOpenMatchForFileName,
        kiriview::directArchiveOpenMatchForMimeTypeName);
}

}
