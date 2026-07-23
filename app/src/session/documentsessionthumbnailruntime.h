// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONTHUMBNAILRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONTHUMBNAILRUNTIME_H

#include "session/activenavigationthumbnaildemand.h"
#include "session/activenavigationthumbnailprojection.h"
#include "session/activenavigationthumbnailruntime.h"
#include "session/documentsessiondocumentports.h"

#include <QAbstractListModel>
#include <QUrl>
#include <QtGlobal>
#include <vector>

class QObject;

namespace kiriview {
class DocumentSessionThumbnailRuntime final
{
public:
    DocumentSessionThumbnailRuntime(QObject* owner,
        DocumentSessionImageDocumentSnapshotPort* imageDocument,
        ActiveNavigationThumbnailRuntimeDependencies dependencies = {});

    [[nodiscard]] QAbstractListModel* model() const;
    [[nodiscard]] quint64 navigationGeneration() const;
    void setRows(std::vector<ActiveNavigationThumbnailRow> rows);
    void setCurrentNumber(int currentNumber);
    bool replaceDemandSnapshot(ActiveNavigationThumbnailDemandSnapshot snapshot);

private:
    ActiveNavigationThumbnailRuntime m_runtime;
};
}

#endif
