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
    void requestMetadata(const ImageSequenceProviderRequestToken &token);
    void requestFrame(const ImageSequenceProviderRequestToken &token, int frame);
    void requestPlayback(const ImageSequenceProviderRequestToken &token, int frame, int position);
    void cancelRequest(const ImageSequenceProviderRequestToken &token);

private:
    ImageViewportPrivate &viewport;
};
