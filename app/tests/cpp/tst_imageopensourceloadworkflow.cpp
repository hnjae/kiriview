// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imageopenworkflow.h"

#include "archive/archivepath.h"
#include "image_document_plan_test_support.h"
#include "location/imagedocumentlocation.h"

#include <QDir>
#include <QObject>
#include <QTest>
#include <QUrl>
#include <optional>

namespace {
using kiriview::TestSupport::hasOperationTypes;
using kiriview::TestSupport::operationAt;
using kiriview::TestSupport::operationTypes;

QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

kiriview::OpenedCollectionScopeLocation testScope(const QUrl& url)
{
    return kiriview::OpenedCollectionScopeLocation::fromResolvedSource(
        kiriview::resolvedNavigationSource(url, kiriview::NavigationSourceEntryFacts {}), url,
        kiriview::OpenedCollectionScopeKind::GeneralArchive);
}
}

class TestImageOpenSourceLoadWorkflow : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void currentSourceLoadUsesRuntimeSnapshotAndRequestedContainer();
    void equivalentSourceKeysReuseCurrentSourceLoad();
    void equivalentOpenedCollectionSourceKeysReuseCurrentSourceLoad();
    void displayedComicBookScopeSuppressesRightToLeftReadingReset();
    void sameScopeImageNavigationStartsOpenWithoutReplacementReset();
    void replacementSourceLoadStartsFreshRuntimeWork();
    void sameRequestedCollectionWithFreshResolvedSourceStartsReplacement();
    void sourceLoadPlanResolvesRequestedRuntimePayloads();
};

void TestImageOpenSourceLoadWorkflow::equivalentSourceKeysReuseCurrentSourceLoad()
{
    const QString relativePath = QStringLiteral("relative/images/page.png");
    const QUrl relativeUrl = QUrl::fromLocalFile(relativePath);
    const QUrl absoluteUrl = QUrl::fromLocalFile(QDir::current().absoluteFilePath(relativePath));
    const kiriview::ImageDocumentSourceLoadSnapshot snapshot {
        absoluteUrl,
        {},
        false,
    };
    const kiriview::ImageDocumentSourceLoadRequest equivalentRequest
        = kiriview::ImageDocumentSourceLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(relativeUrl, {}));

    const kiriview::ImageDocumentRuntimePlan equivalentPlan
        = kiriview::ImageOpenWorkflow::sourceLoadPlan(snapshot, equivalentRequest);

    QVERIFY(hasOperationTypes(equivalentPlan,
        operationTypes<kiriview::CancelFileDeletionOperation,
            kiriview::ClearLoadingContainerNavigationUrlOperation,
            kiriview::SelectImageTargetOperation>()));
    QCOMPARE(operationAt<kiriview::SelectImageTargetOperation>(equivalentPlan, 2).target.url,
        relativeUrl);

    QUrl distinctUrl = relativeUrl;
    distinctUrl.setQuery(QStringLiteral("revision=2"));
    const kiriview::ImageDocumentSourceLoadRequest distinctRequest
        = kiriview::ImageDocumentSourceLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(distinctUrl, {}));
    const kiriview::ImageDocumentRuntimePlan distinctPlan
        = kiriview::ImageOpenWorkflow::sourceLoadPlan(snapshot, distinctRequest);

    QVERIFY(!distinctPlan.empty());
    QVERIFY(std::holds_alternative<kiriview::CancelOpenOperation>(distinctPlan.front()));
}

void TestImageOpenSourceLoadWorkflow::equivalentOpenedCollectionSourceKeysReuseCurrentSourceLoad()
{
    const QString relativePath = QStringLiteral("relative/books/book.cbz");
    const QUrl relativeUrl = QUrl::fromLocalFile(relativePath);
    const QUrl absoluteUrl = QUrl::fromLocalFile(QDir::current().absoluteFilePath(relativePath));
    const std::optional<kiriview::OpenedCollectionScopeLocation> relativeScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(relativeUrl, {}));
    const std::optional<kiriview::OpenedCollectionScopeLocation> absoluteScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(absoluteUrl, {}));
    QVERIFY(relativeScope.has_value());
    QVERIFY(absoluteScope.has_value());

    const kiriview::ImageDocumentSourceLoadSnapshot snapshot {
        absoluteUrl,
        *absoluteScope,
        false,
    };
    const kiriview::ImageDocumentSourceLoadRequest equivalentRequest
        = kiriview::ImageDocumentSourceLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(relativeUrl, {}));

    const kiriview::ImageDocumentRuntimePlan equivalentPlan
        = kiriview::ImageOpenWorkflow::sourceLoadPlan(snapshot, equivalentRequest);

    QVERIFY(hasOperationTypes(equivalentPlan,
        operationTypes<kiriview::CancelFileDeletionOperation,
            kiriview::ClearLoadingContainerNavigationUrlOperation,
            kiriview::SelectImageTargetOperation>()));

    const QUrl relativePage
        = kiriview::openedCollectionEntryUrl(*relativeScope, QStringLiteral("page.png"));
    const QUrl absolutePage
        = kiriview::openedCollectionEntryUrl(*absoluteScope, QStringLiteral("page.png"));
    const kiriview::DisplayedImageLocation relativeDisplay
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(relativePage, *relativeScope);
    const kiriview::DisplayedImageLocation absoluteDisplay
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(absolutePage, *absoluteScope);
    QVERIFY(relativeDisplay == absoluteDisplay);
    QCOMPARE(kiriview::displayScopeIdentityForLocation(relativeDisplay),
        kiriview::displayScopeIdentityForLocation(absoluteDisplay));
}

void TestImageOpenSourceLoadWorkflow::currentSourceLoadUsesRuntimeSnapshotAndRequestedContainer()
{
    const QUrl sourceUrl = localUrl(QStringLiteral("/images/page.png"));
    const QUrl containerUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const kiriview::ImageDocumentSourceLoadSnapshot snapshot {
        sourceUrl,
        {},
        true,
    };
    const kiriview::ImageDocumentSourceLoadRequest request
        = kiriview::ImageDocumentSourceLoadRequest::fromContainerTarget(
            kiriview::ImageDocumentPageTarget {
                sourceUrl,
                kiriview::ImageDocumentPageKind::Video,
            },
            testScope(containerUrl));
    const kiriview::ImageDocumentRuntimePlan plan
        = kiriview::ImageOpenWorkflow::sourceLoadPlan(snapshot, request);

    QVERIFY(hasOperationTypes(plan,
        operationTypes<kiriview::CancelFileDeletionOperation,
            kiriview::ClearLoadingContainerNavigationUrlOperation,
            kiriview::SelectImageTargetOperation, kiriview::SetContainerNavigationUrlOperation>()));
    QCOMPARE(
        operationAt<kiriview::SelectImageTargetOperation>(plan, 2).target.openedCollectionScope,
        request.openedCollectionScope());
    QCOMPARE(operationAt<kiriview::SetContainerNavigationUrlOperation>(plan, 3).url, containerUrl);
}

void TestImageOpenSourceLoadWorkflow::displayedComicBookScopeSuppressesRightToLeftReadingReset()
{
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl imageUrl(QStringLiteral("%1/01.png").arg(archiveCollection->rootUrl().toString()));
    const QUrl replacementUrl = localUrl(QStringLiteral("/images/page.png"));

    const kiriview::ImageDocumentSourceLoadSnapshot snapshot {
        replacementUrl,
        *archiveCollection,
        true,
    };

    kiriview::ImageDocumentSourceLoadRequest request
        = kiriview::ImageDocumentSourceLoadRequest::fromSameScopePageTarget(
            kiriview::ImageDocumentPageTarget(imageUrl, kiriview::ImageDocumentPageKind::Image),
            *archiveCollection);
    kiriview::ImageDocumentRuntimePlan plan
        = kiriview::ImageOpenWorkflow::sourceLoadPlan(snapshot, request);
    QVERIFY(hasOperationTypes(plan,
        operationTypes<kiriview::CancelOpenOperation, kiriview::CancelFileDeletionOperation,
            kiriview::ClearLoadingContainerNavigationUrlOperation,
            kiriview::PrepareSourceLoadOperation, kiriview::SelectImageTargetOperation,
            kiriview::BeginOpenOperation>()));
    QCOMPARE(operationAt<kiriview::SelectImageTargetOperation>(plan, 4).target.url, imageUrl);

    request = kiriview::ImageDocumentSourceLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(replacementUrl, {}));
    plan = kiriview::ImageOpenWorkflow::sourceLoadPlan(snapshot, request);
    QVERIFY(hasOperationTypes(plan,
        operationTypes<kiriview::CancelFileDeletionOperation,
            kiriview::ResetRightToLeftReadingOperation,
            kiriview::NotifyRightToLeftReadingChangedOperation,
            kiriview::ClearLoadingContainerNavigationUrlOperation,
            kiriview::SelectImageTargetOperation>()));
}

void TestImageOpenSourceLoadWorkflow::sameScopeImageNavigationStartsOpenWithoutReplacementReset()
{
    const QUrl currentUrl = localUrl(QStringLiteral("/images/current.png"));
    const QUrl targetUrl = localUrl(QStringLiteral("/images/target.png"));
    const kiriview::ImageDocumentSourceLoadRequest request
        = kiriview::ImageDocumentSourceLoadRequest::fromSameScopePageTarget(
            kiriview::ImageDocumentPageTarget(targetUrl, kiriview::ImageDocumentPageKind::Image),
            kiriview::OpenedCollectionScopeLocation::none());
    const kiriview::ImageDocumentSourceLoadSnapshot snapshot {
        currentUrl,
        {},
        false,
    };
    const kiriview::ImageDocumentRuntimePlan plan
        = kiriview::ImageOpenWorkflow::sourceLoadPlan(snapshot, request);

    QVERIFY(hasOperationTypes(plan,
        operationTypes<kiriview::CancelOpenOperation, kiriview::CancelFileDeletionOperation,
            kiriview::ClearLoadingContainerNavigationUrlOperation,
            kiriview::PrepareSourceLoadOperation, kiriview::SelectImageTargetOperation,
            kiriview::BeginOpenOperation>()));
    QCOMPARE(operationAt<kiriview::SelectImageTargetOperation>(plan, 4).target.url, targetUrl);
    QCOMPARE(operationAt<kiriview::SelectImageTargetOperation>(plan, 4).target.kind,
        kiriview::ImageDocumentPageKind::Image);
}

void TestImageOpenSourceLoadWorkflow::replacementSourceLoadStartsFreshRuntimeWork()
{
    const QUrl currentUrl = localUrl(QStringLiteral("/images/current.png"));
    const QUrl replacementUrl = localUrl(QStringLiteral("/images/replacement.png"));
    const kiriview::ImageDocumentSourceLoadRequest request
        = kiriview::ImageDocumentSourceLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(replacementUrl, {}));
    const kiriview::ImageDocumentSourceLoadSnapshot snapshot {
        currentUrl,
        {},
        true,
    };
    const kiriview::ImageDocumentRuntimePlan plan
        = kiriview::ImageOpenWorkflow::sourceLoadPlan(snapshot, request);

    QVERIFY(hasOperationTypes(plan,
        operationTypes<kiriview::CancelOpenOperation, kiriview::CancelFileDeletionOperation,
            kiriview::CancelAllNavigationOperation, kiriview::CancelPredecodeOperation,
            kiriview::ResetRightToLeftReadingOperation, kiriview::ClearSecondaryPageOperation,
            kiriview::SetLoadingContainerNavigationUrlOperation,
            kiriview::PrepareSourceLoadOperation, kiriview::SelectImageTargetOperation,
            kiriview::BeginOpenOperation, kiriview::NotifyRightToLeftReadingChangedOperation>()));
}

void TestImageOpenSourceLoadWorkflow::
    sameRequestedCollectionWithFreshResolvedSourceStartsReplacement()
{
    const QUrl requestedUrl = localUrl(QStringLiteral("/portal/book.cbz"));
    const kiriview::ResolvedNavigationSource firstSource(requestedUrl, {},
        localUrl(QStringLiteral("/resolved/first/book.cbz")),
        kiriview::NavigationSourceEntryKind::Archive);
    const kiriview::ResolvedNavigationSource reassignedSource(requestedUrl, {},
        localUrl(QStringLiteral("/resolved/second/book.cbz")),
        kiriview::NavigationSourceEntryKind::Archive);
    const std::optional<kiriview::OpenedCollectionScopeLocation> firstScope
        = kiriview::openedCollectionScopeLocationForResolvedExternalSource(firstSource);
    QVERIFY(firstScope.has_value());
    const kiriview::ImageDocumentSourceLoadRequest request
        = kiriview::ImageDocumentSourceLoadRequest::fromExternalSource(reassignedSource);
    const kiriview::ImageDocumentSourceLoadSnapshot snapshot {
        requestedUrl,
        *firstScope,
        false,
    };

    const kiriview::ImageDocumentRuntimePlan plan
        = kiriview::ImageOpenWorkflow::sourceLoadPlan(snapshot, request);
    QVERIFY(hasOperationTypes(plan,
        operationTypes<kiriview::CancelOpenOperation, kiriview::CancelFileDeletionOperation,
            kiriview::CancelAllNavigationOperation, kiriview::CancelPredecodeOperation,
            kiriview::ClearSecondaryPageOperation,
            kiriview::SetLoadingContainerNavigationUrlOperation,
            kiriview::PrepareSourceLoadOperation, kiriview::SelectImageTargetOperation,
            kiriview::BeginOpenOperation>()));
}

void TestImageOpenSourceLoadWorkflow::sourceLoadPlanResolvesRequestedRuntimePayloads()
{
    const QUrl sourceUrl = localUrl(QStringLiteral("/books/page.png"));
    const QUrl containerUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const kiriview::ImageDocumentSourceLoadRequest request
        = kiriview::ImageDocumentSourceLoadRequest::fromContainerTarget(
            kiriview::ImageDocumentPageTarget {
                sourceUrl,
                kiriview::ImageDocumentPageKind::Video,
            },
            testScope(containerUrl));
    const kiriview::ImageDocumentSourceLoadSnapshot replacementSnapshot {
        localUrl(QStringLiteral("/images/current.png")),
        {},
        false,
    };
    const kiriview::ImageDocumentRuntimePlan replacementPlan
        = kiriview::ImageOpenWorkflow::sourceLoadPlan(replacementSnapshot, request);

    QVERIFY(hasOperationTypes(replacementPlan,
        operationTypes<kiriview::CancelOpenOperation, kiriview::CancelFileDeletionOperation,
            kiriview::CancelAllNavigationOperation, kiriview::CancelPredecodeOperation,
            kiriview::ClearSecondaryPageOperation,
            kiriview::SetLoadingContainerNavigationUrlOperation,
            kiriview::PrepareSourceLoadOperation, kiriview::SelectImageTargetOperation,
            kiriview::BeginOpenOperation>()));
    QCOMPARE(
        operationAt<kiriview::SetLoadingContainerNavigationUrlOperation>(replacementPlan, 5).url,
        containerUrl);
    QCOMPARE(
        operationAt<kiriview::PrepareSourceLoadOperation>(replacementPlan, 6).request.sourceUrl(),
        sourceUrl);
    QCOMPARE(
        operationAt<kiriview::PrepareSourceLoadOperation>(replacementPlan, 6).request.sourceKind(),
        kiriview::ImageDocumentPageKind::Video);
    QCOMPARE(operationAt<kiriview::PrepareSourceLoadOperation>(replacementPlan, 6)
                 .request.containerNavigationUrl(),
        containerUrl);
    QCOMPARE(operationAt<kiriview::SelectImageTargetOperation>(replacementPlan, 7).target.url,
        sourceUrl);
    QCOMPARE(operationAt<kiriview::SelectImageTargetOperation>(replacementPlan, 7).target.kind,
        kiriview::ImageDocumentPageKind::Video);

    const kiriview::ImageDocumentSourceLoadSnapshot currentSnapshot {
        sourceUrl,
        {},
        false,
    };
    const kiriview::ImageDocumentRuntimePlan currentPlan
        = kiriview::ImageOpenWorkflow::sourceLoadPlan(currentSnapshot, request);
    QCOMPARE(operationAt<kiriview::SetContainerNavigationUrlOperation>(currentPlan, 3).url,
        containerUrl);
}

QTEST_GUILESS_MAIN(TestImageOpenSourceLoadWorkflow)

#include "tst_imageopensourceloadworkflow.moc"
