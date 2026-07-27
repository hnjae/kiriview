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

    const quint64 admissionRevision = m_admission.current();
    if (control.isCurrent && !control.isCurrent()) {
        return false;
    }
    if (m_admission.current() != admissionRevision) {
        return false;
    }

    const quint64 admissionId = m_admission.next();
    const quint64 operationId = m_execution.start();
    const auto isCurrent = [this, admissionId, operationId, &control]() {
        if (!m_admission.accepts(admissionId) || !m_execution.accepts(operationId)) {
            return false;
        }
        const bool externallyCurrent = !control.isCurrent || control.isCurrent();
        return externallyCurrent && m_admission.accepts(admissionId)
            && m_execution.accepts(operationId);
    };
    const auto abortExecution = [this, operationId]() {
        static_cast<void>(m_execution.finish(operationId));
        return false;
    };

    ResolvedNavigationSource routeSource;
    if (!plan.sourceUrl.isEmpty()) {
        if (!resolveSource) {
            return abortExecution();
        }
        routeSource = resolveSource(plan.sourceUrl);
    }
    if (!isCurrent()) {
        return abortExecution();
    }

    RouteExecutionResult result;
    if (m_ports.session.cancelMediaOpenWith) {
        m_ports.session.cancelMediaOpenWith();
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
        const auto executeCurrentSuppressed
            = [this, &isCurrent, &mutationCurrent](const std::function<void()>& callback) {
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
            [this, &result, &routeSource, &isCurrent, &mutationCurrent, &executeCurrentSuppressed](
                const auto& payload) {
                using Operation = std::decay_t<decltype(payload)>;

                if constexpr (std::is_same_v<Operation, ClearSessionErrorStringRouteOperation>) {
                    if (m_ports.session.clearSessionErrorString) {
                        m_ports.session.clearSessionErrorString();
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         CancelDirectMediaNavigationRouteOperation>) {
                    if (m_ports.directMedia.cancelDirectMediaNavigation) {
                        m_ports.directMedia.cancelDirectMediaNavigation();
                    }
                } else if constexpr (std::is_same_v<Operation, CancelMediaDeletionRouteOperation>) {
                    if (m_ports.directMedia.cancelMediaDeletion) {
                        m_ports.directMedia.cancelMediaDeletion();
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         ClearDirectMediaNavigationRouteOperation>) {
                    if (m_ports.directMedia.clearDirectMediaNavigation) {
                        m_ports.directMedia.clearDirectMediaNavigation();
                    }
                    result.directMediaNavigationCleared = true;
                } else if constexpr (std::is_same_v<Operation,
                                         ClearDirectMediaCursorRouteOperation>) {
                    result.directMediaScopeChanged
                        = (m_ports.directMedia.clearDirectMediaCursor
                              && m_ports.directMedia.clearDirectMediaCursor())
                        || result.directMediaScopeChanged;
                } else if constexpr (std::is_same_v<Operation,
                                         SetDirectVideoCursorRouteOperation>) {
                    result.directMediaScopeChanged
                        = (m_ports.directMedia.setDirectVideoCursor
                              && m_ports.directMedia.setDirectVideoCursor(routeSource))
                        || result.directMediaScopeChanged;
                } else if constexpr (std::is_same_v<Operation,
                                         RequestDirectImageCursorRouteOperation>) {
                    result.directMediaScopeChanged
                        = (m_ports.directMedia.requestDirectImageCursor
                              && m_ports.directMedia.requestDirectImageCursor(routeSource))
                        || result.directMediaScopeChanged;
                } else if constexpr (std::is_same_v<Operation,
                                         ClearThenRequestDirectImageCursorRouteOperation>) {
                    result.directMediaScopeChanged
                        = (m_ports.directMedia.clearDirectMediaCursor
                              && m_ports.directMedia.clearDirectMediaCursor())
                        || result.directMediaScopeChanged;
                    if (!isCurrent()) {
                        mutationCurrent = false;
                        return;
                    }
                    result.directMediaScopeChanged
                        = (m_ports.directMedia.requestDirectImageCursor
                              && m_ports.directMedia.requestDirectImageCursor(routeSource))
                        || result.directMediaScopeChanged;
                } else if constexpr (std::is_same_v<Operation, ClearImageDocumentRouteOperation>) {
                    executeCurrentSuppressed([this]() {
                        if (m_ports.documents.clearImageDocument) {
                            m_ports.documents.clearImageDocument();
                        }
                    });
                } else if constexpr (std::is_same_v<Operation, LeaveVideoModeRouteOperation>) {
                    executeCurrentSuppressed([this]() {
                        if (m_ports.documents.leaveVideoMode) {
                            m_ports.documents.leaveVideoMode();
                        }
                    });
                } else if constexpr (std::is_same_v<Operation, EnterEmptyDocumentRouteOperation>) {
                    executeCurrentSuppressed([this]() {
                        if (m_ports.documents.enterEmptyDocument) {
                            m_ports.documents.enterEmptyDocument();
                        }
                    });
                } else if constexpr (std::is_same_v<Operation, EnterImageDocumentRouteOperation>) {
                    executeCurrentSuppressed([this, &routeSource]() {
                        if (m_ports.documents.enterImageDocument) {
                            m_ports.documents.enterImageDocument(routeSource);
                        }
                    });
                } else if constexpr (std::is_same_v<Operation,
                                         EnterImageDocumentSameScopeNavigationRouteOperation>) {
                    executeCurrentSuppressed([this, &routeSource]() {
                        if (m_ports.documents.enterImageDocumentSameScopeNavigation) {
                            m_ports.documents.enterImageDocumentSameScopeNavigation(routeSource);
                        }
                    });
                } else if constexpr (std::is_same_v<Operation, EnterVideoDocumentRouteOperation>) {
                    executeCurrentSuppressed([this, &routeSource]() {
                        if (m_ports.documents.enterVideoDocument) {
                            m_ports.documents.enterVideoDocument(routeSource);
                        }
                    });
                } else if constexpr (std::is_same_v<Operation,
                                         SyncDirectImageCursorFromDocumentRouteOperation>) {
                    result.directMediaScopeChanged
                        = (m_ports.directMedia.syncDirectImageCursorFromDocument
                              && m_ports.directMedia.syncDirectImageCursorFromDocument())
                        || result.directMediaScopeChanged;
                } else if constexpr (std::is_same_v<Operation, ClearSourceIdentityRouteOperation>) {
                    if (m_ports.sourceIdentity.clearSourceIdentity) {
                        m_ports.sourceIdentity.clearSourceIdentity();
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         UseOriginalSourceIdentityRouteOperation>) {
                    if (m_ports.sourceIdentity.useOriginalSourceIdentity) {
                        m_ports.sourceIdentity.useOriginalSourceIdentity(
                            routeSource.requestedUrl());
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         UseImageDocumentSourceIdentityRouteOperation>) {
                    if (m_ports.sourceIdentity.useImageDocumentSourceIdentity) {
                        m_ports.sourceIdentity.useImageDocumentSourceIdentity();
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
            [this, &result, &isCurrent](const auto& payload) {
                using Effect = std::decay_t<decltype(payload)>;

                if constexpr (std::is_same_v<Effect,
                                  RefreshDirectMediaNavigationAfterRoutingRouteEffect>) {
                    const bool directMediaNavigationActive
                        = m_ports.directMedia.directMediaNavigationActive
                        && m_ports.directMedia.directMediaNavigationActive();
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
        && m_ports.directMedia.clearDirectMediaNavigation) {
        m_ports.directMedia.clearDirectMediaNavigation();
    }
    if (!isCurrent()) {
        return abortExecution();
    }
    if (result.publishPublicProjection) {
        if (control.beforePublicProjection) {
            control.beforePublicProjection();
        }
        if (!isCurrent()) {
            return abortExecution();
        }
        if (m_ports.followUp.recomputePublicProjection) {
            m_ports.followUp.recomputePublicProjection();
        }
    }
    if (!isCurrent()) {
        return abortExecution();
    }
    if (result.syncMediaPredecodeScope && m_ports.followUp.syncMediaPredecodeScope) {
        m_ports.followUp.syncMediaPredecodeScope();
    }
    if (!isCurrent()) {
        return abortExecution();
    }
    if (result.refreshDirectMediaNavigation && m_ports.directMedia.refreshDirectMediaNavigation) {
        m_ports.directMedia.refreshDirectMediaNavigation();
    }
    if (!isCurrent()) {
        return abortExecution();
    }

    if (m_ports.session.routeCompleted) {
        m_ports.session.routeCompleted();
    }
    if (!isCurrent()) {
        return abortExecution();
    }

    return m_execution.finish(operationId);
}

void DocumentSessionRouteRuntime::executeSuppressed(const std::function<void()>& mutation)
{
    if (m_ports.session.executeWithRoutingSuppressed) {
        m_ports.session.executeWithRoutingSuppressed(mutation);
        return;
    }

    mutation();
}
}
