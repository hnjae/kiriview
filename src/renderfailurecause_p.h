#pragma once

#include <QtCore/QMetaType>

enum class RenderFailureCause {
    None,
    TextureCreationFailure,
    ImageNodeCreationFailure,
    InvalidRolePayload,
    UnknownBackendFailure,
};

Q_DECLARE_METATYPE(RenderFailureCause)
