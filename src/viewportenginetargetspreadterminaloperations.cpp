#include "viewportenginetargetspreadterminaloperations_p.h"

namespace {
using namespace ImageViewportInternal;

TargetSpreadRoleTerminalState& terminalForRole(RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Secondary ? request.targetSpreadTerminal.secondary
                                                    : request.targetSpreadTerminal.primary;
}

bool roleRequired(const RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Primary ? request.roles[0].source.facts.present
                                                  : request.roles[1].sequence;
}

const TargetSpreadRoleTerminalState* currentTerminal(
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
    const auto* primary = currentTerminal(request, ImageViewportPageRole::Primary);
    const auto* secondary = currentTerminal(request, ImageViewportPageRole::Secondary);
    if (!primary) {
        return secondary;
    }
    if (!secondary || primary->status == secondary->status
        || primary->status == ImageViewportRequestStatus::Error) {
        return primary;
    }
    return secondary->status == ImageViewportRequestStatus::Error ? secondary : primary;
}
}

ImageViewportInternal::ViewportChangeSet recordViewportEngineTargetSpreadTerminal(
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
    auto& roleTerminal = terminalForRole(request, input.role);
    roleTerminal.terminal = true;
    roleTerminal.status = input.status;
    roleTerminal.reason = input.reason;
    roleTerminal.failureScope = input.scope;
    roleTerminal.diagnostic = input.diagnostic;

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
