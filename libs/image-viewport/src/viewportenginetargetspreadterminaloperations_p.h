/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportenginestate_p.h"

struct ViewportEngineTargetSpreadTerminalInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageViewportRequestStatus status = ImageViewportRequestStatus::Error;
    ImageViewportRequestReason reason = ImageViewportRequestReason::ProviderFailure;
    ImageViewportInternal::PublicDiagnosticText diagnostic;
    ImageViewportInternal::ViewportChangeSet changes;
    bool providerFailureAvailable = false;
    ImageSequenceProviderFailureCause providerCause
        = ImageSequenceProviderFailureCause::Unavailable;
    ImageSequenceProviderFailureReference providerReference;
    quint64 providerFailureLeaseId = 0;
};

struct ViewportEngineProjectedTerminal
{
    const ImageViewportInternal::TargetSpreadRoleTerminalState* terminal = nullptr;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageViewportFailureScope scope = ImageViewportFailureScope::Unavailable;
};

ImageViewportInternal::ViewportChangeSet recordViewportEngineDisplayRequestTerminal(
    ViewportEngineTargetSpreadTerminalInput, ImageViewportInternal::RequestState&);
ImageViewportInternal::ViewportChangeSet recordViewportEngineGenerationTerminal(
    ViewportEngineTargetSpreadTerminalInput, ImageViewportInternal::RequestState&);

bool viewportEngineHasCurrentGenerationTerminal(const ImageViewportInternal::RequestState&);
bool viewportEngineHasCurrentTerminal(const ImageViewportInternal::RequestState&);
bool viewportEngineRoleCanRefineCurrentTerminal(
    const ImageViewportInternal::RequestState&, ImageViewportPageRole);
ImageViewportInternal::ViewportChangeSet projectViewportEngineCurrentTerminal(
    ImageViewportInternal::ViewportChangeSet, ImageViewportInternal::RequestState&);
ViewportEngineProjectedTerminal projectViewportEngineTerminal(
    const ImageViewportInternal::RequestState&);
