#pragma once

#include "imageviewport.h"

class ImageViewportPrivate;

class ViewportProviderBridge
{
public:
    explicit ViewportProviderBridge(ImageViewportPrivate &viewport);

    void closeSession();
    bool openSession();
    ImageSequenceProviderRequestToken nextRequestToken();

private:
    ImageViewportPrivate &viewport;
};
