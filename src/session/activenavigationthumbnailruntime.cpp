// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "activenavigationthumbnailruntime.h"

#include <utility>

namespace kiriview {
ActiveNavigationThumbnailRuntime::ActiveNavigationThumbnailRuntime(
    QObject* owner, ActiveNavigationThumbnailRuntimeDependencies dependencies)
    : ActiveNavigationThumbnailRuntime(owner, std::move(dependencies.lookupProvider),
          std::move(dependencies.imageStore), std::move(dependencies.generationProvider),
          std::move(dependencies.sourceAdapter), std::move(dependencies.workerScheduler))
{
}

ActiveNavigationThumbnailRuntime::ActiveNavigationThumbnailRuntime(QObject* owner,
    ThumbnailCacheLookupProvider lookupProvider, std::shared_ptr<ThumbnailImageStore> imageStore,
    ThumbnailGenerationProvider generationProvider, ThumbnailSourceAdapter sourceAdapter,
    ImageWorkerScheduler workerScheduler)
    : m_rowStore(std::make_unique<ActiveNavigationThumbnailRowStore>(owner, std::move(imageStore)))
    , m_workCoordinator(
          std::make_unique<ActiveNavigationThumbnailWorkCoordinator>(owner, *m_rowStore,
              lookupProvider ? std::move(lookupProvider)
                             : defaultThumbnailCacheLookupProvider(workerScheduler),
              generationProvider ? std::move(generationProvider)
                                 : defaultThumbnailGenerationProvider(workerScheduler),
              sourceAdapter ? std::move(sourceAdapter) : defaultThumbnailSourceAdapter()))
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
    int currentNumber = 0;
    for (const ActiveNavigationThumbnailRow& row : rows) {
        if (row.current) {
            currentNumber = row.number;
            break;
        }
    }
    if (m_rowStore->hasSameRowIdentities(rows)) {
        m_rowStore->setRows(std::move(rows));
        m_workCoordinator->setCurrentNumber(currentNumber);
        return;
    }

    m_workCoordinator->invalidateRows();
    m_rowStore->setRows(std::move(rows));
    m_workCoordinator->resetRows(m_rowStore->sourceKeys(), m_rowStore->navigationGeneration());
    m_workCoordinator->setCurrentNumber(currentNumber);
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
