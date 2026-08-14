// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <ImageViewport/imageviewporttypes.h>

#include <type_traits>

static_assert(std::is_enum_v<ImageViewportFitMode>);
static_assert(std::is_enum_v<ImageViewportContentAnchor>);
static_assert(std::is_enum_v<ImageSequenceAuthoredAnimationLoopMode>);
static_assert(std::is_copy_constructible_v<ImageSequenceAuthoredAnimationFacts>);
static_assert(std::is_copy_constructible_v<ImageViewportRevisionToken>);
static_assert(std::is_copy_constructible_v<ImageViewportAllocationGenerationToken>);
static_assert(!std::is_constructible_v<ImageViewportAllocationGenerationToken, quint64>);
static_assert(!std::is_same_v<ImageViewportAllocationGenerationToken,
    ImageViewportPresentationTargetGenerationToken>);
static_assert(std::is_copy_constructible_v<ImageViewportRoleSet>);
