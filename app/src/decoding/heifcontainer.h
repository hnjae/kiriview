// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_HEIFCONTAINER_H
#define KIRIVIEW_HEIFCONTAINER_H

#include <QByteArray>

namespace kiriview {
struct HeifContainerInfo
{
    bool stillImage = false;
    bool imageSequence = false;

    bool isHeif() const { return stillImage || imageSequence; }
};

HeifContainerInfo heifContainerInfo(const QByteArray& data);
bool isLikelyHeifContainer(const QByteArray& data);
bool isLikelyHeifStillImageContainer(const QByteArray& data);
}

#endif
