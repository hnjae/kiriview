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
