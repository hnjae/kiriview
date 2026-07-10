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
    if (m_rowStore->hasSameRowIdentities(rows)) {
        m_rowStore->setRows(std::move(rows));
        return;
    }

    m_workCoordinator->invalidateRows();
    m_rowStore->setRows(std::move(rows));
    m_workCoordinator->resetRows(m_rowStore->rowCount(), m_rowStore->navigationGeneration());
}

void ActiveNavigationThumbnailRuntime::setCurrentNumber(int currentNumber)
{
    m_rowStore->setCurrentNumber(currentNumber);
}

bool ActiveNavigationThumbnailRuntime::beginDemandWindow(quint64 navigationGeneration)
{
    return m_workCoordinator->beginDemandWindow(navigationGeneration);
}

void ActiveNavigationThumbnailRuntime::finishDemandWindow(quint64 navigationGeneration)
{
    m_workCoordinator->finishDemandWindow(navigationGeneration);
}

bool ActiveNavigationThumbnailRuntime::reportDemand(int number, const QUrl& url,
    ActiveNavigationThumbnailDemandBucket bucket, ActiveNavigationThumbnailDemandPriority priority,
    quint64 navigationGeneration)
{
    return m_workCoordinator->reportDemand(number, url, bucket, priority, navigationGeneration);
}

bool ActiveNavigationThumbnailRuntime::applyCompletion(
    const ActiveNavigationThumbnailCompletion& completion)
{
    if (!m_workCoordinator->acceptCompletion(completion)) {
        return false;
    }
    m_rowStore->applyResult(completion.sourceKey, completion.result);
    return true;
}

ThumbnailSourceKey ActiveNavigationThumbnailRuntime::sourceKeyAt(std::size_t row) const
{
    return m_rowStore->sourceKeyAt(row);
}

ActiveNavigationThumbnailResult ActiveNavigationThumbnailRuntime::resultAt(std::size_t row) const
{
    return m_rowStore->resultAt(row);
}

const std::vector<ActiveNavigationThumbnailFailureDiagnostic>&
ActiveNavigationThumbnailRuntime::failureDiagnostics() const
{
    return m_workCoordinator->failureDiagnostics();
}

qsizetype ActiveNavigationThumbnailRuntime::activeJobCount() const
{
    return m_workCoordinator->activeJobCount();
}

qsizetype ActiveNavigationThumbnailRuntime::canceledJobCount() const
{
    return m_workCoordinator->canceledJobCount();
}
}
