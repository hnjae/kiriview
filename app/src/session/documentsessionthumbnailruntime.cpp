// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionthumbnailruntime.h"

#include "archive/openedcollectionthumbnailpolicy.h"

#include <utility>

namespace {
kiriview::ThumbnailSourceAdapter documentSessionThumbnailSourceAdapter(
    kiriview::DocumentSessionImageDocumentSnapshotPort* imageDocument,
    kiriview::ThumbnailSourceAdapter injectedAdapter)
{
    return [imageDocument, injectedAdapter = std::move(injectedAdapter),
               directAdapter = kiriview::defaultThumbnailSourceAdapter()](
               const kiriview::ThumbnailSourceAdapterRequest& request) mutable {
        if (injectedAdapter) {
            kiriview::ThumbnailSourceAdapterPlan plan = injectedAdapter(request);
            if (plan.kind != kiriview::ThumbnailSourceAdapterPlanKind::Unsupported) {
                return plan;
            }
        }

        kiriview::ThumbnailSourceAdapterPlan directPlan = directAdapter(request);
        if (directPlan.kind != kiriview::ThumbnailSourceAdapterPlanKind::Unsupported) {
            return directPlan;
        }

        if (imageDocument == nullptr || !imageDocument->snapshot
            || request.sourceKey.row.sourceKind
                != kiriview::activeNavigationThumbnailSourceKindIdentity(
                    kiriview::ActiveNavigationThumbnailSourceKind::ImageDocumentPageImage)) {
            return kiriview::ThumbnailSourceAdapterPlan {};
        }

        const kiriview::OpenedCollectionScopeLocation openedCollectionScope
            = imageDocument->snapshot().displayedOpenedCollectionScope;
        const kiriview::OpenedCollectionThumbnailSourcePlan collectionPlan
            = kiriview::openedCollectionThumbnailSourcePlan(openedCollectionScope,
                request.sourceKey.sourceUrl, kiriview::ImageDocumentPageKind::Image);
        if (collectionPlan.kind
            != kiriview::OpenedCollectionThumbnailSourcePlanKind::CacheableOpenedCollectionEntry) {
            return kiriview::ThumbnailSourceAdapterPlan {};
        }

        kiriview::ThumbnailSourceAdapterPlan plan;
        plan.kind = kiriview::ThumbnailSourceAdapterPlanKind::CacheableOpenedCollectionEntry;
        plan.openedCollectionScope = collectionPlan.openedCollectionScope;
        return plan;
    };
}

kiriview::ActiveNavigationThumbnailRuntimeDependencies documentSessionThumbnailDependencies(
    kiriview::DocumentSessionImageDocumentSnapshotPort* imageDocument,
    kiriview::ActiveNavigationThumbnailRuntimeDependencies dependencies)
{
    dependencies.sourceAdapter = documentSessionThumbnailSourceAdapter(
        imageDocument, std::move(dependencies.sourceAdapter));
    return dependencies;
}
}

namespace kiriview {
DocumentSessionThumbnailRuntime::DocumentSessionThumbnailRuntime(QObject* owner,
    DocumentSessionImageDocumentSnapshotPort* imageDocument,
    ActiveNavigationThumbnailRuntimeDependencies dependencies)
    : m_runtime(owner, documentSessionThumbnailDependencies(imageDocument, std::move(dependencies)))
{
}

QAbstractListModel* DocumentSessionThumbnailRuntime::model() const { return m_runtime.model(); }

quint64 DocumentSessionThumbnailRuntime::navigationGeneration() const
{
    return m_runtime.navigationGeneration();
}

void DocumentSessionThumbnailRuntime::setRows(std::vector<ActiveNavigationThumbnailRow> rows)
{
    m_runtime.setRows(std::move(rows));
}

void DocumentSessionThumbnailRuntime::setCurrentNumber(int currentNumber)
{
    m_runtime.setCurrentNumber(currentNumber);
}

bool DocumentSessionThumbnailRuntime::replaceDemandSnapshot(
    const ActiveNavigationThumbnailDemandSnapshot& snapshot)
{
    return m_runtime.replaceDemandSnapshot(snapshot);
}
}
