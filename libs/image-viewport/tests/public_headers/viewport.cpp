// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <ImageViewport/imageviewport.h>

#include <QtQuick/QQuickItem>

#include <type_traits>

static_assert(std::is_base_of_v<QQuickItem, ImageViewport>);
static_assert(std::is_copy_constructible_v<ImageViewportPresentationCommand>);
static_assert(std::is_copy_constructible_v<PresentationTargetTransitionPolicy>);
