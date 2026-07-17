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

const TargetSpreadRoleTerminalState* projectedTerminal(const RequestState& request)
{
    const auto* primary = currentGenerationTerminal(request, ImageViewportPageRole::Primary);
    if (!primary) {
        primary = currentDisplayRequestTerminal(request, ImageViewportPageRole::Primary);
    }
    const auto* secondary = currentGenerationTerminal(request, ImageViewportPageRole::Secondary);
    if (!secondary) {
        secondary = currentDisplayRequestTerminal(request, ImageViewportPageRole::Secondary);
    }
    if (!primary) {
        return secondary;
    }
    if (!secondary || primary->status == secondary->status
        || primary->status == ImageViewportRequestStatus::Error) {
        return primary;
    }
    return secondary->status == ImageViewportRequestStatus::Error ? secondary : primary;
}

ViewportChangeSet projectTerminal(
    ViewportEngineTargetSpreadTerminalInput input, RequestState& request)
{
    const auto* projected = projectedTerminal(request);
    if (!projected) {
        return input.changes;
    }
    const bool diagnosticChanged = request.errorString != projected->diagnostic;
    request.status = projected->status;
    request.reason = projected->reason;
    request.errorString = projected->diagnostic;
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
    return projectTerminal(std::move(input), request);
}

bool viewportEngineHasCurrentDisplayRequestTerminal(
    const ImageViewportInternal::RequestState& request)
{
    return currentDisplayRequestTerminal(request, ImageViewportPageRole::Primary)
        || currentDisplayRequestTerminal(request, ImageViewportPageRole::Secondary);
}

bool viewportEngineHasCurrentGenerationTerminal(const ImageViewportInternal::RequestState& request)
{
    return currentGenerationTerminal(request, ImageViewportPageRole::Primary)
        || currentGenerationTerminal(request, ImageViewportPageRole::Secondary);
}

bool viewportEngineHasCurrentTerminal(const ImageViewportInternal::RequestState& request)
{
    return projectedTerminal(request);
}
