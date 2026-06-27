#pragma once

#include "imageviewport.h"
#include "timingintervals_p.h"

class FramePreparation
{
public:
    struct ProviderMetadataAdmissionResult
    {
        enum class Cause {
            Accepted,
            InvalidMetadata,
            LogicalWidthTooLarge,
            LogicalHeightTooLarge,
            PixelCountTooLarge,
            FrameCountTooLarge,
            InvalidFrameDuration,
            FrameDurationTooLarge,
            TotalDurationTooLarge,
        };

        Cause cause = Cause::Accepted;
        ImageViewport::RequestStatus status = ImageViewport::RequestStatus::Ready;
        ImageViewport::RequestReason reason = ImageViewport::RequestReason::Ready;
        QString diagnostic;
        bool timedMetadata = false;
        QSizeF logicalSize;
        TimingIntervals timingIntervals;

        bool accepted() const;
    };

    struct ProviderKnownFactsAdmissionResult
    {
        enum class Cause {
            Accepted,
            InvalidFacts,
            LogicalWidthTooLarge,
            LogicalHeightTooLarge,
            PixelCountTooLarge,
            FrameCountTooLarge,
            FrameDurationTooLarge,
            TotalDurationTooLarge,
        };

        Cause cause = Cause::Accepted;
        ImageSequenceFactoryResult::FactoryOutcome outcome
            = ImageSequenceFactoryResult::FactoryOutcome::Created;
        QString diagnostic;
        QSizeF logicalSize;
        int frameCount = -1;
        TimingIntervals timingIntervals;

        bool accepted() const;
    };

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

    static ProviderMetadataAdmissionResult admitProviderMetadata(
        const ImageSequenceProviderMetadata& metadata);
    static ProviderKnownFactsAdmissionResult admitProviderKnownFacts(
        const ImageSequenceProviderKnownFacts& facts);
    static ProviderFrameAdmissionResult admitProviderFrame(ImageFrame* frame,
        ImageSequenceProviderFrameMetadata metadata, const ProviderFrameState& state);
    static int providerFrameStartPosition(const QVector<int>& frameDurations, int frame);
    static int providerFrameIndexForPosition(const QVector<int>& frameDurations, int position);
    static int totalDuration(const QVector<int>& frameDurations);
    static QString boundedDiagnostic(QString diagnostic, QString fallback);
};
