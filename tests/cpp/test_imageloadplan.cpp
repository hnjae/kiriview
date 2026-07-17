// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imageloadplan.h"
#include "image_test_support.h"
#include "location/imagedocumentlocation.h"

#include <QObject>
#include <QSize>
#include <QTest>
#include <QUrl>
#include <QtGlobal>
#include <optional>

namespace {
using kiriview::TestSupport::archivePageUrl;
using kiriview::TestSupport::localUrl;
}

class TestImageLoadPlan : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void localFilePlansDirectImageLoad();
    void localComicBookArchivePlansArchiveListing();
    void sealedDirectSourcePlansDirectImageLoad();
    void sealedDirectorySourcePlansDocumentListing();
    void resolvedLocalDirectoryScopePlansDocumentListing();
    void containerNavigationRestoresArchiveCollectionForInteriorImage();
    void displayedArchiveContextIsKeptForInteriorImage();
    void explicitKioArchiveImagePlansDirectLoad();
};

void TestImageLoadPlan::localFilePlansDirectImageLoad()
{
    const QUrl imageUrl = localUrl(QStringLiteral("/images/page.png"));
    const kiriview::ImageLoadPlan plan = kiriview::imageLoadPlan(7,
        kiriview::ImageLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(imageUrl, {})),
        kiriview::ImageFirstDisplayDecodeContext { QSize(320, 240) });

    QCOMPARE(plan.session.id(), quint64(7));
    QCOMPARE(plan.startEffect, kiriview::ImageLoadStartEffect::DecodeImage);
    QCOMPARE(plan.session.imageUrl(), imageUrl);
    QCOMPARE(plan.session.firstDisplay().physicalViewportSize, QSize(320, 240));
    QVERIFY(plan.session.openedCollectionScope().isEmpty());
}

void TestImageLoadPlan::localComicBookArchivePlansArchiveListing()
{
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());

    const kiriview::ImageLoadPlan plan = kiriview::imageLoadPlan(8,
        kiriview::ImageLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(archiveUrl, {})));

    QCOMPARE(plan.session.id(), quint64(8));
    QCOMPARE(plan.startEffect, kiriview::ImageLoadStartEffect::LoadOpenedCollectionScopeCandidates);
    QCOMPARE(plan.session.imageUrl(), archiveUrl);
    QCOMPARE(
        plan.session.location().openedCollectionScopeSourceUrl(), archiveCollection->fileUrl());
    QCOMPARE(plan.session.location().openedCollectionScopeRootUrl(), archiveCollection->rootUrl());
    QCOMPARE(plan.session.openedCollectionScope().kind(),
        kiriview::OpenedCollectionScopeKind::ComicBookArchive);
}

void TestImageLoadPlan::sealedDirectSourcePlansDirectImageLoad()
{
    const QUrl directoryUrl = localUrl(QStringLiteral("/synthetic/album"));
    const kiriview::ImageLoadPlan plan = kiriview::imageLoadPlan(12,
        kiriview::ImageLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(directoryUrl, {})));

    QCOMPARE(plan.session.id(), quint64(12));
    QCOMPARE(plan.startEffect, kiriview::ImageLoadStartEffect::DecodeImage);
    QCOMPARE(plan.session.imageUrl(), directoryUrl);
    QVERIFY(plan.session.openedCollectionScope().isEmpty());
}

void TestImageLoadPlan::sealedDirectorySourcePlansDocumentListing()
{
    const QUrl directoryUrl = localUrl(QStringLiteral("/synthetic/album"));
    kiriview::NavigationSourceEntryFacts facts;
    facts.requestedLocalSourceIsDirectory = true;
    const kiriview::ImageLoadPlan plan = kiriview::imageLoadPlan(14,
        kiriview::ImageLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(directoryUrl, facts)));

    QCOMPARE(plan.session.id(), quint64(14));
    QCOMPARE(plan.startEffect, kiriview::ImageLoadStartEffect::LoadOpenedCollectionScopeCandidates);
    QCOMPARE(plan.session.imageUrl(), directoryUrl);
    QCOMPARE(plan.session.location().openedCollectionScopeSourceUrl(), directoryUrl);
    QCOMPARE(plan.session.location().openedCollectionScopeRootUrl(),
        kiriview::normalizedDirectoryContainerUrl(directoryUrl));
    QCOMPARE(plan.session.openedCollectionScope().kind(),
        kiriview::OpenedCollectionScopeKind::Directory);
}

void TestImageLoadPlan::resolvedLocalDirectoryScopePlansDocumentListing()
{
    const QUrl directoryUrl = localUrl(QStringLiteral("/synthetic/album"));
    const kiriview::OpenedCollectionScopeLocation directoryCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(directoryUrl,
            kiriview::normalizedDirectoryContainerUrl(directoryUrl),
            kiriview::OpenedCollectionScopeKind::Directory);

    const kiriview::ImageLoadPlan plan = kiriview::imageLoadPlan(13,
        kiriview::ImageLoadRequest::fromSameScopePageTarget(
            kiriview::ImageDocumentPageTarget(directoryUrl, kiriview::ImageDocumentPageKind::Image),
            directoryCollection, false));

    QCOMPARE(plan.session.id(), quint64(13));
    QCOMPARE(plan.startEffect, kiriview::ImageLoadStartEffect::LoadOpenedCollectionScopeCandidates);
    QCOMPARE(plan.session.imageUrl(), directoryUrl);
    QCOMPARE(
        plan.session.location().openedCollectionScopeSourceUrl(), directoryCollection.fileUrl());
    QCOMPARE(plan.session.location().openedCollectionScopeRootUrl(), directoryCollection.rootUrl());
    QCOMPARE(plan.session.openedCollectionScope().kind(),
        kiriview::OpenedCollectionScopeKind::Directory);
}

void TestImageLoadPlan::containerNavigationRestoresArchiveCollectionForInteriorImage()
{
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl imageUrl = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("page.png"));

    const kiriview::ImageLoadRequest request = kiriview::ImageLoadRequest::fromContainerTarget(
        kiriview::ImageDocumentPageTarget(imageUrl, kiriview::ImageDocumentPageKind::Image),
        *archiveCollection);
    const kiriview::ImageLoadPlan plan = kiriview::imageLoadPlan(9, request);
    const kiriview::OpenedCollectionScopeLoadPlan archivePlan
        = kiriview::openedCollectionScopeLoadPlan(request);

    QCOMPARE(plan.session.id(), quint64(9));
    QCOMPARE(plan.startEffect, kiriview::ImageLoadStartEffect::DecodeImage);
    QCOMPARE(plan.session.imageUrl(), imageUrl);
    QCOMPARE(plan.session.location().openedCollectionScopeRootUrl(), archiveCollection->rootUrl());
    QCOMPARE(plan.session.containerNavigationUrl(), archiveUrl);
    QCOMPARE(archivePlan.effect, kiriview::OpenedCollectionScopeLoadEffect::ReadImage);
    QCOMPARE(archivePlan.openedCollectionScope.rootUrl(), archiveCollection->rootUrl());
}

void TestImageLoadPlan::displayedArchiveContextIsKeptForInteriorImage()
{
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl imageUrl = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("page.png"));

    const kiriview::ImageLoadRequest request = kiriview::ImageLoadRequest::fromSameScopePageTarget(
        kiriview::ImageDocumentPageTarget(imageUrl, kiriview::ImageDocumentPageKind::Image),
        *archiveCollection, false);
    const kiriview::ImageLoadPlan plan = kiriview::imageLoadPlan(10, request);
    const kiriview::OpenedCollectionScopeLoadPlan archivePlan
        = kiriview::openedCollectionScopeLoadPlan(request);

    QCOMPARE(plan.session.id(), quint64(10));
    QCOMPARE(plan.startEffect, kiriview::ImageLoadStartEffect::DecodeImage);
    QCOMPARE(plan.session.imageUrl(), imageUrl);
    QCOMPARE(
        plan.session.location().openedCollectionScopeSourceUrl(), archiveCollection->fileUrl());
    QCOMPARE(plan.session.location().openedCollectionScopeRootUrl(), archiveCollection->rootUrl());
    QCOMPARE(archivePlan.effect, kiriview::OpenedCollectionScopeLoadEffect::ReadImage);
    QCOMPARE(archivePlan.openedCollectionScope.fileUrl(), archiveCollection->fileUrl());
    QCOMPARE(archivePlan.openedCollectionScope.rootUrl(), archiveCollection->rootUrl());
}

void TestImageLoadPlan::explicitKioArchiveImagePlansDirectLoad()
{
    const QUrl imageUrl(QStringLiteral("zip:///books/book.cbz/page.png"));
    const kiriview::ImageLoadPlan plan = kiriview::imageLoadPlan(11,
        kiriview::ImageLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(imageUrl, {})));

    QCOMPARE(plan.session.id(), quint64(11));
    QCOMPARE(plan.startEffect, kiriview::ImageLoadStartEffect::DecodeImage);
    QCOMPARE(plan.session.imageUrl(), imageUrl);
    QVERIFY(plan.session.openedCollectionScope().isEmpty());
}

QTEST_GUILESS_MAIN(TestImageLoadPlan)

#include "test_imageloadplan.moc"
