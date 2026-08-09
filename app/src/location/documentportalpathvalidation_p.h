// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTPORTALPATHVALIDATION_P_H
#define KIRIVIEW_DOCUMENTPORTALPATHVALIDATION_P_H

#include <QByteArray>
#include <QString>
#include <cstdint>
#include <optional>

namespace kiriview::NavigationSourceDetail {
struct DocumentPortalMountFacts
{
    std::int64_t entryFileSystemType = 0;
    std::int64_t rootFileSystemType = 0;
    std::uint64_t entryDevice = 0;
    std::uint64_t rootDevice = 0;
    std::uint64_t parentDevice = 0;
};

bool isAuthenticatedDocumentPortalMount(const DocumentPortalMountFacts& facts);
std::optional<QString> validatedDocumentPortalHostPath(
    QByteArray attributeValue, const QString& requestedLocalPath);
bool isDocumentPortalPathCandidate(const QString& localPath, const QString& runtimeDir);
}

#endif
