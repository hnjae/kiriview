/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <ImageViewport/imagesequenceprovider.h>

struct ViewportProviderEvent
{
    ImageSequenceProviderEventKind kind = ImageSequenceProviderEventKind::Waiting;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    quint64 sessionSerial = 0;
    quint64 generation = 0;
    ImageSequenceProviderRequestToken token;
    ImageSequenceProviderMetadata metadata;
    ImageSequenceProviderFrameHandle* frameHandle = nullptr;
    quint64 frameLeaseId = 0;
    ImageSequenceProviderFrameEnvelope frameEnvelope;
    double progress = 0.0;
    ImageSequenceProviderUnsupportedCause unsupportedCause
        = ImageSequenceProviderUnsupportedCause::PayloadRejection;
};
