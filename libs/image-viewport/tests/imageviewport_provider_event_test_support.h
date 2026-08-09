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
    (void)session->submitEvent(
        ImageSequenceProviderEvent::metadataReady(token, std::move(metadata)));
}

void emitProviderFrameHandleReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* handle,
    ImageSequenceProviderFrameEnvelope envelope)
{
    std::unique_ptr<ImageSequenceProviderFrameHandle> owner(handle);
    const ImageSequenceProviderEvent event
        = ImageSequenceProviderEvent::frameReady(token, handle, envelope);
    if (session->submitEvent(event) == ImageSequenceProviderEventSubmissionOutcome::Accepted) {
        [[maybe_unused]] auto* const transferredHandle = owner.release();
    }
}

void emitProviderProvisionalFrameHandleReady(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* handle,
    ImageSequenceProviderFrameEnvelope envelope)
{
    std::unique_ptr<ImageSequenceProviderFrameHandle> owner(handle);
    const ImageSequenceProviderEvent event
        = ImageSequenceProviderEvent::provisionalFrameReady(token, handle, envelope);
    if (session->submitEvent(event) == ImageSequenceProviderEventSubmissionOutcome::Accepted) {
        [[maybe_unused]] auto* const transferredHandle = owner.release();
    }
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
    (void)session->submitEvent(ImageSequenceProviderEvent::waiting(token));
}

void emitProviderProgress(
    ImageSequenceProviderSession* session, ImageSequenceProviderRequestToken token, double progress)
{
    (void)session->submitEvent(ImageSequenceProviderEvent::progress(token, progress));
}

void emitProviderEndOfSequence(
    ImageSequenceProviderSession* session, ImageSequenceProviderRequestToken token)
{
    (void)session->submitEvent(ImageSequenceProviderEvent::endOfSequence(token));
}

void emitProviderFailed(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    Q_UNUSED(diagnostic);
    (void)session->submitEvent(ImageSequenceProviderEvent::failed(
        token, ImageSequenceProviderFailure(ImageSequenceProviderFailureCause::ProviderInternal)));
}

void emitProviderUnsupported(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderUnsupportedCause cause,
    const QString& diagnostic)
{
    Q_UNUSED(diagnostic);
    (void)session->submitEvent(ImageSequenceProviderEvent::unsupported(token, cause));
}

void emitProviderCancelled(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    Q_UNUSED(diagnostic);
    (void)session->submitEvent(ImageSequenceProviderEvent::cancelled(token));
}

}
