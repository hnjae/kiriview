/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <ImageViewport/imagesequence.h>

struct ImageSequenceFactoryResultQmlForeign
{
    Q_GADGET
    QML_FOREIGN(ImageSequenceFactoryResult)
    QML_NAMED_ELEMENT(ImageSequenceFactoryResult)
    QML_UNCREATABLE("ImageSequenceFactoryResult objects are returned by ImageSequenceFactory")
    QML_EXTENDED_NAMESPACE(ImageSequenceFactoryEnums)
};
