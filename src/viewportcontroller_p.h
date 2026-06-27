#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"

class ImageViewportPrivate;

struct ViewportCommandResult
{
    ImageViewport::CommandOutcome outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewportInternal::ViewportChangeSet changes;
};

class ViewportController
{
public:
    explicit ViewportController(ImageViewportPrivate& viewport);

    ViewportCommandResult clear();
    ViewportCommandResult play();
    ViewportCommandResult pause();
    ViewportCommandResult stop();
    ViewportCommandResult seek(int frame);
    ViewportCommandResult seekToPosition(int milliseconds);
    ViewportCommandResult resetView();
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void advancePlaybackForTest(int elapsedMilliseconds);
    void setNextProviderRequestTokenForTest(quint64 token);
    bool hasPendingRenderCommitForTest() const;
    quint64 activeRequestIdForTest() const;
    quint64 displayedRequestIdForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
#endif

private:
    ImageViewportPrivate& viewport;
};
