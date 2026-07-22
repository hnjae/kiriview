// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportenginetargetspreadterminaloperations_p.h"

namespace {
using namespace ImageViewportInternal;

template <typename TerminalState>
TargetSpreadRoleTerminalState& terminalForRole(TerminalState& terminal, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Secondary ? terminal.secondary : terminal.primary;
}

bool roleRequired(const RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Primary ? request.roles[0].source.facts.present
                                                  : request.roles[1].sequence;
}

const TargetSpreadRoleTerminalState* currentGenerationTerminal(
    const RequestState& request, ImageViewportPageRole role)
{
    const auto& terminal = request.generationTerminal;
    if (!terminal.sealed || terminal.generation != request.sequenceGeneration
        || !roleRequired(request, role)) {
        return nullptr;
    }
    const auto& roleTerminal
        = role == ImageViewportPageRole::Secondary ? terminal.secondary : terminal.primary;
    return roleTerminal.terminal ? &roleTerminal : nullptr;
}

const TargetSpreadRoleTerminalState* currentDisplayRequestTerminal(
    const RequestState& request, ImageViewportPageRole role)
{
    const auto& terminal = request.targetSpreadTerminal;
    if (!terminal.sealed || terminal.generation != request.sequenceGeneration
        || terminal.requestId != request.roles[0].activeRequest.identity.id
        || !roleRequired(request, role)) {
        return nullptr;
    }
    const auto& roleTerminal
        = role == ImageViewportPageRole::Secondary ? terminal.secondary : terminal.primary;
    return roleTerminal.terminal ? &roleTerminal : nullptr;
}

ViewportEngineProjectedTerminal projectedTerminal(const RequestState& request)
{
    const auto* primary = currentGenerationTerminal(request, ImageViewportPageRole::Primary);
    ImageViewportFailureScope primaryScope = ImageViewportFailureScope::Generation;
    if (!primary) {
        primary = currentDisplayRequestTerminal(request, ImageViewportPageRole::Primary);
        primaryScope = ImageViewportFailureScope::DisplayRequest;
    }
    const auto* secondary = currentGenerationTerminal(request, ImageViewportPageRole::Secondary);
    ImageViewportFailureScope secondaryScope = ImageViewportFailureScope::Generation;
    if (!secondary) {
        secondary = currentDisplayRequestTerminal(request, ImageViewportPageRole::Secondary);
        secondaryScope = ImageViewportFailureScope::DisplayRequest;
    }
    if (!primary) {
        return { secondary, ImageViewportPageRole::Secondary,
            secondary ? secondaryScope : ImageViewportFailureScope::Unavailable };
    }
    if (!secondary || primary->status == secondary->status
        || primary->status == ImageViewportRequestStatus::Error) {
        return { primary, ImageViewportPageRole::Primary, primaryScope };
    }
    return secondary->status == ImageViewportRequestStatus::Error
        ? ViewportEngineProjectedTerminal { secondary, ImageViewportPageRole::Secondary,
              secondaryScope }
        : ViewportEngineProjectedTerminal { primary, ImageViewportPageRole::Primary, primaryScope };
}

ViewportChangeSet projectTerminal(
    ViewportEngineTargetSpreadTerminalInput input, RequestState& request)
{
    const auto projected = projectedTerminal(request);
    if (!projected.terminal) {
        return input.changes;
    }
    const bool diagnosticChanged = request.errorString != projected.terminal->diagnostic;
    request.status = projected.terminal->status;
    request.reason = projected.terminal->reason;
    request.errorString = projected.terminal->diagnostic;
    input.changes.requestState = true;
    input.changes.requestRevision = true;
    input.changes.diagnostics = diagnosticChanged;
    return input.changes;
}
}

ImageViewportInternal::ViewportChangeSet recordViewportEngineDisplayRequestTerminal(
    ViewportEngineTargetSpreadTerminalInput input, ImageViewportInternal::RequestState& request)
{
    auto& terminal = request.targetSpreadTerminal;
    if (terminal.generation != request.sequenceGeneration
        || terminal.requestId != request.roles[0].activeRequest.identity.id) {
        terminal.clear();
        terminal.generation = request.sequenceGeneration;
        terminal.requestId = request.roles[0].activeRequest.identity.id;
    }
    terminal.sealed = true;
    auto& roleTerminal = terminalForRole(terminal, input.role);
    roleTerminal.terminal = true;
    roleTerminal.status = input.status;
    roleTerminal.reason = input.reason;
    roleTerminal.diagnostic = input.diagnostic;
    roleTerminal.providerFailureAvailable = input.providerFailureAvailable;
    roleTerminal.providerCause = input.providerCause;
    roleTerminal.providerReference = input.providerReference;
    roleTerminal.providerFailureLeaseId = input.providerFailureLeaseId;
    return projectTerminal(std::move(input), request);
}

ImageViewportInternal::ViewportChangeSet recordViewportEngineGenerationTerminal(
    ViewportEngineTargetSpreadTerminalInput input, ImageViewportInternal::RequestState& request)
{
    auto& terminal = request.generationTerminal;
    if (terminal.generation != request.sequenceGeneration) {
        terminal.clear();
        terminal.generation = request.sequenceGeneration;
    }
    terminal.sealed = true;
    auto& roleTerminal = terminalForRole(terminal, input.role);
    roleTerminal.terminal = true;
    roleTerminal.status = input.status;
    roleTerminal.reason = input.reason;
    roleTerminal.diagnostic = input.diagnostic;
    roleTerminal.providerFailureAvailable = input.providerFailureAvailable;
    roleTerminal.providerCause = input.providerCause;
    roleTerminal.providerReference = input.providerReference;
    roleTerminal.providerFailureLeaseId = input.providerFailureLeaseId;
    return projectTerminal(std::move(input), request);
}

bool viewportEngineHasCurrentGenerationTerminal(const ImageViewportInternal::RequestState& request)
{
    return currentGenerationTerminal(request, ImageViewportPageRole::Primary)
        || currentGenerationTerminal(request, ImageViewportPageRole::Secondary);
}

bool viewportEngineHasCurrentTerminal(const ImageViewportInternal::RequestState& request)
{
    return projectedTerminal(request).terminal;
}

bool viewportEngineRoleCanRefineCurrentTerminal(
    const ImageViewportInternal::RequestState& request, ImageViewportPageRole role)
{
    if (!roleRequired(request, role)) {
        return false;
    }
    const auto* roleTerminal = currentGenerationTerminal(request, role);
    if (!roleTerminal) {
        roleTerminal = currentDisplayRequestTerminal(request, role);
    }
    if (roleTerminal) {
        return false;
    }
    const auto projected = projectedTerminal(request);
    if (!projected.terminal
        || projected.terminal->status == ImageViewportRequestStatus::Unsupported) {
        return true;
    }
    const auto* secondary = currentGenerationTerminal(request, ImageViewportPageRole::Secondary);
    if (!secondary) {
        secondary = currentDisplayRequestTerminal(request, ImageViewportPageRole::Secondary);
    }
    return role == ImageViewportPageRole::Primary && secondary
        && secondary->status == ImageViewportRequestStatus::Error;
}

ViewportEngineProjectedTerminal projectViewportEngineTerminal(
    const ImageViewportInternal::RequestState& request)
{
    return projectedTerminal(request);
}

ImageViewportInternal::ViewportChangeSet projectViewportEngineCurrentTerminal(
    ImageViewportInternal::ViewportChangeSet changes, ImageViewportInternal::RequestState& request)
{
    return projectTerminal({ {}, {}, {}, {}, changes }, request);
}
