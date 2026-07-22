// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "activenavigationthumbnailruntime.h"

#include <utility>

namespace kiriview {
ActiveNavigationThumbnailRuntime::ActiveNavigationThumbnailRuntime(
    QObject* owner, ActiveNavigationThumbnailRuntimeDependencies dependencies)
    : ActiveNavigationThumbnailRuntime(owner, std::move(dependencies.lookupProvider),
          std::move(dependencies.imageStore), std::move(dependencies.generationProvider),
          std::move(dependencies.sourceAdapter), std::move(dependencies.workerScheduler),
          std::move(dependencies.failureDiagnosticCallback))
{
}

ActiveNavigationThumbnailRuntime::ActiveNavigationThumbnailRuntime(QObject* owner,
    ThumbnailCacheLookupProvider lookupProvider, std::shared_ptr<ThumbnailImageStore> imageStore,
    ThumbnailGenerationProvider generationProvider, ThumbnailSourceAdapter sourceAdapter,
    ImageWorkerScheduler workerScheduler,
    ActiveNavigationThumbnailFailureDiagnosticCallback failureDiagnosticCallback)
    : m_rowStore(std::make_unique<ActiveNavigationThumbnailRowStore>(std::move(imageStore)))
    , m_workCoordinator(
          std::make_unique<ActiveNavigationThumbnailWorkCoordinator>(owner, *m_rowStore,
              lookupProvider ? std::move(lookupProvider)
                             : defaultThumbnailCacheLookupProvider(workerScheduler),
              generationProvider ? std::move(generationProvider)
                                 : defaultThumbnailGenerationProvider(workerScheduler),
              sourceAdapter ? std::move(sourceAdapter) : defaultThumbnailSourceAdapter(),
              std::move(failureDiagnosticCallback)))
{
}

ActiveNavigationThumbnailRuntime::~ActiveNavigationThumbnailRuntime()
{
    m_workCoordinator->invalidateRows();
}

QAbstractListModel* ActiveNavigationThumbnailRuntime::model() const { return m_rowStore->model(); }

quint64 ActiveNavigationThumbnailRuntime::navigationGeneration() const
{
    return m_rowStore->navigationGeneration();
}

void ActiveNavigationThumbnailRuntime::setRows(std::vector<ActiveNavigationThumbnailRow> rows)
{
    ActiveNavigationThumbnailRowUpdatePlan plan = m_rowStore->prepareRows(std::move(rows));
    const auto kind = plan.kind();
    if (kind == ActiveNavigationThumbnailRowUpdateKind::IdentityReplacement) {
        m_workCoordinator->invalidateRows();
    }
    ActiveNavigationThumbnailRowCommit commit = m_rowStore->commitRows(std::move(plan));
    if (kind == ActiveNavigationThumbnailRowUpdateKind::IdentityReplacement) {
        Q_ASSERT(commit.schedulingSnapshot.has_value());
        const bool reset = m_workCoordinator->resetRows(std::move(*commit.schedulingSnapshot));
        Q_ASSERT(reset);
        Q_UNUSED(reset);
    } else if (kind == ActiveNavigationThumbnailRowUpdateKind::SourceRefresh) {
        Q_ASSERT(commit.schedulingSnapshot.has_value());
        const bool refreshed
            = m_workCoordinator->refreshRows(std::move(*commit.schedulingSnapshot));
        Q_ASSERT(refreshed);
        Q_UNUSED(refreshed);
    }
    m_workCoordinator->setCurrentNumber(commit.currentNumber);
}

void ActiveNavigationThumbnailRuntime::setCurrentNumber(int currentNumber)
{
    m_rowStore->setCurrentNumber(currentNumber);
    m_workCoordinator->setCurrentNumber(currentNumber);
}

bool ActiveNavigationThumbnailRuntime::replaceDemandSnapshot(
    ActiveNavigationThumbnailDemandSnapshot snapshot)
{
    return m_workCoordinator->replaceDemandSnapshot(std::move(snapshot));
}

}
