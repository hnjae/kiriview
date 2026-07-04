#pragma once

enum class RenderFailureCause {
    None,
    MissingWindow,
    TextureCreationFailure,
    ImageNodeCreationFailure,
    InvalidRolePayload,
    UnknownBackendFailure,
};
