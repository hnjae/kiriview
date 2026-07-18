/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <QtCore/QMetaType>

enum class RenderFailureCause {
    None,
    TextureCreationFailure,
    ImageNodeCreationFailure,
    InvalidRolePayload,
    InvalidRenderGeometry,
    UnknownBackendFailure,
};

Q_DECLARE_METATYPE(RenderFailureCause)
