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
    Q_UNUSED(diagnostic);
    emit session->providerEvent(ImageSequenceProviderEvent::failed(token));
}

void emitProviderUnsupported(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderUnsupportedCause cause,
    QString diagnostic)
{
    Q_UNUSED(diagnostic);
    emit session->providerEvent(ImageSequenceProviderEvent::unsupported(token, cause));
}

void emitProviderCancelled(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken token, QString diagnostic)
{
    Q_UNUSED(diagnostic);
    emit session->providerEvent(ImageSequenceProviderEvent::cancelled(token));
}

}
