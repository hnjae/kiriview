#pragma once

#include "imageviewport.h"

#include <utility>

namespace {

ImageSequenceProviderUnsupportedCause typedUnsupportedCause(
    ImageSequenceProviderSession::UnsupportedCause cause)
{
    switch (cause) {
    case ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest:
        return ImageSequenceProviderUnsupportedCause::UnsupportedRequest;
    case ImageSequenceProviderSession::UnsupportedCause::PayloadRejection:
        return ImageSequenceProviderUnsupportedCause::PayloadRejection;
    }
    return ImageSequenceProviderUnsupportedCause::PayloadRejection;
}

ImageSequenceProviderFrameEnvelope providerFrameEnvelopeForEvent(
    ImageFrame* frame, const ImageSequenceProviderFrameMetadata& metadata)
{
    ImageSequenceProviderFrameEnvelope envelope;
    if (frame) {
        envelope = frame->envelope();
    }
    if (metadata.isTimedFrame()) {
        envelope.setFrame(metadata.frame());
        envelope.setFrameStartPosition(metadata.frameStartPosition());
        envelope.setFrameDuration(metadata.frameDuration());
    } else if (metadata.isStillFrame()) {
        envelope.setFrame(0);
        envelope.setFrameStartPosition(-1);
        envelope.setFrameDuration(-1);
    } else {
        envelope.setFrame(-1);
    }
    return envelope;
}

void emitProviderMetadataReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderMetadata metadata)
{
    emit session->providerEvent(
        ImageSequenceProviderEvent::metadataReady(token, std::move(metadata)));
}

void emitProviderFrameHandleReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* handle,
    ImageSequenceProviderFrameMetadata metadata = ImageSequenceProviderFrameMetadata::stillFrame())
{
    ImageSequenceProviderEvent event = ImageSequenceProviderEvent::frameReady(
        token, handle, providerFrameEnvelopeForEvent(handle ? handle->frame() : nullptr, metadata));
    emit session->providerEvent(event);
}

void emitProviderFrameReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata = ImageSequenceProviderFrameMetadata::stillFrame())
{
    auto* handle = new ImageSequenceProviderFrameHandle(frame, [](ImageFrame*) {});
    emitProviderFrameHandleReady(session, token, handle, metadata);
}

void emitProviderWaiting(
    ImageSequenceProviderSession* session, ImageSequenceProviderRequestToken token)
{
    emit session->providerEvent(ImageSequenceProviderEvent::waiting(token));
}

void emitProviderProgress(
    ImageSequenceProviderSession* session, ImageSequenceProviderRequestToken token, double progress)
{
    emit session->providerEvent(ImageSequenceProviderEvent::progress(token, progress));
}

void emitProviderEndOfSequence(
    ImageSequenceProviderSession* session, ImageSequenceProviderRequestToken token)
{
    emit session->providerEvent(ImageSequenceProviderEvent::endOfSequence(token));
}

void emitProviderFailed(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, QString diagnostic)
{
    emit session->providerEvent(ImageSequenceProviderEvent::failed(token, std::move(diagnostic)));
}

void emitProviderUnsupported(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderUnsupportedCause cause,
    QString diagnostic)
{
    emit session->providerEvent(
        ImageSequenceProviderEvent::unsupported(token, cause, std::move(diagnostic)));
}

void emitProviderUnsupported(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderSession::UnsupportedCause cause,
    QString diagnostic)
{
    emitProviderUnsupported(session, token, typedUnsupportedCause(cause), std::move(diagnostic));
}

void emitProviderCancelled(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, QString diagnostic)
{
    emit session->providerEvent(
        ImageSequenceProviderEvent::cancelled(token, std::move(diagnostic)));
}

}
