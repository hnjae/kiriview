#pragma once

#include "imageviewport.h"

struct ViewportEnginePresentationTargetState
{
    ImageViewportPresentationTarget presentationTarget
        = ImageViewportPresentationTarget::clear();
    ImageViewportRoleSet acceptedRoleSet;
    ImageViewportRoleSet targetRoleSet;
    quint64 generation = 0;
    quint64 primaryRoleGeneration = 0;
    quint64 secondaryRoleGeneration = 0;
    ImageViewport::PageRole activeRole = ImageViewport::PageRole::Primary;
    bool activeRoleValid = false;
};
