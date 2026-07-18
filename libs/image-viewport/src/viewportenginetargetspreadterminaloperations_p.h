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
};

ImageViewportInternal::ViewportChangeSet recordViewportEngineDisplayRequestTerminal(
    ViewportEngineTargetSpreadTerminalInput, ImageViewportInternal::RequestState&);
ImageViewportInternal::ViewportChangeSet recordViewportEngineGenerationTerminal(
    ViewportEngineTargetSpreadTerminalInput, ImageViewportInternal::RequestState&);

bool viewportEngineHasCurrentDisplayRequestTerminal(const ImageViewportInternal::RequestState&);
bool viewportEngineHasCurrentGenerationTerminal(const ImageViewportInternal::RequestState&);
bool viewportEngineHasCurrentTerminal(const ImageViewportInternal::RequestState&);
bool viewportEngineRoleCanRefineCurrentTerminal(
    const ImageViewportInternal::RequestState&, ImageViewportPageRole);
ImageViewportInternal::ViewportChangeSet projectViewportEngineCurrentTerminal(
    ImageViewportInternal::ViewportChangeSet, ImageViewportInternal::RequestState&);
