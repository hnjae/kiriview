// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "navigation/imagedocumentpagecandidatelistsource.h"

#include "candidate_test_support.h"
#include "location/imagedocumentlocation.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <optional>
#include <type_traits>

namespace {
using kiriview::ImageDocumentPageCandidateListContext;
using kiriview::TestSupport::archivePageUrl;
using kiriview::TestSupport::localUrl;

using OpenedCollectionCandidateContext
    = ImageDocumentPageCandidateListContext::OpenedCollectionScopeContext;
using DirectoryCandidateContext = ImageDocumentPageCandidateListContext::DirectoryContext;

template <typename ExpectedContext>
const ExpectedContext* typedCandidateContext(const ImageDocumentPageCandidateListContext& context)
{
    return context.visit([](const auto& typedContext) -> const ExpectedContext* {
        using Context = std::decay_t<decltype(typedContext)>;
        if constexpr (std::is_same_v<Context, ExpectedContext>) {
            return &typedContext;
        } else {
            return nullptr;
        }
    });
}
}

class TestImageDocumentPageCandidateListSource : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void displayedImageContextsSelectDirectoryOrArchiveListing();
    void candidateListSourceIdentityComparesNormalizedSources();
};

void TestImageDocumentPageCandidateListSource::
    displayedImageContextsSelectDirectoryOrArchiveListing()
{
    const QUrl fileUrl = localUrl(QStringLiteral("/images/02.png"));
    const std::optional<kiriview::ImageDocumentPageCandidateListContext> directoryContext
        = kiriview::imageDocumentPageCandidateListContextForDisplayedImage(
            kiriview::DisplayedImageLocation::fromUrl(fileUrl));
    QVERIFY(directoryContext.has_value());
    QCOMPARE(directoryContext->currentUrl(), fileUrl);
    const DirectoryCandidateContext* directory
        = typedCandidateContext<DirectoryCandidateContext>(*directoryContext);
    QVERIFY(directory != nullptr);
    QCOMPARE(directory->directoryUrl, localUrl(QStringLiteral("/images/")));
    QVERIFY(directoryContext->openedCollectionScope().isEmpty());

    const QUrl requestedPortalUrl = localUrl(QStringLiteral("/portal/document/current.png"));
    const QUrl resolvedNavigationUrl = localUrl(QStringLiteral("/resolved/media/current.png"));
    const std::optional<kiriview::ImageDocumentPageCandidateListContext> resolvedContext
        = kiriview::imageDocumentPageCandidateListContextForDisplayedImage(
            kiriview::DisplayedImageLocation::fromResolvedSource(
                kiriview::ResolvedNavigationSource(requestedPortalUrl, {}, resolvedNavigationUrl)));
    QVERIFY(resolvedContext.has_value());
    QCOMPARE(resolvedContext->currentUrl(), resolvedNavigationUrl);
    const DirectoryCandidateContext* resolvedDirectory
        = typedCandidateContext<DirectoryCandidateContext>(*resolvedContext);
    QVERIFY(resolvedDirectory != nullptr);
    QCOMPARE(resolvedDirectory->directoryUrl, localUrl(QStringLiteral("/resolved/media/")));

    const QUrl queryScopedNavigationUrl(
        QStringLiteral("smb://example.test/media/current.png?token=abc#entry"));
    const kiriview::DisplayedImageLocation queryScopedLocation
        = kiriview::DisplayedImageLocation::fromResolvedSource(
            kiriview::ResolvedNavigationSource(requestedPortalUrl, {}, queryScopedNavigationUrl));
    const std::optional<kiriview::ImageDocumentPageCandidateListContext> queryScopedContext
        = kiriview::imageDocumentPageCandidateListContextForDisplayedImage(queryScopedLocation);
    QVERIFY(queryScopedLocation.directMediaPageScopeIdentity().has_value());
    QVERIFY(queryScopedContext.has_value());
    const DirectoryCandidateContext* queryScopedDirectory
        = typedCandidateContext<DirectoryCandidateContext>(*queryScopedContext);
    QVERIFY(queryScopedDirectory != nullptr);
    QCOMPARE(queryScopedContext->currentUrl(),
        queryScopedLocation.directMediaPageScopeIdentity()->currentKey().normalizedUrl);
    QCOMPARE(queryScopedDirectory->directoryUrl,
        queryScopedLocation.directMediaPageScopeIdentity()->parentKey().normalizedUrl);
    QCOMPARE(queryScopedDirectory->directoryUrl.query(), QStringLiteral("token=abc"));
    QCOMPARE(queryScopedDirectory->directoryUrl.fragment(), QStringLiteral("entry"));
    const QUrl sameScopeSibling(
        QStringLiteral("smb://example.test/media/adjacent.png?token=abc#entry"));
    const QUrl otherQuerySibling(
        QStringLiteral("smb://example.test/media/adjacent.png?token=other#entry"));
    const QUrl otherFragmentSibling(
        QStringLiteral("smb://example.test/media/adjacent.png?token=abc#other"));
    QVERIFY(kiriview::directMediaPageScopeIdentityForOwnerCandidate(
        sameScopeSibling, queryScopedLocation.directMediaPageScopeIdentity()->parentKey())
            .has_value());
    QVERIFY(!kiriview::directMediaPageScopeIdentityForOwnerCandidate(
        otherQuerySibling, queryScopedLocation.directMediaPageScopeIdentity()->parentKey())
            .has_value());
    QVERIFY(!kiriview::directMediaPageScopeIdentityForOwnerCandidate(
        otherFragmentSibling, queryScopedLocation.directMediaPageScopeIdentity()->parentKey())
            .has_value());

    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> openedCollectionScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(openedCollectionScope.has_value());
    const QUrl pageUrl = archivePageUrl(openedCollectionScope->rootUrl(), QStringLiteral("02.png"));

    const std::optional<kiriview::ImageDocumentPageCandidateListContext> archiveContext
        = kiriview::imageDocumentPageCandidateListContextForDisplayedImage(
            kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                pageUrl, *openedCollectionScope));
    QVERIFY(archiveContext.has_value());
    QCOMPARE(archiveContext->currentUrl(), pageUrl);
    const OpenedCollectionCandidateContext* archive
        = typedCandidateContext<OpenedCollectionCandidateContext>(*archiveContext);
    QVERIFY(archive != nullptr);
    QCOMPARE(archive->openedCollectionScope.rootUrl(), openedCollectionScope->rootUrl());
    QCOMPARE(archiveContext->openedCollectionScope().rootUrl(), openedCollectionScope->rootUrl());

    const QUrl directArchiveUrl = localUrl(QStringLiteral("/books/book.zip"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> directArchiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(directArchiveUrl, {}));
    QVERIFY(directArchiveCollection.has_value());
    const QUrl directArchivePageUrl
        = archivePageUrl(directArchiveCollection->rootUrl(), QStringLiteral("02.png"));

    const std::optional<kiriview::ImageDocumentPageCandidateListContext> directArchiveContext
        = kiriview::imageDocumentPageCandidateListContextForDisplayedImage(
            kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                directArchivePageUrl, *directArchiveCollection));
    QVERIFY(directArchiveContext.has_value());
    QCOMPARE(directArchiveContext->currentUrl(), directArchivePageUrl);
    const OpenedCollectionCandidateContext* directArchive
        = typedCandidateContext<OpenedCollectionCandidateContext>(*directArchiveContext);
    QVERIFY(directArchive != nullptr);
    QCOMPARE(directArchive->openedCollectionScope.rootUrl(), directArchiveCollection->rootUrl());
    QCOMPARE(directArchiveContext->openedCollectionScope().rootUrl(),
        directArchiveCollection->rootUrl());

    const QUrl explicitArchiveImageUrl(QStringLiteral("zip:///books/book.cbz/chapter/02.png"));
    const std::optional<kiriview::ImageDocumentPageCandidateListContext> explicitArchiveContext
        = kiriview::imageDocumentPageCandidateListContextForDisplayedImage(
            kiriview::DisplayedImageLocation::fromUrl(explicitArchiveImageUrl));
    QVERIFY(explicitArchiveContext.has_value());
    QCOMPARE(explicitArchiveContext->currentUrl(), explicitArchiveImageUrl);
    const DirectoryCandidateContext* explicitArchiveDirectory
        = typedCandidateContext<DirectoryCandidateContext>(*explicitArchiveContext);
    QVERIFY(explicitArchiveDirectory != nullptr);
    QCOMPARE(explicitArchiveDirectory->directoryUrl,
        QUrl(QStringLiteral("zip:///books/book.cbz/chapter/")));
}

void TestImageDocumentPageCandidateListSource::
    candidateListSourceIdentityComparesNormalizedSources()
{
    const ImageDocumentPageCandidateListContext directoryContext
        = ImageDocumentPageCandidateListContext::forDirectory(
            localUrl(QStringLiteral("/images/01.png")), localUrl(QStringLiteral("/images/")));
    const ImageDocumentPageCandidateListContext normalizedDirectoryContext
        = ImageDocumentPageCandidateListContext::forDirectory(
            localUrl(QStringLiteral("/images/./01.png")), localUrl(QStringLiteral("/images/./")));
    const ImageDocumentPageCandidateListContext otherDirectoryContext
        = ImageDocumentPageCandidateListContext::forDirectory(
            localUrl(QStringLiteral("/other/01.png")), localUrl(QStringLiteral("/other/")));

    QVERIFY(kiriview::sameImageDocumentPageCandidateListSource(
        directoryContext.source(), normalizedDirectoryContext.source()));
    QVERIFY(!kiriview::sameImageDocumentPageCandidateListSource(
        directoryContext.source(), otherDirectoryContext.source()));
    const ImageDocumentPageCandidateListContext invalidDirectoryContext
        = ImageDocumentPageCandidateListContext::forDirectory({}, {});
    QVERIFY(kiriview::sameImageDocumentPageCandidateListSource(
        invalidDirectoryContext.source(), invalidDirectoryContext.source()));

    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> openedCollectionScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(openedCollectionScope.has_value());
    const QUrl pageUrl = archivePageUrl(openedCollectionScope->rootUrl(), QStringLiteral("01.png"));
    const ImageDocumentPageCandidateListContext archiveContext
        = ImageDocumentPageCandidateListContext::forOpenedCollectionScope(
            pageUrl, *openedCollectionScope);
    const kiriview::OpenedCollectionScopeLocation reassignedScope
        = kiriview::OpenedCollectionScopeLocation::fromResolvedSource(
            kiriview::ResolvedNavigationSource(openedCollectionScope->fileUrl(),
                openedCollectionScope->source().facts(),
                localUrl(QStringLiteral("/resolved/reassigned/book.cbz")),
                openedCollectionScope->source().entryKind()),
            openedCollectionScope->rootUrl(), openedCollectionScope->kind());
    const ImageDocumentPageCandidateListContext reassignedArchiveContext
        = ImageDocumentPageCandidateListContext::forOpenedCollectionScope(pageUrl, reassignedScope);

    QVERIFY(!kiriview::sameImageDocumentPageCandidateListSource(
        directoryContext.source(), archiveContext.source()));
    QVERIFY(!kiriview::sameImageDocumentPageCandidateListSource(
        archiveContext.source(), reassignedArchiveContext.source()));
}

QTEST_GUILESS_MAIN(TestImageDocumentPageCandidateListSource)

#include "tst_imagedocumentpagecandidatelistsource.moc"
