// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <ImageViewport/imageviewportstate.h>

#include <type_traits>

#if defined(QT_QML_LIB) || defined(QT_QUICK_LIB)
#error "State-only consumers must not inherit Qt QML or Qt Quick usage requirements"
#endif

static_assert(std::is_copy_constructible_v<ImageViewportStateSnapshot>);
static_assert(std::is_copy_constructible_v<ImageViewportCommandResult>);
static_assert(std::is_copy_constructible_v<ImageViewportCoordinateInput>);
