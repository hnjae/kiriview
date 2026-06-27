#include "framepreparation_p.h"

#include "imageviewporthelpers_p.h"
#include "timingintervals_p.h"

#include <utility>

using namespace ImageViewportInternal;

namespace {

FramePreparation::ProviderMetadataAdmissionResult providerMetadataRejection(
    FramePreparation::ProviderMetadataAdmissionResult::Cause cause, QString diagnostic)
{
    return {
        cause,
        ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::PayloadRejection,
        std::move(diagnostic),
    };
}

FramePreparation::ProviderKnownFactsAdmissionResult providerKnownFactsRejection(
    FramePreparation::ProviderKnownFactsAdmissionResult::Cause cause, QString diagnostic)
{
    return {
        cause,
        ImageSequenceFactoryResult::FactoryOutcome::Invalid,
        std::move(diagnostic),
    };
}

FramePreparation::ProviderFrameAdmissionResult providerFrameRejection(
    FramePreparation::ProviderFrameAdmissionResult::Cause cause,
    ImageViewport::RequestStatus status, QString diagnostic)
{
    return {
        cause,
        status,
        ImageViewport::RequestReason::PayloadRejection,
        std::move(diagnostic),
    };
}

FramePreparation::ProviderFrameAdmissionResult providerFrameError(
    FramePreparation::ProviderFrameAdmissionResult::Cause cause, QString diagnostic)
{
    return providerFrameRejection(
        cause, ImageViewport::RequestStatus::Error, std::move(diagnostic));
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

FramePreparation::ProviderMetadataAdmissionResult FramePreparation::admitProviderMetadata(
    const ImageSequenceProviderMetadata& metadata)
{
    using Cause = ProviderMetadataAdmissionResult::Cause;

    if (!metadata.isSpecified() || (!metadata.isStill() && !metadata.isTimedFrameList())) {
        return providerMetadataRejection(
            Cause::InvalidMetadata, QStringLiteral("provider metadata is invalid"));
    }
    if (metadata.isStill() && (metadata.timedPlaybackSupport() || metadata.positionSeekSupport())) {
        return providerMetadataRejection(
            Cause::InvalidMetadata, QStringLiteral("provider metadata is invalid"));
    }

    const QSizeF size = metadata.logicalSize();
    if (!isPositiveFiniteInteger(size.width()) || !isPositiveFiniteInteger(size.height())) {
        return providerMetadataRejection(
            Cause::InvalidMetadata, QStringLiteral("provider metadata is invalid"));
    }
    if (size.width() > ImageSequenceLimits::maximumLogicalWidth()) {
        return providerMetadataRejection(Cause::LogicalWidthTooLarge,
            QStringLiteral("provider metadata logical width exceeds maximumLogicalWidth"));
    }
    if (size.height() > ImageSequenceLimits::maximumLogicalHeight()) {
        return providerMetadataRejection(Cause::LogicalHeightTooLarge,
            QStringLiteral("provider metadata logical height exceeds maximumLogicalHeight"));
    }

    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (width * height > ImageSequenceLimits::maximumPixelsPerFrame()) {
        return providerMetadataRejection(Cause::PixelCountTooLarge,
            QStringLiteral("provider metadata logical size exceeds maximumPixelsPerFrame"));
    }

    if (metadata.isStill()) {
        return {
            Cause::Accepted,
            ImageViewport::RequestStatus::Ready,
            ImageViewport::RequestReason::Ready,
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
    if (durations.size() > ImageSequenceLimits::maximumTimedListFrameCount()) {
        return providerMetadataRejection(Cause::FrameCountTooLarge,
            QStringLiteral("provider metadata frame count exceeds maximumTimedListFrameCount"));
    }

    qint64 totalDuration = 0;
    for (int duration : durations) {
        if (duration <= 0) {
            return providerMetadataRejection(Cause::InvalidFrameDuration,
                QStringLiteral("provider metadata frame duration must be positive"));
        }
        if (duration > ImageSequenceLimits::maximumFrameDuration()) {
            return providerMetadataRejection(Cause::FrameDurationTooLarge,
                QStringLiteral("provider metadata frame duration exceeds maximumFrameDuration"));
        }
        totalDuration += duration;
        if (totalDuration > ImageSequenceLimits::maximumTotalSequenceDuration()) {
            return providerMetadataRejection(Cause::TotalDurationTooLarge,
                QStringLiteral(
                    "provider metadata total duration exceeds maximumTotalSequenceDuration"));
        }
    }

    return {
        Cause::Accepted,
        ImageViewport::RequestStatus::Ready,
        ImageViewport::RequestReason::Ready,
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
    if (size.width() > ImageSequenceLimits::maximumLogicalWidth()) {
        return providerKnownFactsRejection(Cause::LogicalWidthTooLarge,
            QStringLiteral("provider known facts logical width exceeds maximumLogicalWidth"));
    }
    if (size.height() > ImageSequenceLimits::maximumLogicalHeight()) {
        return providerKnownFactsRejection(Cause::LogicalHeightTooLarge,
            QStringLiteral("provider known facts logical height exceeds maximumLogicalHeight"));
    }

    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (width * height > ImageSequenceLimits::maximumPixelsPerFrame()) {
        return providerKnownFactsRejection(Cause::PixelCountTooLarge,
            QStringLiteral("provider known facts logical size exceeds maximumPixelsPerFrame"));
    }

    const int frameCount = facts.frameCount();
    if (frameCount > ImageSequenceLimits::maximumTimedListFrameCount()) {
        return providerKnownFactsRejection(Cause::FrameCountTooLarge,
            QStringLiteral("provider known facts frame count exceeds maximumTimedListFrameCount"));
    }

    if (!facts.isTimedFrameList()) {
        return {
            Cause::Accepted,
            ImageSequenceFactoryResult::FactoryOutcome::Created,
            {},
            size,
            frameCount,
        };
    }

    const QVector<int> durations = facts.frameDurations();
    qint64 totalDuration = 0;
    for (int duration : durations) {
        if (duration > ImageSequenceLimits::maximumFrameDuration()) {
            return providerKnownFactsRejection(Cause::FrameDurationTooLarge,
                QStringLiteral(
                    "provider known facts frame duration exceeds maximumFrameDuration"));
        }
        totalDuration += duration;
        if (totalDuration > ImageSequenceLimits::maximumTotalSequenceDuration()) {
            return providerKnownFactsRejection(Cause::TotalDurationTooLarge,
                QStringLiteral(
                    "provider known facts total duration exceeds maximumTotalSequenceDuration"));
        }
    }

    return {
        Cause::Accepted,
        ImageSequenceFactoryResult::FactoryOutcome::Created,
        {},
        size,
        frameCount,
        TimingIntervals::fromFrameDurations(durations),
    };
}

FramePreparation::ProviderFrameAdmissionResult FramePreparation::admitProviderFrame(
    ImageFrame* frame, ImageSequenceProviderFrameMetadata metadata, const ProviderFrameState& state)
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
    if (frame->logicalSize() != state.logicalSize) {
        return providerFrameError(
            Cause::LogicalSizeMismatch, QStringLiteral("provider frame logical size mismatch"));
    }
    if (frame->payloadByteSize() <= 0) {
        return providerFrameError(Cause::InvalidPayloadByteSize,
            QStringLiteral("provider frame payload byte size is invalid"));
    }
    if (frame->payloadByteSize() > ImageSequenceLimits::maximumPayloadBytesPerFrame()) {
        return providerFrameRejection(Cause::PayloadTooLarge,
            ImageViewport::RequestStatus::Unsupported,
            QStringLiteral("provider frame payload exceeds maximumPayloadBytesPerFrame"));
    }
    if (!metadata.isValid()) {
        return providerFrameError(
            Cause::InvalidFrameMetadata, QStringLiteral("provider frame metadata is invalid"));
    }

    if (state.timedMetadata) {
        if (!metadata.isTimedFrame() || metadata.frame() != state.currentFrame) {
            if (!metadata.isTimedFrame()) {
                return providerFrameError(Cause::InvalidFrameMetadata,
                    QStringLiteral("provider frame metadata is invalid"));
            }
            return providerFrameError(Cause::ResolvedFrameMismatch,
                QStringLiteral("provider frame resolved frame mismatch"));
        }
        if (metadata.frameStartPosition()
            != providerFrameStartPosition(state.frameDurations, state.currentFrame)) {
            return providerFrameError(Cause::FrameStartMismatch,
                QStringLiteral("provider frame start position mismatch"));
        }
        if (metadata.frameDuration() != -1
            && metadata.frameDuration() != state.frameDurations.at(state.currentFrame)) {
            return providerFrameError(
                Cause::FrameDurationMismatch, QStringLiteral("provider frame duration mismatch"));
        }
        return {};
    }

    if (!metadata.isStillFrame()) {
        return providerFrameError(
            Cause::InvalidFrameMetadata, QStringLiteral("provider frame metadata is invalid"));
    }
    if (metadata.frame() != 0) {
        return providerFrameError(
            Cause::ResolvedFrameMismatch, QStringLiteral("provider frame resolved frame mismatch"));
    }
    return {};
}

int FramePreparation::providerFrameStartPosition(const QVector<int>& frameDurations, int frame)
{
    return TimingIntervals::fromFrameDurations(frameDurations).frameStartPosition(frame);
}

int FramePreparation::providerFrameIndexForPosition(
    const QVector<int>& frameDurations, int position)
{
    return TimingIntervals::fromFrameDurations(frameDurations).frameIndexForPosition(position);
}

int FramePreparation::totalDuration(const QVector<int>& frameDurations)
{
    return TimingIntervals::fromFrameDurations(frameDurations).totalDuration();
}

QString FramePreparation::boundedDiagnostic(QString diagnostic, QString fallback)
{
    QString selected = plainTextDiagnostic(redactDiagnosticDetails(
        diagnostic.isEmpty() ? std::move(fallback) : std::move(diagnostic)));
    const auto scalars = selected.toUcs4();
    const int maximumLength = ImageSequenceLimits::maximumDiagnosticStringLength();
    if (scalars.size() <= maximumLength) {
        return selected;
    }
    QString bounded;
    bounded.reserve(selected.size());
    for (int i = 0; i < maximumLength; ++i) {
        const char32_t scalar = static_cast<char32_t>(scalars.at(i));
        bounded += QString::fromUcs4(&scalar, 1);
    }
    return bounded;
}
