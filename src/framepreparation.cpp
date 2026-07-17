#include "framepreparation_p.h"

#include "imagesequence_p.h"
#include "imagesequencesource_p.h"
#include "imageviewportdiagnostics_p.h"
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
    if (state.demandRevision.isValid() && payloadDemand.isValid()
        && payloadDemand != state.demandRevision) {
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
        ImageViewportInternal::PreparedPayload admitted = state.preparedPayload;
        admitted.image = ImageFramePrivateAccess::image(*frame);
        admitted.sourceLogicalSize = frame->sourceLogicalSize();
        admitted.payloadRasterSize = frame->payloadRasterSize();
        admitted.sourceToPayloadScale = frame->sourceToPayloadScale();
        admitted.payloadByteSize = frame->payloadByteSize();
        admitted.quality = frame->quality();
        admitted.exactness = frame->exactness();
        admitted.demandRevision = envelope.demandRevision();
        return {
            Cause::Accepted,
            ImageViewportRequestStatus::Ready,
            ImageViewportRequestReason::Ready,
            {},
            admitted,
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
    ImageViewportInternal::PreparedPayload admitted = state.preparedPayload;
    admitted.image = ImageFramePrivateAccess::image(*frame);
    admitted.sourceLogicalSize = frame->sourceLogicalSize();
    admitted.payloadRasterSize = frame->payloadRasterSize();
    admitted.sourceToPayloadScale = frame->sourceToPayloadScale();
    admitted.payloadByteSize = frame->payloadByteSize();
    admitted.quality = frame->quality();
    admitted.exactness = frame->exactness();
    admitted.demandRevision = envelope.demandRevision();
    return {
        Cause::Accepted,
        ImageViewportRequestStatus::Ready,
        ImageViewportRequestReason::Ready,
        {},
        admitted,
    };
}

FramePreparation::BuiltInFrameAdmissionResult FramePreparation::admitBuiltInFrame(
    const ImageViewportInternal::ImageSequenceSource& source, int frame,
    const ImageViewportInternal::PreparedPayload& preparedPayload)
{
    using Cause = BuiltInFrameAdmissionResult::Cause;

    QImage image = sourceFrameImage(source, frame);
    if (image.isNull()) {
        return builtInFrameError(
            Cause::InvalidFramePayload, QStringLiteral("built-in frame payload is invalid"));
    }

    ImageViewportInternal::PreparedPayload admittedPayload = preparedPayload;
    admittedPayload.image = std::move(image);
    const FramePayloadFacts facts = sourceFramePayloadFacts(source, frame);
    admittedPayload.sourceLogicalSize = facts.sourceLogicalSize;
    admittedPayload.payloadRasterSize = facts.payloadRasterSize;
    admittedPayload.sourceToPayloadScale = facts.sourceToPayloadScale;
    admittedPayload.payloadByteSize = facts.payloadByteSize;
    admittedPayload.quality = facts.quality;
    admittedPayload.exactness = facts.exactness;
    admittedPayload.demandRevision = facts.demandRevision;
    return {
        Cause::Accepted,
        ImageViewportRequestStatus::Ready,
        ImageViewportRequestReason::Ready,
        {},
        admittedPayload,
    };
}

QString FramePreparation::boundedDiagnostic(QString diagnostic, QString fallback)
{
    QString selected = plainTextDiagnostic(redactDiagnosticDetails(
        diagnostic.isEmpty() ? std::move(fallback) : std::move(diagnostic)));
    const auto scalars = selected.toUcs4();
    const int maximumLength = ImageSequenceLimits::maximumDiagnosticCharacters();
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
