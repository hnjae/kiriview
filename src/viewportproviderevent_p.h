#pragma once

#include "imageviewport.h"

struct ViewportProviderEvent
{
    enum class Kind {
        MetadataReady,
        ImageFrameReady,
        ImageFrameWithMetadataReady,
        FrameHandleReady,
        FrameHandleWithMetadataReady,
        Waiting,
        Progress,
        EndOfSequence,
        Failure,
        Unsupported,
        Cancellation
    };

    Kind kind = Kind::Waiting;
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    quint64 sessionSerial = 0;
    quint64 generation = 0;
    ImageSequenceProviderRequestToken token;
    ImageSequenceProviderMetadata metadata;
    ImageFrame* imageFrame = nullptr;
    ImageSequenceProviderFrameHandle* frameHandle = nullptr;
    ImageSequenceProviderFrameMetadata frameMetadata;
    double progress = 0.0;
    ImageSequenceProviderSession::UnsupportedCause unsupportedCause
        = ImageSequenceProviderSession::UnsupportedCause::PayloadRejection;
    bool unsupportedCauseExplicit = false;
    QString diagnostic;
};
