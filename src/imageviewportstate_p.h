#pragma once

#include "imageviewport.h"

#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtGui/QImage>

#include <memory>

namespace ImageViewportInternal {

struct ViewportChangeSet
{
    bool requestState = false;
    bool displayState = false;
    bool geometryState = false;
    bool playbackPhase = false;
    bool diagnostics = false;
    bool presentation = false;
    bool sequence = false;
    bool looping = false;
    bool displayRevision = false;
    bool requestRevision = false;
    bool commandRevision = false;
    bool scheduleUpdate = false;
};

struct PresentationState
{
    ImageViewport::FillMode fillMode = ImageViewport::FillMode::Contain;
    ImageViewport::HorizontalAlignment horizontalAlignment
        = ImageViewport::HorizontalAlignment::AlignHCenter;
    ImageViewport::VerticalAlignment verticalAlignment
        = ImageViewport::VerticalAlignment::AlignVCenter;
    ImageViewport::BackgroundMode backgroundMode = ImageViewport::BackgroundMode::Transparent;
    QColor backgroundColor = Qt::transparent;
    double zoom = 1.0;
    QPointF pan;
    bool smoothing = true;
    bool mipmap = false;
    bool mirrorHorizontally = false;
    bool mirrorVertically = false;
};

struct DisplayState
{
    ImageViewport::DisplayStatus status = ImageViewport::DisplayStatus::Empty;
    int displayedFrame = -1;
    int displayedPosition = -1;
    quint64 displayedGeneration = 0;
    QSizeF displayedImageSize;
    QImage displayedImage;
    QImage pendingDisplayImage;
    bool renderCommitPending = false;
    bool renderFailureRetainedDisplayValid = false;
    int renderFailureRetainedFrame = -1;
    int renderFailureRetainedPosition = -1;
    quint64 renderFailureRetainedGeneration = 0;
    QSizeF renderFailureRetainedImageSize;
    QImage renderFailureRetainedImage;
    uint revision = 0;
};

struct RequestState
{
    QPointer<ImageSequence> sequence;
    std::shared_ptr<ImageSequence> sequenceOwner;
    ImageViewport::RequestStatus status = ImageViewport::RequestStatus::NoRequest;
    ImageViewport::RequestReason reason = ImageViewport::RequestReason::NoRequest;
    ImageViewport::CommandReason commandReason = ImageViewport::CommandReason::NoCommand;
    ImageViewport::PlaybackPhase playbackPhase = ImageViewport::PlaybackPhase::Stopped;
    bool looping = false;
    bool stopPlaybackWhenRequestReady = false;
    bool providerPlaybackStartPending = false;
    int currentFrame = -1;
    int requestedPosition = -1;
    int playbackPosition = -1;
    int latestNonPlaybackFrame = -1;
    int latestNonPlaybackPosition = -1;
    quint64 sequenceGeneration = 0;
    uint requestRevision = 0;
    uint commandRevision = 0;
    QString errorString;
    QString warningString;
};

struct ProviderGenerationState
{
    QPointer<ImageSequenceProviderSession> session;
    quint64 sessionSerial = 0;
    quint64 nextRequestToken = 0;
    ImageSequenceProviderRequestToken activeMetadataToken;
    ImageSequenceProviderRequestToken activeFrameToken;
    bool activeFrameFromPlayback = false;
    bool metadataReady = false;
    bool timedMetadata = false;
    QSizeF logicalSize;
    QVector<int> frameDurations;
};

}
