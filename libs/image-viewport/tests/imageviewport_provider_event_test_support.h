/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <ImageViewport/ImageViewport>

#include <utility>

namespace {

void emitProviderMetadataReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderMetadata metadata)
{
    Q_EMIT session->providerEvent(
        ImageSequenceProviderEvent::metadataReady(token, std::move(metadata)));
}

void emitProviderFrameHandleReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* handle,
    ImageSequenceProviderFrameEnvelope envelope)
{
    ImageSequenceProviderEvent event
        = ImageSequenceProviderEvent::frameReady(token, handle, envelope);
    Q_EMIT session->providerEvent(event);
}

void emitProviderProvisionalFrameHandleReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* handle,
    ImageSequenceProviderFrameEnvelope envelope)
{
    ImageSequenceProviderEvent event
        = ImageSequenceProviderEvent::provisionalFrameReady(token, handle, envelope);
    Q_EMIT session->providerEvent(event);
}

void emitProviderFrameReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageFrame* frame,
    ImageSequenceProviderFrameEnvelope envelope)
{
    auto* handle = new ImageSequenceProviderFrameHandle(frame, [](ImageFrame*) { });
    emitProviderFrameHandleReady(session, token, handle, envelope);
}

void emitProviderProvisionalFrameReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageFrame* frame,
    ImageSequenceProviderFrameEnvelope envelope)
{
    auto* handle = new ImageSequenceProviderFrameHandle(frame, [](ImageFrame*) { });
    emitProviderProvisionalFrameHandleReady(session, token, handle, envelope);
}

void emitProviderWaiting(
    ImageSequenceProviderSession* session, ImageSequenceProviderRequestToken token)
{
    Q_EMIT session->providerEvent(ImageSequenceProviderEvent::waiting(token));
}

void emitProviderProgress(
    ImageSequenceProviderSession* session, ImageSequenceProviderRequestToken token, double progress)
{
    Q_EMIT session->providerEvent(ImageSequenceProviderEvent::progress(token, progress));
}

void emitProviderEndOfSequence(
    ImageSequenceProviderSession* session, ImageSequenceProviderRequestToken token)
{
    Q_EMIT session->providerEvent(ImageSequenceProviderEvent::endOfSequence(token));
}

void emitProviderFailed(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    Q_UNUSED(diagnostic);
    Q_EMIT session->providerEvent(ImageSequenceProviderEvent::failed(
        token, ImageSequenceProviderFailure(ImageSequenceProviderFailureCause::ProviderInternal)));
}

void emitProviderUnsupported(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderUnsupportedCause cause,
    const QString& diagnostic)
{
    Q_UNUSED(diagnostic);
    Q_EMIT session->providerEvent(ImageSequenceProviderEvent::unsupported(token, cause));
}

void emitProviderCancelled(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    Q_UNUSED(diagnostic);
    Q_EMIT session->providerEvent(ImageSequenceProviderEvent::cancelled(token));
}

}
