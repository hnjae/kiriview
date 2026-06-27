#pragma once

#include "imageviewport.h"

class FramePreparation
{
public:
    struct ProviderFrameState
    {
        bool metadataReady = false;
        bool timedMetadata = false;
        QSizeF logicalSize;
        QVector<int> frameDurations;
        int currentFrame = -1;
    };

    struct ProviderFrameAdmissionResult
    {
        enum class Cause {
            Accepted,
            InvalidFramePayload,
            MetadataNotReady,
            LogicalSizeMismatch,
            InvalidPayloadByteSize,
            PayloadTooLarge,
            InvalidFrameMetadata,
            ResolvedFrameMismatch,
            FrameStartMismatch,
            FrameDurationMismatch,
        };

        Cause cause = Cause::Accepted;
        ImageViewport::RequestStatus status = ImageViewport::RequestStatus::Ready;
        ImageViewport::RequestReason reason = ImageViewport::RequestReason::Ready;
        QString diagnostic;

        bool accepted() const;
    };

    static bool validateProviderStillMetadata(const ImageSequenceProviderMetadata& metadata);
    static bool validateProviderTimedMetadata(const ImageSequenceProviderMetadata& metadata);
    static ProviderFrameAdmissionResult admitProviderFrame(ImageFrame* frame,
        ImageSequenceProviderFrameMetadata metadata, const ProviderFrameState& state);
    static int providerFrameStartPosition(const QVector<int>& frameDurations, int frame);
    static int providerFrameIndexForPosition(const QVector<int>& frameDurations, int position);
    static int totalDuration(const QVector<int>& frameDurations);
    static QString boundedDiagnostic(QString diagnostic, QString fallback);
};
