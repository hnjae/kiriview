#pragma once

#include <ImageViewport/ImageViewport>

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
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    quint64 sessionSerial = 0;
    quint64 generation = 0;
    ImageSequenceProviderRequestToken token;
    ImageSequenceProviderMetadata metadata;
    ImageFrame* imageFrame = nullptr;
    ImageSequenceProviderFrameHandle* frameHandle = nullptr;
    quint64 frameLeaseId = 0;
    ImageSequenceProviderFrameEnvelope frameEnvelope;
    double progress = 0.0;
    ImageSequenceProviderUnsupportedCause unsupportedCause
        = ImageSequenceProviderUnsupportedCause::PayloadRejection;
    bool unsupportedCauseExplicit = false;
    QString diagnostic;
};
