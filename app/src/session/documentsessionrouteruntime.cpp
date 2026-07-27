// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionrouteruntime.h"

#include <type_traits>
#include <utility>
#include <variant>

namespace kiriview {
DocumentSessionRouteRuntime::DocumentSessionRouteRuntime(DocumentSessionRouteRuntimePorts ports)
    : m_ports(std::move(ports))
{
}

DocumentSessionRouteRuntime::~DocumentSessionRouteRuntime() { m_callbackLifetime.reset(); }

bool DocumentSessionRouteRuntime::executeWithSourceResolver(const DocumentSessionRoutePlan& plan,
    const DocumentSessionRouteSourceResolver& resolveSource,
    const DocumentSessionRouteExecutionControl& control)
{
    struct RouteExecutionResult
    {
        bool directMediaScopeChanged = false;
        bool directMediaNavigationCleared = false;
        bool publishPublicProjection = false;
        bool refreshDirectMediaNavigation = false;
        bool clearDirectMediaNavigationBeforePredecode = false;
        bool syncMediaPredecodeScope = false;
    };

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const DocumentSessionRouteRuntimePorts ports = m_ports;
    const DocumentSessionRouteSourceResolver sourceResolver = resolveSource;
    // The callbacks must remain valid if route execution destroys their original owner.
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    const DocumentSessionRouteExecutionControl executionControl = control;
    ImageAsyncTicket* const admission = &m_admission;
    ImageAsyncOperationState* const execution = &m_execution;
    const auto externallyCurrent = executionControl.isCurrent;
    const auto beforePublicProjection = executionControl.beforePublicProjection;
    const auto executeWithRoutingSuppressed = ports.session.executeWithRoutingSuppressed;

    const quint64 admissionRevision = admission->current();
    if (externallyCurrent && !externallyCurrent()) {
        return false;
    }
    if (lifetime.expired() || admission->current() != admissionRevision) {
        return false;
    }

    const quint64 admissionId = admission->next();
    const quint64 operationId = execution->start();
    const auto internallyCurrent = [lifetime, admission, execution, admissionId, operationId]() {
        return !lifetime.expired() && admission->accepts(admissionId)
            && execution->accepts(operationId);
    };
    const auto isCurrent = [internallyCurrent, externallyCurrent]() {
        if (!internallyCurrent()) {
            return false;
        }
        if (externallyCurrent && !externallyCurrent()) {
            return false;
        }
        return internallyCurrent();
    };
    const auto abortExecution = [lifetime, execution, operationId]() {
        if (!lifetime.expired()) {
            static_cast<void>(execution->finish(operationId));
        }
        return false;
    };
    const auto executeSuppressed
        = [&executeWithRoutingSuppressed](const std::function<void()>& mutation) {
              if (executeWithRoutingSuppressed) {
                  executeWithRoutingSuppressed(mutation);
                  return;
              }
              mutation();
          };

    ResolvedNavigationSource routeSource;
    if (!plan.sourceUrl.isEmpty()) {
        if (!sourceResolver) {
            return abortExecution();
        }
        routeSource = sourceResolver(plan.sourceUrl);
    }
    if (!isCurrent()) {
        return abortExecution();
    }

    RouteExecutionResult result;
    if (ports.session.cancelMediaOpenWith) {
        ports.session.cancelMediaOpenWith();
    }
    if (!isCurrent()) {
        return abortExecution();
    }

    result.publishPublicProjection = plan.publishPublicProjection;

    for (const DocumentSessionRouteMutation& mutation : plan.mutations) {
        if (!isCurrent()) {
            return abortExecution();
        }
        bool mutationCurrent = true;
        const auto executeCurrentSuppressed = [&executeSuppressed, &isCurrent, &mutationCurrent](
                                                  const std::function<void()>& callback) {
            executeSuppressed([&isCurrent, &mutationCurrent, &callback]() {
                if (!isCurrent()) {
                    mutationCurrent = false;
                    return;
                }
                callback();
                mutationCurrent = isCurrent();
            });
            mutationCurrent = mutationCurrent && isCurrent();
        };
        std::visit(
            [&ports, &result, &routeSource, &isCurrent, &mutationCurrent,
                &executeCurrentSuppressed](const auto& payload) {
                using Operation = std::decay_t<decltype(payload)>;

                if constexpr (std::is_same_v<Operation, ClearSessionErrorStringRouteOperation>) {
                    if (ports.session.clearSessionErrorString) {
                        ports.session.clearSessionErrorString();
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         CancelDirectMediaNavigationRouteOperation>) {
                    if (ports.directMedia.cancelDirectMediaNavigation) {
                        ports.directMedia.cancelDirectMediaNavigation();
                    }
                } else if constexpr (std::is_same_v<Operation, CancelMediaDeletionRouteOperation>) {
                    if (ports.directMedia.cancelMediaDeletion) {
                        ports.directMedia.cancelMediaDeletion();
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         ClearDirectMediaNavigationRouteOperation>) {
                    if (ports.directMedia.clearDirectMediaNavigation) {
                        ports.directMedia.clearDirectMediaNavigation();
                    }
                    result.directMediaNavigationCleared = true;
                } else if constexpr (std::is_same_v<Operation,
                                         ClearDirectMediaCursorRouteOperation>) {
                    result.directMediaScopeChanged
                        = (ports.directMedia.clearDirectMediaCursor
                              && ports.directMedia.clearDirectMediaCursor())
                        || result.directMediaScopeChanged;
                } else if constexpr (std::is_same_v<Operation,
                                         SetDirectVideoCursorRouteOperation>) {
                    result.directMediaScopeChanged
                        = (ports.directMedia.setDirectVideoCursor
                              && ports.directMedia.setDirectVideoCursor(routeSource))
                        || result.directMediaScopeChanged;
                } else if constexpr (std::is_same_v<Operation,
                                         RequestDirectImageCursorRouteOperation>) {
                    result.directMediaScopeChanged
                        = (ports.directMedia.requestDirectImageCursor
                              && ports.directMedia.requestDirectImageCursor(routeSource))
                        || result.directMediaScopeChanged;
                } else if constexpr (std::is_same_v<Operation,
                                         ClearThenRequestDirectImageCursorRouteOperation>) {
                    result.directMediaScopeChanged
                        = (ports.directMedia.clearDirectMediaCursor
                              && ports.directMedia.clearDirectMediaCursor())
                        || result.directMediaScopeChanged;
                    if (!isCurrent()) {
                        mutationCurrent = false;
                        return;
                    }
                    result.directMediaScopeChanged
                        = (ports.directMedia.requestDirectImageCursor
                              && ports.directMedia.requestDirectImageCursor(routeSource))
                        || result.directMediaScopeChanged;
                } else if constexpr (std::is_same_v<Operation, ClearImageDocumentRouteOperation>) {
                    executeCurrentSuppressed([&ports]() {
                        if (ports.documents.clearImageDocument) {
                            ports.documents.clearImageDocument();
                        }
                    });
                } else if constexpr (std::is_same_v<Operation, LeaveVideoModeRouteOperation>) {
                    executeCurrentSuppressed([&ports]() {
                        if (ports.documents.leaveVideoMode) {
                            ports.documents.leaveVideoMode();
                        }
                    });
                } else if constexpr (std::is_same_v<Operation, EnterEmptyDocumentRouteOperation>) {
                    executeCurrentSuppressed([&ports]() {
                        if (ports.documents.enterEmptyDocument) {
                            ports.documents.enterEmptyDocument();
                        }
                    });
                } else if constexpr (std::is_same_v<Operation, EnterImageDocumentRouteOperation>) {
                    executeCurrentSuppressed([&ports, &routeSource]() {
                        if (ports.documents.enterImageDocument) {
                            ports.documents.enterImageDocument(routeSource);
                        }
                    });
                } else if constexpr (std::is_same_v<Operation,
                                         EnterImageDocumentSameScopeNavigationRouteOperation>) {
                    executeCurrentSuppressed([&ports, &routeSource]() {
                        if (ports.documents.enterImageDocumentSameScopeNavigation) {
                            ports.documents.enterImageDocumentSameScopeNavigation(routeSource);
                        }
                    });
                } else if constexpr (std::is_same_v<Operation, EnterVideoDocumentRouteOperation>) {
                    executeCurrentSuppressed([&ports, &routeSource]() {
                        if (ports.documents.enterVideoDocument) {
                            ports.documents.enterVideoDocument(routeSource);
                        }
                    });
                } else if constexpr (std::is_same_v<Operation,
                                         SyncDirectImageCursorFromDocumentRouteOperation>) {
                    result.directMediaScopeChanged
                        = (ports.directMedia.syncDirectImageCursorFromDocument
                              && ports.directMedia.syncDirectImageCursorFromDocument())
                        || result.directMediaScopeChanged;
                } else if constexpr (std::is_same_v<Operation, ClearSourceIdentityRouteOperation>) {
                    if (ports.sourceIdentity.clearSourceIdentity) {
                        ports.sourceIdentity.clearSourceIdentity();
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         UseOriginalSourceIdentityRouteOperation>) {
                    if (ports.sourceIdentity.useOriginalSourceIdentity) {
                        ports.sourceIdentity.useOriginalSourceIdentity(routeSource.requestedUrl());
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         UseImageDocumentSourceIdentityRouteOperation>) {
                    if (ports.sourceIdentity.useImageDocumentSourceIdentity) {
                        ports.sourceIdentity.useImageDocumentSourceIdentity();
                    }
                }
            },
            mutation);
        if (!mutationCurrent || !isCurrent()) {
            return abortExecution();
        }
    }

    for (const DocumentSessionRouteFollowUpEffect& effect : plan.followUpEffects) {
        if (!isCurrent()) {
            return abortExecution();
        }
        std::visit(
            [&ports, &result, &isCurrent](const auto& payload) {
                using Effect = std::decay_t<decltype(payload)>;

                if constexpr (std::is_same_v<Effect,
                                  RefreshDirectMediaNavigationAfterRoutingRouteEffect>) {
                    const bool directMediaNavigationActive
                        = ports.directMedia.directMediaNavigationActive
                        && ports.directMedia.directMediaNavigationActive();
                    if (!isCurrent()) {
                        return;
                    }
                    if (result.directMediaScopeChanged || result.directMediaNavigationCleared
                        || directMediaNavigationActive) {
                        result.refreshDirectMediaNavigation = true;
                    }
                } else if constexpr (std::is_same_v<Effect, ClearMediaPredecodeRouteEffect>) {
                    if (result.directMediaNavigationCleared) {
                        result.clearDirectMediaNavigationBeforePredecode = true;
                        result.publishPublicProjection = true;
                    }
                    result.syncMediaPredecodeScope = true;
                }
            },
            effect);
        if (!isCurrent()) {
            return abortExecution();
        }
    }

    result.syncMediaPredecodeScope
        = result.syncMediaPredecodeScope || result.directMediaScopeChanged;
    if (result.clearDirectMediaNavigationBeforePredecode
        && ports.directMedia.clearDirectMediaNavigation) {
        ports.directMedia.clearDirectMediaNavigation();
    }
    if (!isCurrent()) {
        return abortExecution();
    }
    if (result.publishPublicProjection) {
        if (beforePublicProjection) {
            beforePublicProjection();
        }
        if (!isCurrent()) {
            return abortExecution();
        }
        if (ports.followUp.recomputePublicProjection) {
            ports.followUp.recomputePublicProjection();
        }
    }
    if (!isCurrent()) {
        return abortExecution();
    }
    if (result.syncMediaPredecodeScope && ports.followUp.syncMediaPredecodeScope) {
        ports.followUp.syncMediaPredecodeScope();
    }
    if (!isCurrent()) {
        return abortExecution();
    }
    const bool delegatesNavigationRefresh
        = result.refreshDirectMediaNavigation && ports.directMedia.refreshDirectMediaNavigation;
    if (delegatesNavigationRefresh) {
        ports.directMedia.refreshDirectMediaNavigation();
    }
    const auto continuationCurrent
        = [&]() { return delegatesNavigationRefresh ? internallyCurrent() : isCurrent(); };
    if (!continuationCurrent()) {
        return abortExecution();
    }

    if (ports.session.routeCompleted) {
        ports.session.routeCompleted();
    }
    if (!continuationCurrent()) {
        return abortExecution();
    }

    return execution->finish(operationId);
}
}
