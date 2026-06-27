#pragma once

#include "imageviewport.h"

class ImageViewportPrivate;

class ViewportProviderBridge
{
public:
    explicit ViewportProviderBridge(ImageViewportPrivate& viewport);

    void closeSession(ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken);
    bool openSession();
    void requestMetadata(ImageSequenceProviderRequestToken token);
    void requestFrame(ImageSequenceProviderRequestToken token, int frame);
    void requestPosition(ImageSequenceProviderRequestToken token, int frame, int position);
    void requestPlayback(ImageSequenceProviderRequestToken token, int frame, int position);
    void cancelRequest(ImageSequenceProviderRequestToken token);

private:
    ImageSequenceProviderThreadingContract threadingContract() const;

    ImageViewportPrivate& viewport;
};
