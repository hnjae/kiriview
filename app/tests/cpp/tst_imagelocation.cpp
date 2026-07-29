// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "location/imagelocation.h"

#include "candidate_test_support.h"

#include <QByteArray>
#include <QObject>
#include <QTest>
#include <optional>

class TestImageLocation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void locationValuesStoreCanonicalUrlIdentity();
    void archiveCollectionIdentityComparesNormalizedUrlsAndKind();
    void directMediaDisplayIdentityPreservesResolvedNavigationScope();
};

void TestImageLocation::locationValuesStoreCanonicalUrlIdentity()
{
    const QUrl rawImageUrl
        = kiriview::TestSupport::localUrl(QStringLiteral("/images/chapter/../page.png"));
    const QUrl normalizedImageUrl
        = kiriview::TestSupport::localUrl(QStringLiteral("/images/page.png"));
    const QUrl rawArchiveRootUrl(QStringLiteral("zip:///books/./book.cbz/"));
    const QUrl normalizedArchiveRootUrl(QStringLiteral("zip:///books/book.cbz/"));
    const QUrl invalidImageUrl
        = QUrl::fromEncoded(QByteArrayLiteral("http://example.test/%zz"), QUrl::StrictMode);

    const kiriview::ImageLocation imageLocation = kiriview::ImageLocation::fromUrl(rawImageUrl);
    QCOMPARE(imageLocation.url(), normalizedImageUrl);
    QVERIFY(imageLocation == kiriview::ImageLocation::fromUrl(normalizedImageUrl));
    const kiriview::ImageLocation invalidImageLocation
        = kiriview::ImageLocation::fromUrl(invalidImageUrl);
    QVERIFY(invalidImageLocation == invalidImageLocation);

    const kiriview::ContainerLocation containerLocation
        = kiriview::ContainerLocation::fromUrl(rawImageUrl);
    QCOMPARE(containerLocation.url(), normalizedImageUrl);
    QVERIFY(containerLocation == kiriview::ContainerLocation::fromUrl(normalizedImageUrl));
    QVERIFY(kiriview::ContainerLocation::none() == kiriview::ContainerLocation::none());

    const kiriview::OpenedCollectionScopeLocation archiveCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            rawImageUrl, rawArchiveRootUrl, kiriview::OpenedCollectionScopeKind::ComicBookArchive);
    QCOMPARE(archiveCollection.fileUrl(), normalizedImageUrl);
    QCOMPARE(archiveCollection.rootUrl(), normalizedArchiveRootUrl);
    QVERIFY(archiveCollection
        == kiriview::OpenedCollectionScopeLocation::fromUrls(normalizedImageUrl,
            normalizedArchiveRootUrl, kiriview::OpenedCollectionScopeKind::ComicBookArchive));

    const kiriview::DisplayedImageLocation displayedLocation
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
            rawImageUrl, archiveCollection);
    QCOMPARE(displayedLocation.imageUrl(), normalizedImageUrl);
    QVERIFY(displayedLocation
        == kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
            normalizedImageUrl, archiveCollection));
}

void TestImageLocation::archiveCollectionIdentityComparesNormalizedUrlsAndKind()
{
    const kiriview::OpenedCollectionScopeLocation archiveCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            kiriview::TestSupport::localUrl(QStringLiteral("/books/book.cbz")),
            QUrl(QStringLiteral("kio-fuse:///books/book.cbz/")),
            kiriview::OpenedCollectionScopeKind::ComicBookArchive);
    const kiriview::OpenedCollectionScopeLocation normalizedArchiveCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            kiriview::TestSupport::localUrl(QStringLiteral("/books/./book.cbz")),
            QUrl(QStringLiteral("kio-fuse:///books/./book.cbz/")),
            kiriview::OpenedCollectionScopeKind::ComicBookArchive);
    const kiriview::OpenedCollectionScopeLocation differentKind
        = kiriview::OpenedCollectionScopeLocation::fromUrls(archiveCollection.fileUrl(),
            archiveCollection.rootUrl(), kiriview::OpenedCollectionScopeKind::GeneralArchive);

    QVERIFY(kiriview::sameOpenedCollectionScopeLocation(
        archiveCollection, normalizedArchiveCollection));
    QVERIFY(!kiriview::sameOpenedCollectionScopeLocation(archiveCollection, differentKind));
}

void TestImageLocation::directMediaDisplayIdentityPreservesResolvedNavigationScope()
{
    const QUrl requestedUrl
        = kiriview::TestSupport::localUrl(QStringLiteral("/portal/document/page.png"));
    const kiriview::ResolvedNavigationSource firstSource(requestedUrl, {},
        kiriview::TestSupport::localUrl(QStringLiteral("/resolved/first/page.png")));
    const kiriview::ResolvedNavigationSource normalizedEquivalentSource(requestedUrl, {},
        kiriview::TestSupport::localUrl(QStringLiteral("/resolved/first/chapter/../page.png")));
    const kiriview::ResolvedNavigationSource trailingSlashEquivalentSource(
        requestedUrl, {}, QUrl(QStringLiteral("file:///resolved/first/chapter/../page.png/")));
    const kiriview::ResolvedNavigationSource requestedAliasSource(
        kiriview::TestSupport::localUrl(QStringLiteral("/portal/alias/page.png")), {},
        kiriview::TestSupport::localUrl(QStringLiteral("/resolved/first/page.png")));
    const kiriview::ResolvedNavigationSource otherScopeSource(requestedUrl, {},
        kiriview::TestSupport::localUrl(QStringLiteral("/resolved/second/page.png")));
    const std::optional<kiriview::DirectMediaPageScopeIdentity> firstScopeIdentity
        = kiriview::directMediaPageScopeIdentityForSource(firstSource);
    QVERIFY(firstScopeIdentity.has_value());
    QVERIFY(kiriview::directMediaPageScopeIdentityForOwnerCandidate(
        kiriview::TestSupport::localUrl(QStringLiteral("/resolved/first/adjacent.png")),
        firstScopeIdentity->parentKey())
            .has_value());
    QVERIFY(!kiriview::directMediaPageScopeIdentityForOwnerCandidate(
        kiriview::TestSupport::localUrl(QStringLiteral("/resolved/second/adjacent.png")),
        firstScopeIdentity->parentKey())
            .has_value());

    const kiriview::DisplayedImageLocation first
        = kiriview::DisplayedImageLocation::fromResolvedSource(firstSource);
    const kiriview::DisplayedImageLocation normalizedEquivalent
        = kiriview::DisplayedImageLocation::fromResolvedSource(normalizedEquivalentSource);
    const kiriview::DisplayedImageLocation trailingSlashEquivalent
        = kiriview::DisplayedImageLocation::fromResolvedSource(trailingSlashEquivalentSource);
    const kiriview::DisplayedImageLocation requestedAlias
        = kiriview::DisplayedImageLocation::fromResolvedSource(requestedAliasSource);
    const kiriview::DisplayedImageLocation otherScope
        = kiriview::DisplayedImageLocation::fromResolvedSource(otherScopeSource);

    QCOMPARE(first.imageUrl(), requestedUrl);
    QCOMPARE(otherScope.imageUrl(), requestedUrl);
    QVERIFY(first == normalizedEquivalent);
    QCOMPARE(kiriview::displayScopeIdentityForLocation(first),
        kiriview::displayScopeIdentityForLocation(normalizedEquivalent));
    QVERIFY(first == trailingSlashEquivalent);
    QCOMPARE(kiriview::displayScopeIdentityForLocation(first),
        kiriview::displayScopeIdentityForLocation(trailingSlashEquivalent));
    QVERIFY(first != requestedAlias);
    QCOMPARE(kiriview::displayScopeIdentityForLocation(first),
        kiriview::displayScopeIdentityForLocation(requestedAlias));
    QVERIFY(first != otherScope);
    QVERIFY(kiriview::displayScopeIdentityForLocation(first)
        != kiriview::displayScopeIdentityForLocation(otherScope));
}

QTEST_GUILESS_MAIN(TestImageLocation)

#include "tst_imagelocation.moc"
