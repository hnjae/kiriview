// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_APPLICATIONACTIONSOURCEATTACHMENT_H
#define KIRIVIEW_APPLICATIONACTIONSOURCEATTACHMENT_H

#include "applicationactionruntime.h"
#include "session/documentsessiondocumentports.h"

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QtGlobal>
#include <vector>

namespace kiriview::ApplicationActions {
class ApplicationActionSourceAttachment final
{
public:
    ApplicationActionSourceAttachment(ApplicationActionRuntime& runtime, QObject& context);
    ~ApplicationActionSourceAttachment();

    void setDocumentSessionSnapshotPort(DocumentSessionActionStateSnapshotPort source);
    void setUiGateSnapshot(ApplicationActionUiGateSnapshot snapshot);

private:
    void connectSource();
    void disconnectSource();
    void refresh();

    ApplicationActionRuntime& m_runtime;
    QPointer<QObject> m_context;
    DocumentSessionActionStateSnapshotPort m_source;
    ApplicationActionUiGateSnapshot m_uiGateSnapshot;
    quint64 m_uiGateRevision = 0;
    std::vector<QMetaObject::Connection> m_connections;
};
}

#endif
