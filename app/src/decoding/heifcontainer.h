// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_HEIFCONTAINER_H
#define KIRIVIEW_HEIFCONTAINER_H

#include <QByteArray>
#include <QByteArrayView>

namespace kiriview {
enum class HeifBrandKind {
    Unknown,
    StillImage,
    ImageSequence,
};

struct HeifContainerInfo
{
    bool stillImage = false;
    bool imageSequence = false;

    [[nodiscard]] bool isHeif() const { return stillImage || imageSequence; }
};

HeifContainerInfo heifContainerInfo(const QByteArray& data);
HeifBrandKind heifBrandKind(QByteArrayView brand);
bool isLikelyHeifContainer(const QByteArray& data);
bool isLikelyHeifStillImageContainer(const QByteArray& data);
}

#endif
