/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imageviewportstate_p.h"
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
        ImageViewportRequestStatus status = ImageViewportRequestStatus::Ready;
        ImageViewportRequestReason reason = ImageViewportRequestReason::Ready;
        QString diagnostic;
        bool timedMetadata = false;
        QSizeF logicalSize;
        TimingIntervals timingIntervals;

        [[nodiscard]] bool accepted() const;
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
        ImageSequenceFactoryOutcome outcome = ImageSequenceFactoryOutcome::Created;
        QString diagnostic;
        QSizeF logicalSize;
        int frameCount = -1;
        TimingIntervals timingIntervals;

        [[nodiscard]] bool accepted() const;
    };

    struct ProviderFrameState
    {
        bool metadataReady = false;
        bool timedMetadata = false;
        QSizeF logicalSize;
        TimingIntervals timingIntervals;
        ImageViewportInternal::ResolvedFrameIdentity resolvedFrame;
        ImageViewportPageRole role = ImageViewportPageRole::Primary;
        ImageViewportInternal::PreparedPayload preparedPayload;
        ImageViewportDemandRevisionToken demandRevision;
        qint64 maximumTextureSize = -1;
        qint64 maximumPayloadBytes = -1;
        qint64 displayByteBudget = -1;
        ImageViewportExactnessPreference exactnessPreference
            = ImageViewportExactnessPreference::Default;
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
            DemandRevisionMismatch,
            ExactnessMismatch,
        };

        Cause cause = Cause::Accepted;
        ImageViewportRequestStatus status = ImageViewportRequestStatus::Ready;
        ImageViewportRequestReason reason = ImageViewportRequestReason::Ready;
        QString diagnostic;
        ImageViewportInternal::PreparedPayload preparedPayload;

        [[nodiscard]] bool accepted() const;
    };

    struct BuiltInFrameAdmissionResult
    {
        enum class Cause {
            Accepted,
            InvalidFramePayload,
            PayloadTooLarge,
            ExactnessMismatch,
        };

        Cause cause = Cause::Accepted;
        ImageViewportRequestStatus status = ImageViewportRequestStatus::Ready;
        ImageViewportRequestReason reason = ImageViewportRequestReason::Ready;
        QString diagnostic;
        ImageViewportInternal::PreparedPayload preparedPayload;

        [[nodiscard]] bool accepted() const;
    };

    static ProviderMetadataAdmissionResult admitProviderMetadata(
        const ImageSequenceProviderMetadata& metadata);
    static ProviderKnownFactsAdmissionResult admitProviderKnownFacts(
        const ImageViewportInternal::ImageSequenceProviderKnownFacts& facts);
    static ProviderFrameAdmissionResult admitProviderFrame(ImageFrame* frame,
        const ImageSequenceProviderFrameEnvelope& envelope, const ProviderFrameState& state);
    static BuiltInFrameAdmissionResult admitBuiltInFrame(
        const ImageViewportInternal::ImageSequenceSource& source, int frame,
        const ImageViewportInternal::PreparedPayload& preparedPayload,
        ImageViewportExactnessPreference exactnessPreference,
        ImageViewportPageRole role = ImageViewportPageRole::Primary);
};
