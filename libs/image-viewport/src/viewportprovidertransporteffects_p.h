/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportprovidercontract_p.h"

inline ImageSequenceProviderRequest providerRequestForTransport(
    ImageViewportPageRole role, const ViewportProviderFrameCommand& command)
{
    if (command.targetKind == ImageViewportInternal::ProviderRequestTargetKind::Playback) {
        return ImageSequenceProviderRequest::playback(
            command.token, role, command.frame, command.position, command.demand);
    }
    if (command.targetKind == ImageViewportInternal::ProviderRequestTargetKind::Position) {
        return ImageSequenceProviderRequest::position(
            command.token, role, command.position, command.frame, command.demand);
    }
    return ImageSequenceProviderRequest::frame(command.token, role, command.frame, command.demand);
}

inline void appendProviderTransport(ViewportProviderTransportBatch& batch,
    const ViewportProviderMetadataTransportEffect& effect, ImageViewportPageRole role)
{
    if (effect.closeSession) {
        batch.append({ ViewportProviderTransportCommand::Kind::CloseSession, role, {},
            effect.sessionClose });
    }
    if (effect.sendCommand) {
        batch.append({ ViewportProviderTransportCommand::Kind::SendRequest, role,
            ImageSequenceProviderRequest::metadata(effect.token) });
    }
}

inline void appendProviderTransport(ViewportProviderTransportBatch& batch,
    const ViewportProviderFrameTransportEffect& effect, ImageViewportPageRole role)
{
    if (effect.cancelToken.isValid()) {
        batch.append({ ViewportProviderTransportCommand::Kind::SendRequest, role,
            ImageSequenceProviderRequest::cancel({ effect.cancelToken }), {},
            ViewportProviderDeferredEngineEvent::None, false });
    }
    if (effect.deferredEngineEvent != ViewportProviderDeferredEngineEvent::None) {
        batch.append({ ViewportProviderTransportCommand::Kind::ScheduleDeferredEvent, role, {}, {},
            effect.deferredEngineEvent });
    }
    if (effect.closeSession) {
        batch.append({ ViewportProviderTransportCommand::Kind::CloseSession, role, {},
            effect.sessionClose });
    }
    if (effect.sendCommand) {
        batch.append({ ViewportProviderTransportCommand::Kind::SendRequest, role,
            providerRequestForTransport(role, effect.command) });
    }
}
