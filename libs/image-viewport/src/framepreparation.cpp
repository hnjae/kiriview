// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "framepreparation_p.h"

#include "imagesequence_p.h"
#include "imagesequencesource_p.h"
#include "imageviewportlimits_p.h"
#include "timingintervals_p.h"

#include <utility>

using namespace ImageViewportInternal;

namespace {

FramePreparation::ProviderMetadataAdmissionResult providerMetadataRejection(
    FramePreparation::ProviderMetadataAdmissionResult::Cause cause, QString diagnostic)
{
    return {
        cause,
        ImageViewportRequestStatus::Error,
        ImageViewportRequestReason::PayloadRejection,
        std::move(diagnostic),
    };
}

FramePreparation::ProviderKnownFactsAdmissionResult providerKnownFactsRejection(
    FramePreparation::ProviderKnownFactsAdmissionResult::Cause cause, QString diagnostic)
{
    return {
        cause,
        ImageSequenceFactoryOutcome::Rejected,
        std::move(diagnostic),
    };
}

FramePreparation::ProviderFrameAdmissionResult providerFrameRejection(
    FramePreparation::ProviderFrameAdmissionResult::Cause cause, ImageViewportRequestStatus status,
    QString diagnostic)
{
    return {
        cause,
        status,
        ImageViewportRequestReason::PayloadRejection,
        std::move(diagnostic),
    };
}

FramePreparation::ProviderFrameAdmissionResult providerFrameError(
    FramePreparation::ProviderFrameAdmissionResult::Cause cause, QString diagnostic)
{
    return providerFrameRejection(cause, ImageViewportRequestStatus::Error, std::move(diagnostic));
}

FramePreparation::BuiltInFrameAdmissionResult builtInFrameError(
    FramePreparation::BuiltInFrameAdmissionResult::Cause cause, QString diagnostic)
{
    return {
        cause,
        ImageViewportRequestStatus::Error,
        ImageViewportRequestReason::PayloadRejection,
        std::move(diagnostic),
    };
}

FramePreparation::BuiltInFrameAdmissionResult builtInFrameRejection(
    FramePreparation::BuiltInFrameAdmissionResult::Cause cause, QString diagnostic)
{
    return {
        cause,
        ImageViewportRequestStatus::Unsupported,
        ImageViewportRequestReason::PayloadRejection,
        std::move(diagnostic),
    };
}

bool positiveFinite(QSizeF size)
{
    return std::isfinite(size.width()) && std::isfinite(size.height()) && size.width() > 0.0
        && size.height() > 0.0;
}

bool validOrientationPolicy(ImageFrame::OrientationPolicy policy)
{
    switch (policy) {
    case ImageFrame::OrientationPolicy::Identity:
    case ImageFrame::OrientationPolicy::MirrorHorizontally:
    case ImageFrame::OrientationPolicy::MirrorVertically:
    case ImageFrame::OrientationPolicy::Rotate180:
    case ImageFrame::OrientationPolicy::Rotate90:
    case ImageFrame::OrientationPolicy::MirrorHorizontallyAndRotate90:
    case ImageFrame::OrientationPolicy::MirrorVerticallyAndRotate90:
    case ImageFrame::OrientationPolicy::Rotate270:
        return true;
    }
    return false;
}

enum class CommonPayloadCause {
    Accepted,
    Invalid,
    TooLarge,
    ExactnessMismatch,
};

struct CommonPayloadAdmission
{
    CommonPayloadCause cause = CommonPayloadCause::Accepted;
    QString diagnostic;
    PreparedPayload preparedPayload;
};

CommonPayloadAdmission admitPayload(const FramePayload& payload,
    const PreparedPayload& preparedPayload, ImageViewportExactnessPreference exactnessPreference,
    ImageViewportPageRole role, ResolvedFrameIdentity resolvedFrame, int frameDuration,
    ImageViewportDemandRevisionToken demandRevision)
{
    const auto& facts = payload.facts;
    const bool exactPair = (facts.quality == ImageViewportPayloadQuality::Exact)
        == (facts.exactness == ImageViewportPayloadExactness::ExactForSource);
    const QSizeF mapped(facts.sourceLogicalSize.width() * facts.sourceToPayloadScale.width(),
        facts.sourceLogicalSize.height() * facts.sourceToPayloadScale.height());
    if (payload.image.isNull() || !positiveFinite(facts.sourceLogicalSize)
        || !positiveFinite(facts.payloadRasterSize) || !positiveFinite(facts.sourceToPayloadScale)
        || facts.payloadRasterSize != QSizeF(payload.image.size())
        || facts.payloadByteSize < payload.image.sizeInBytes() || !exactPair
        || facts.hasAlpha != payload.image.hasAlphaChannel()
        || !validOrientationPolicy(facts.orientationPolicy)
        || qAbs(mapped.width() - facts.payloadRasterSize.width()) >= 0.0001
        || qAbs(mapped.height() - facts.payloadRasterSize.height()) >= 0.0001) {
        return { CommonPayloadCause::Invalid,
            QStringLiteral("frame payload facts are inconsistent") };
    }

    const qint64 logicalWidth = static_cast<qint64>(facts.sourceLogicalSize.width());
    const qint64 logicalHeight = static_cast<qint64>(facts.sourceLogicalSize.height());
    if (facts.sourceLogicalSize.width() > ImageSequenceLimits::maximumSourceLogicalWidth()
        || facts.sourceLogicalSize.height() > ImageSequenceLimits::maximumSourceLogicalHeight()
        || logicalWidth * logicalHeight > ImageSequenceLimits::maximumSourceLogicalPixels()
        || facts.payloadRasterSize.width() > ImageSequenceLimits::maximumPayloadRasterWidth()
        || facts.payloadRasterSize.height() > ImageSequenceLimits::maximumPayloadRasterHeight()
        || facts.payloadByteSize > ImageSequenceLimits::maximumPayloadBytes()
        || facts.formatIdentifier.toUcs4().size()
            > ImageSequenceLimits::maximumFormatIdentifierCharacters()) {
        return { CommonPayloadCause::TooLarge,
            QStringLiteral("frame payload exceeds an admission limit") };
    }
    if (exactnessPreference == ImageViewportExactnessPreference::RequireExact
        && facts.exactness != ImageViewportPayloadExactness::ExactForSource) {
        return { CommonPayloadCause::ExactnessMismatch,
            QStringLiteral("frame payload does not satisfy exactness preference") };
    }

    PreparedPayload admitted = preparedPayload;
    admitted.image = payload.image;
    admitted.sourceLogicalSize = facts.sourceLogicalSize;
    admitted.payloadRasterSize = facts.payloadRasterSize;
    admitted.sourceToPayloadScale = facts.sourceToPayloadScale;
    admitted.payloadByteSize = facts.payloadByteSize;
    admitted.quality = facts.quality;
    admitted.exactness = facts.exactness;
    admitted.hasAlpha = facts.hasAlpha;
    admitted.orientationPolicy = facts.orientationPolicy;
    admitted.formatIdentifier = facts.formatIdentifier;
    admitted.roleValid = true;
    admitted.role = role;
    admitted.resolvedFrame = resolvedFrame;
    admitted.frameDuration = frameDuration;
    admitted.demandRevision = demandRevision;
    return { CommonPayloadCause::Accepted, {}, std::move(admitted) };
}

} // namespace

bool FramePreparation::ProviderMetadataAdmissionResult::accepted() const
{
    return cause == Cause::Accepted;
}

bool FramePreparation::ProviderKnownFactsAdmissionResult::accepted() const
{
    return cause == Cause::Accepted;
}

bool FramePreparation::ProviderFrameAdmissionResult::accepted() const
{
    return cause == Cause::Accepted;
}

bool FramePreparation::BuiltInFrameAdmissionResult::accepted() const
{
    return cause == Cause::Accepted;
}

FramePreparation::ProviderMetadataAdmissionResult FramePreparation::admitProviderMetadata(
    const ImageSequenceProviderMetadata& metadata)
{
    using Cause = ProviderMetadataAdmissionResult::Cause;

    if (!metadata.isSpecified() || (!metadata.isStill() && !metadata.isTimedFrameList())
        || (metadata.hasAuthoredAnimationFacts() && !metadata.authoredAnimationFacts().isValid())) {
        return providerMetadataRejection(
            Cause::InvalidMetadata, QStringLiteral("provider metadata is invalid"));
    }
    if (metadata.isStill()
        && (metadata.timedPlaybackSupport() == ImageViewportCapabilitySupport::True
            || metadata.positionSeekSupport() == ImageViewportCapabilitySupport::True)) {
        return providerMetadataRejection(
            Cause::InvalidMetadata, QStringLiteral("provider metadata is invalid"));
    }

    const QSizeF size = metadata.sourceLogicalSize();
    if (!isPositiveFiniteInteger(size.width()) || !isPositiveFiniteInteger(size.height())) {
        return providerMetadataRejection(
            Cause::InvalidMetadata, QStringLiteral("provider metadata is invalid"));
    }
    if (size.width() > ImageSequenceLimits::maximumSourceLogicalWidth()) {
        return providerMetadataRejection(Cause::LogicalWidthTooLarge,
            QStringLiteral("provider metadata logical width exceeds maximumSourceLogicalWidth"));
    }
    if (size.height() > ImageSequenceLimits::maximumSourceLogicalHeight()) {
        return providerMetadataRejection(Cause::LogicalHeightTooLarge,
            QStringLiteral("provider metadata logical height exceeds maximumSourceLogicalHeight"));
    }

    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (width * height > ImageSequenceLimits::maximumSourceLogicalPixels()) {
        return providerMetadataRejection(Cause::PixelCountTooLarge,
            QStringLiteral("provider metadata logical size exceeds maximumSourceLogicalPixels"));
    }

    if (metadata.isStill()) {
        return {
            Cause::Accepted,
            ImageViewportRequestStatus::Ready,
            ImageViewportRequestReason::Ready,
            {},
            false,
            size,
        };
    }

    const QVector<int> durations = metadata.frameDurations();
    if (durations.isEmpty()) {
        return providerMetadataRejection(
            Cause::InvalidMetadata, QStringLiteral("provider metadata is invalid"));
    }
    if (durations.size() > ImageSequenceLimits::maximumFrameCount()) {
        return providerMetadataRejection(Cause::FrameCountTooLarge,
            QStringLiteral("provider metadata frame count exceeds maximumFrameCount"));
    }

    qint64 totalDuration = 0;
    for (int duration : durations) {
        if (duration <= 0) {
            return providerMetadataRejection(Cause::InvalidFrameDuration,
                QStringLiteral("provider metadata frame duration must be positive"));
        }
        if (duration > ImageSequenceLimits::maximumFrameDurationMilliseconds()) {
            return providerMetadataRejection(Cause::FrameDurationTooLarge,
                QStringLiteral(
                    "provider metadata frame duration exceeds maximumFrameDurationMilliseconds"));
        }
        totalDuration += duration;
        if (totalDuration > ImageSequenceLimits::maximumTotalDurationMilliseconds()) {
            return providerMetadataRejection(Cause::TotalDurationTooLarge,
                QStringLiteral(
                    "provider metadata total duration exceeds maximumTotalDurationMilliseconds"));
        }
    }

    return {
        Cause::Accepted,
        ImageViewportRequestStatus::Ready,
        ImageViewportRequestReason::Ready,
        {},
        true,
        size,
        TimingIntervals::fromFrameDurations(durations),
    };
}

FramePreparation::ProviderKnownFactsAdmissionResult FramePreparation::admitProviderKnownFacts(
    const ImageSequenceProviderKnownFacts& facts)
{
    using Cause = ProviderKnownFactsAdmissionResult::Cause;

    if (!facts.isSpecified()) {
        return {};
    }
    if (!facts.isValid()) {
        return providerKnownFactsRejection(
            Cause::InvalidFacts, QStringLiteral("provider known facts are invalid"));
    }

    const QSizeF size = facts.logicalSize();
    if (size.width() > ImageSequenceLimits::maximumSourceLogicalWidth()) {
        return providerKnownFactsRejection(Cause::LogicalWidthTooLarge,
            QStringLiteral("provider known facts logical width exceeds maximumSourceLogicalWidth"));
    }
    if (size.height() > ImageSequenceLimits::maximumSourceLogicalHeight()) {
        return providerKnownFactsRejection(Cause::LogicalHeightTooLarge,
            QStringLiteral(
                "provider known facts logical height exceeds maximumSourceLogicalHeight"));
    }

    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (width * height > ImageSequenceLimits::maximumSourceLogicalPixels()) {
        return providerKnownFactsRejection(Cause::PixelCountTooLarge,
            QStringLiteral("provider known facts logical size exceeds maximumSourceLogicalPixels"));
    }

    const int frameCount = facts.frameCount();
    if (frameCount > ImageSequenceLimits::maximumFrameCount()) {
        return providerKnownFactsRejection(Cause::FrameCountTooLarge,
            QStringLiteral("provider known facts frame count exceeds maximumFrameCount"));
    }

    if (!facts.isTimedFrameList()) {
        return {
            Cause::Accepted,
            ImageSequenceFactoryOutcome::Created,
            {},
            size,
            frameCount,
        };
    }

    const QVector<int> durations = facts.frameDurations();
    qint64 totalDuration = 0;
    for (int duration : durations) {
        if (duration > ImageSequenceLimits::maximumFrameDurationMilliseconds()) {
            return providerKnownFactsRejection(Cause::FrameDurationTooLarge,
                QStringLiteral("provider known facts frame duration exceeds "
                               "maximumFrameDurationMilliseconds"));
        }
        totalDuration += duration;
        if (totalDuration > ImageSequenceLimits::maximumTotalDurationMilliseconds()) {
            return providerKnownFactsRejection(Cause::TotalDurationTooLarge,
                QStringLiteral("provider known facts total duration exceeds "
                               "maximumTotalDurationMilliseconds"));
        }
    }

    return {
        Cause::Accepted,
        ImageSequenceFactoryOutcome::Created,
        {},
        size,
        frameCount,
        TimingIntervals::fromFrameDurations(durations),
    };
}

FramePreparation::ProviderFrameAdmissionResult FramePreparation::admitProviderFrame(
    ImageFrame* frame, const ImageSequenceProviderFrameEnvelope& envelope,
    const ProviderFrameState& state)
{
    using Cause = ProviderFrameAdmissionResult::Cause;

    if (!frame || !frame->isValid()) {
        return providerFrameError(
            Cause::InvalidFramePayload, QStringLiteral("provider frame payload is invalid"));
    }
    if (!state.metadataReady) {
        return providerFrameError(
            Cause::MetadataNotReady, QStringLiteral("provider frame metadata is not ready"));
    }
    if (frame->sourceLogicalSize() != state.logicalSize) {
        return providerFrameError(
            Cause::LogicalSizeMismatch, QStringLiteral("provider frame logical size mismatch"));
    }
    if (frame->payloadByteSize() <= 0) {
        return providerFrameError(Cause::InvalidPayloadByteSize,
            QStringLiteral("provider frame payload byte size is invalid"));
    }
    if (frame->payloadByteSize() > ImageSequenceLimits::maximumPayloadBytes()) {
        return providerFrameRejection(Cause::PayloadTooLarge,
            ImageViewportRequestStatus::Unsupported,
            QStringLiteral("provider frame payload exceeds maximumPayloadBytes"));
    }
    if ((state.maximumPayloadBytes >= 0 && frame->payloadByteSize() > state.maximumPayloadBytes)
        || (state.displayByteBudget >= 0 && frame->payloadByteSize() > state.displayByteBudget)) {
        return providerFrameRejection(Cause::PayloadTooLarge,
            ImageViewportRequestStatus::Unsupported,
            QStringLiteral("provider frame payload exceeds active display budget"));
    }
    if (frame->payloadRasterSize().width() > ImageSequenceLimits::maximumPayloadRasterWidth()
        || frame->payloadRasterSize().height()
            > ImageSequenceLimits::maximumPayloadRasterHeight()) {
        return providerFrameRejection(Cause::PayloadTooLarge,
            ImageViewportRequestStatus::Unsupported,
            QStringLiteral("provider frame payload exceeds maximumPayloadRaster size"));
    }
    if (state.maximumTextureSize >= 0
        && (frame->payloadRasterSize().width() > state.maximumTextureSize
            || frame->payloadRasterSize().height() > state.maximumTextureSize)) {
        return providerFrameRejection(Cause::PayloadTooLarge,
            ImageViewportRequestStatus::Unsupported,
            QStringLiteral("provider frame payload exceeds active texture cap"));
    }
    if (frame->formatIdentifier().toUcs4().size()
        > ImageSequenceLimits::maximumFormatIdentifierCharacters()) {
        return providerFrameRejection(Cause::PayloadTooLarge,
            ImageViewportRequestStatus::Unsupported,
            QStringLiteral(
                "provider frame format identifier exceeds maximumFormatIdentifierCharacters"));
    }
    if (!envelope.isValid()) {
        return providerFrameError(
            Cause::InvalidFrameMetadata, QStringLiteral("provider frame metadata is invalid"));
    }
    const ImageViewportDemandRevisionToken payloadDemand = envelope.demandRevision();
    if (!state.demandRevision.isValid() || !payloadDemand.isValid()
        || payloadDemand != state.demandRevision) {
        return providerFrameError(Cause::DemandRevisionMismatch,
            QStringLiteral("provider frame demand revision mismatch"));
    }
    if (state.exactnessPreference == ImageViewportExactnessPreference::RequireExact
        && frame->exactness() != ImageViewportPayloadExactness::ExactForSource) {
        return providerFrameRejection(Cause::ExactnessMismatch,
            ImageViewportRequestStatus::Unsupported,
            QStringLiteral("provider frame does not satisfy exactness preference"));
    }

    if (state.timedMetadata) {
        if (!envelope.isTimedFrame() || envelope.frame() != state.resolvedFrame.frame) {
            if (!envelope.isTimedFrame()) {
                return providerFrameError(Cause::InvalidFrameMetadata,
                    QStringLiteral("provider frame metadata is invalid"));
            }
            return providerFrameError(Cause::ResolvedFrameMismatch,
                QStringLiteral("provider frame resolved frame mismatch"));
        }
        if (envelope.frameStartPosition() != state.resolvedFrame.position) {
            return providerFrameError(Cause::FrameStartMismatch,
                QStringLiteral("provider frame start position mismatch"));
        }
        const int expectedFrameDuration
            = state.timingIntervals.frameDuration(state.resolvedFrame.frame);
        if (envelope.frameDuration() != expectedFrameDuration) {
            return providerFrameError(
                Cause::FrameDurationMismatch, QStringLiteral("provider frame duration mismatch"));
        }
        const FramePayload payload { ImageFramePrivateAccess::image(*frame),
            { frame->sourceLogicalSize(), frame->payloadRasterSize(), frame->sourceToPayloadScale(),
                frame->payloadByteSize(), frame->quality(), frame->exactness(), frame->hasAlpha(),
                frame->orientationPolicy(), frame->formatIdentifier() } };
        const auto common = admitPayload(payload, state.preparedPayload, state.exactnessPreference,
            state.role, state.resolvedFrame, expectedFrameDuration, envelope.demandRevision());
        if (common.cause != CommonPayloadCause::Accepted) {
            return providerFrameError(Cause::InvalidFramePayload, common.diagnostic);
        }
        return {
            Cause::Accepted,
            ImageViewportRequestStatus::Ready,
            ImageViewportRequestReason::Ready,
            {},
            common.preparedPayload,
        };
    }

    if (!envelope.isStillFrame()) {
        return providerFrameError(
            Cause::InvalidFrameMetadata, QStringLiteral("provider frame metadata is invalid"));
    }
    if (envelope.frame() != 0) {
        return providerFrameError(
            Cause::ResolvedFrameMismatch, QStringLiteral("provider frame resolved frame mismatch"));
    }
    const FramePayload payload { ImageFramePrivateAccess::image(*frame),
        { frame->sourceLogicalSize(), frame->payloadRasterSize(), frame->sourceToPayloadScale(),
            frame->payloadByteSize(), frame->quality(), frame->exactness(), frame->hasAlpha(),
            frame->orientationPolicy(), frame->formatIdentifier() } };
    const auto common = admitPayload(payload, state.preparedPayload, state.exactnessPreference,
        state.role, state.resolvedFrame, -1, envelope.demandRevision());
    if (common.cause != CommonPayloadCause::Accepted) {
        return providerFrameError(Cause::InvalidFramePayload, common.diagnostic);
    }
    return {
        Cause::Accepted,
        ImageViewportRequestStatus::Ready,
        ImageViewportRequestReason::Ready,
        {},
        common.preparedPayload,
    };
}

FramePreparation::BuiltInFrameAdmissionResult FramePreparation::admitBuiltInFrame(
    const ImageViewportInternal::ImageSequenceSource& source, int frame,
    const ImageViewportInternal::PreparedPayload& preparedPayload,
    ImageViewportExactnessPreference exactnessPreference, ImageViewportPageRole role)
{
    using Cause = BuiltInFrameAdmissionResult::Cause;

    const FramePayload payload = sourceFramePayload(source, frame);
    if (payload.image.isNull()) {
        return builtInFrameError(
            Cause::InvalidFramePayload, QStringLiteral("built-in frame payload is invalid"));
    }

    const int frameStart = source.facts.timed ? sourceFrameStartPosition(source, frame) : -1;
    const TimingIntervals timing = ImageSequencePrivateAccess::timingIntervals(source.sequence);
    const int frameDuration = source.facts.timed ? timing.frameDuration(frame) : -1;
    const auto admission = admitPayload(payload, preparedPayload, exactnessPreference, role,
        { frame, frameStart }, frameDuration, {});
    if (admission.cause == CommonPayloadCause::ExactnessMismatch) {
        return builtInFrameRejection(Cause::ExactnessMismatch, admission.diagnostic);
    }
    if (admission.cause == CommonPayloadCause::TooLarge) {
        return builtInFrameRejection(Cause::PayloadTooLarge, admission.diagnostic);
    }
    if (admission.cause != CommonPayloadCause::Accepted) {
        return builtInFrameError(Cause::InvalidFramePayload, admission.diagnostic);
    }
    return {
        Cause::Accepted,
        ImageViewportRequestStatus::Ready,
        ImageViewportRequestReason::Ready,
        {},
        admission.preparedPayload,
    };
}
