// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "applicationactionsourceattachment.h"

#include <utility>

namespace kiriview::ApplicationActions {
ApplicationActionSourceAttachment::ApplicationActionSourceAttachment(
    ApplicationActionRuntime& runtime, QObject& context)
    : m_runtime(runtime)
    , m_context(&context)
    , m_callbackContext(std::make_unique<QObject>())
{
}

ApplicationActionSourceAttachment::~ApplicationActionSourceAttachment()
{
    disconnectSource();
    m_callbackContext.reset();
}

void ApplicationActionSourceAttachment::setDocumentSessionSnapshotPort(
    DocumentSessionActionStateSnapshotPort source)
{
    ++m_sourceGeneration;
    disconnectSource();
    m_runtime.resetImageToolbarPresentationHistory();
    m_source = std::move(source);
    connectSource();
    refresh();
}

void ApplicationActionSourceAttachment::setUiGateSnapshot(ApplicationActionUiGateSnapshot snapshot)
{
    ++m_uiGateRevision;
    m_uiGateSnapshot = snapshot;
    refresh();
}

void ApplicationActionSourceAttachment::refresh()
{
    ApplicationActionStateSnapshot snapshot;
    snapshot.uiGateRevision = m_uiGateRevision;
    snapshot.uiGates = m_uiGateSnapshot;
    if (m_source.snapshot) {
        snapshot.documentSession = m_source.snapshot();
    }
    m_runtime.setActionStateSnapshot(snapshot);
}

void ApplicationActionSourceAttachment::connectSource()
{
    if (!m_source.snapshotChanged || m_context == nullptr || m_callbackContext == nullptr) {
        return;
    }

    const quint64 sourceGeneration = m_sourceGeneration;
    m_connections = m_source.snapshotChanged(m_callbackContext.get(), [this, sourceGeneration]() {
        if (m_context != nullptr && sourceGeneration == m_sourceGeneration) {
            refresh();
        }
    });
}

void ApplicationActionSourceAttachment::disconnectSource()
{
    for (const QMetaObject::Connection& connection : m_connections) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
}
}
