#pragma once

#include <ImageViewport/ImageViewport>

#include <utility>

namespace {

void emitProviderMetadataReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderMetadata metadata)
{
    emit session->providerEvent(
        ImageSequenceProviderEvent::metadataReady(token, std::move(metadata)));
}

void emitProviderFrameHandleReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* handle,
    ImageSequenceProviderFrameEnvelope envelope)
{
    ImageSequenceProviderEvent event
        = ImageSequenceProviderEvent::frameReady(token, handle, std::move(envelope));
    emit session->providerEvent(event);
}

void emitProviderFrameReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageFrame* frame,
    ImageSequenceProviderFrameEnvelope envelope)
{
    auto* handle = new ImageSequenceProviderFrameHandle(frame, [](ImageFrame*) { });
    emitProviderFrameHandleReady(session, token, handle, std::move(envelope));
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

void emitProviderCancelled(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, QString diagnostic)
{
    emit session->providerEvent(
        ImageSequenceProviderEvent::cancelled(token, std::move(diagnostic)));
}

}
