// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_AVIFCOMPATIBILITY_H
#define KIRIVIEW_AVIFCOMPATIBILITY_H

#include <QByteArray>
#include <QtGlobal>
#include <optional>

namespace kiriview {
enum class AvifCompatibleDataStorage {
    Original,
    OwnedReplacement,
};

struct AvifCompatibleData
{
    QByteArray data;
    AvifCompatibleDataStorage storage = AvifCompatibleDataStorage::Original;
};

std::optional<qsizetype> avifCompatibilityWorkspaceByteCost(qsizetype sourceByteCount);
AvifCompatibleData avifDataWithCompatibilityFixes(const QByteArray& data);
}

#endif
