// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILRUNTIME_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILRUNTIME_H

#include "session/activenavigationthumbnailrowstore.h"
#include "session/activenavigationthumbnailworkcoordinator.h"

#include <QUrl>
#include <QtGlobal>
#include <cstddef>
#include <memory>
#include <vector>

class QAbstractListModel;
class QObject;

namespace kiriview {
struct ActiveNavigationThumbnailRuntimeDependencies
{
    ThumbnailCacheLookupProvider lookupProvider;
    std::shared_ptr<ThumbnailImageStore> imageStore;
    ThumbnailGenerationProvider generationProvider;
    ThumbnailSourceAdapter sourceAdapter;
    ImageWorkerScheduler workerScheduler;
    ActiveNavigationThumbnailFailureDiagnosticCallback failureDiagnosticCallback;
};

class ActiveNavigationThumbnailRuntime final
{
public:
    explicit ActiveNavigationThumbnailRuntime(
        QObject* owner = nullptr, ActiveNavigationThumbnailRuntimeDependencies dependencies = {});
    ActiveNavigationThumbnailRuntime(QObject* owner,
        ThumbnailCacheLookupProvider lookupProvider = {},
        std::shared_ptr<ThumbnailImageStore> imageStore = {},
        ThumbnailGenerationProvider generationProvider = {},
        ThumbnailSourceAdapter sourceAdapter = {}, const ImageWorkerScheduler& workerScheduler = {},
        ActiveNavigationThumbnailFailureDiagnosticCallback failureDiagnosticCallback = {});
    ~ActiveNavigationThumbnailRuntime();
    Q_DISABLE_COPY_MOVE(ActiveNavigationThumbnailRuntime)

    [[nodiscard]] QAbstractListModel* model() const;
    [[nodiscard]] quint64 navigationGeneration() const;

    void setRows(std::vector<ActiveNavigationThumbnailRow> rows);
    void setCurrentNumber(int currentNumber);
    bool replaceDemandSnapshot(const ActiveNavigationThumbnailDemandSnapshot& snapshot);

private:
    std::unique_ptr<ActiveNavigationThumbnailRowStore> m_rowStore;
    std::unique_ptr<ActiveNavigationThumbnailWorkCoordinator> m_workCoordinator;
};
}

#endif
