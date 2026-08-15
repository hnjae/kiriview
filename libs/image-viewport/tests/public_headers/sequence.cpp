// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <ImageViewport/imagesequence.h>

#include <type_traits>

static_assert(std::is_base_of_v<QObject, ImageSequence>);
static_assert(std::is_base_of_v<QObject, ImageFrame>);
static_assert(std::is_enum_v<ImageSequenceFactoryOutcome>);
static_assert(!std::is_constructible_v<ImageSequenceFactoryResult, ImageSequence*,
    ImageSequenceFactoryOutcome, ImageSequenceFactoryReason>);
static_assert(!std::is_constructible_v<ImageSequenceFactoryResult, ImageSequence*,
    ImageSequenceFactoryOutcome, ImageSequenceFactoryReason, QObject*>);
static_assert(!std::is_constructible_v<ImageSequenceFactoryResult, ImageSequence*,
    ImageSequenceFactoryOutcome, ImageSequenceFactoryReason, QString>);
