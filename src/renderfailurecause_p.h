#pragma once

#include <QtCore/QMetaType>

enum class RenderFailureCause {
    None,
    MissingWindow,
    TextureCreationFailure,
    ImageNodeCreationFailure,
    InvalidRolePayload,
    UnknownBackendFailure,
};

Q_DECLARE_METATYPE(RenderFailureCause)
