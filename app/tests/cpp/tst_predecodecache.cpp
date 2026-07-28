// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "cache/imagecachepolicy.h"
#include "image_test_support.h"
#include "predecode/predecodecache.h"

#include <QImage>
#include <QObject>
#include <QTest>
#include <QUrl>
#include <Qt>
#include <optional>
#include <utility>
#include <vector>

namespace {
using kiriview::TestSupport::indexedImageUrl;
using kiriview::TestSupport::staticDisplayTestImagePayload;

QImage cacheImage()
{
    QImage image(10, 1, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    return image;
}

QImage tooLargeImage()
{
    QImage image(30, 1, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    return image;
}

kiriview::OpenedCollectionScopeLocation comicBookArchiveCollection()
{
    return kiriview::OpenedCollectionScopeLocation::fromUrls(
        QUrl::fromLocalFile(QStringLiteral("/books/book.cbz")),
        QUrl(QStringLiteral("zip:///books/book.cbz/")),
        kiriview::OpenedCollectionScopeKind::ComicBookArchive);
}

kiriview::DisplayedImageLocation displayedLocation(const QUrl& url,
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope
    = kiriview::OpenedCollectionScopeLocation::none())
{
    return kiriview::DisplayedImageLocation::fromUrl(url, openedCollectionScope);
}

std::vector<kiriview::DisplayedImageLocation> displayedLocations(const std::vector<QUrl>& urls,
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope
    = kiriview::OpenedCollectionScopeLocation::none())
{
    std::vector<kiriview::DisplayedImageLocation> locations;
    locations.reserve(urls.size());
    for (const QUrl& url : urls) {
        locations.push_back(displayedLocation(url, openedCollectionScope));
    }
    return locations;
}

kiriview::PredecodeActiveLoads activeLoads(std::vector<kiriview::DisplayedImageLocation> locations)
{
    return kiriview::PredecodeActiveLoads::fromLocations(locations);
}

kiriview::StaticDisplayImagePayload cacheDisplayImage(
    const QImage& image, bool firstDisplay = false)
{
    const kiriview::DisplayImageQuality quality = firstDisplay
        ? kiriview::DisplayImageQuality::FirstDisplay
        : kiriview::DisplayImageQuality::Exact;
    return staticDisplayTestImagePayload(image, image, quality);
}

kiriview::PredecodeCache defaultCache()
{
    return kiriview::PredecodeCache(kiriview::predecodeCacheByteBudgetForSystemMemory(0));
}
}

class TestPredecodeCache : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void queueContainsOnlyMissingWindowImages();
    void queueSkipsAllDisplayedWindowImages();
    void takeNextRequestDiscardsSkippedQueuePrefix();
    void cacheStoresAndFindsWindowImages();
    void cacheFindsImagesByUrlAndOpenedCollectionScope();
    void queueAndActiveWorkDistinguishOpenedCollectionScope();
    void cacheRetainsDisplayedImagesBeforeAdjacentImages();
    void cacheRetainsRecentDisplayedImagesBeforeAdjacentImages();
    void cacheKeepsOnlyFourRecentDisplayedImages();
    void cacheRejectsUncacheableAndOversizedImages();
    void cacheRejectsProvisionalPreviewPayloads();
    void cacheEvictsLowestPriorityImagesWhenBudgetIsExceeded();
    void cacheRetainsWarmImagesAcrossWindowReprioritization();
    void cacheRefreshesWarmImageRecencyOnLookup();
};

void TestPredecodeCache::queueContainsOnlyMissingWindowImages()
{
    kiriview::PredecodeCache cache = defaultCache();
    const QUrl displayedUrl = indexedImageUrl(0);
    const QUrl firstQueuedUrl = indexedImageUrl(1);
    const QUrl secondQueuedUrl = indexedImageUrl(2);

    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();
    cache.setWindowLocations(displayedLocations(
        { displayedUrl, firstQueuedUrl, firstQueuedUrl, QUrl(), secondQueuedUrl },
        openedCollectionScope));
    const QImage firstImage = cacheImage();
    cache.cacheImage(
        displayedLocation(firstQueuedUrl, openedCollectionScope), cacheDisplayImage(firstImage));
    cache.enqueueMissingWindowLoads(
        displayedLocation(displayedUrl, openedCollectionScope), kiriview::PredecodeActiveLoads {});

    QVERIFY(cache.isInFlight(displayedLocation(secondQueuedUrl, openedCollectionScope),
        kiriview::PredecodeActiveLoads {}));

    const std::optional<kiriview::PredecodeRequest> request = cache.takeNextRequest(
        activeLoads({ displayedLocation(indexedImageUrl(9), openedCollectionScope) }));
    QVERIFY(request.has_value());
    QCOMPARE(request->location.imageUrl(), secondQueuedUrl);
    QCOMPARE(request->location.openedCollectionScope().rootUrl(), openedCollectionScope.rootUrl());
    QVERIFY(!cache.takeNextRequest(kiriview::PredecodeActiveLoads {}).has_value());
}

void TestPredecodeCache::queueSkipsAllDisplayedWindowImages()
{
    kiriview::PredecodeCache cache = defaultCache();
    const QUrl primaryDisplayedUrl = indexedImageUrl(0);
    const QUrl secondaryDisplayedUrl = indexedImageUrl(1);
    const QUrl queuedUrl = indexedImageUrl(2);
    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();

    cache.setDisplayedLocations(
        displayedLocations({ primaryDisplayedUrl, secondaryDisplayedUrl }, openedCollectionScope));
    cache.setWindowLocations(displayedLocations(
        { primaryDisplayedUrl, secondaryDisplayedUrl, queuedUrl }, openedCollectionScope));
    cache.enqueueMissingWindowLoads(displayedLocation(primaryDisplayedUrl, openedCollectionScope),
        kiriview::PredecodeActiveLoads {});

    const std::optional<kiriview::PredecodeRequest> request
        = cache.takeNextRequest(kiriview::PredecodeActiveLoads {});
    QVERIFY(request.has_value());
    QCOMPARE(request->location.imageUrl(), queuedUrl);
    QVERIFY(!cache.takeNextRequest(kiriview::PredecodeActiveLoads {}).has_value());
}

void TestPredecodeCache::takeNextRequestDiscardsSkippedQueuePrefix()
{
    kiriview::PredecodeCache cache = defaultCache();
    const QUrl cachedQueuedUrl = indexedImageUrl(0);
    const QUrl firstRequestUrl = indexedImageUrl(1);
    const QUrl secondRequestUrl = indexedImageUrl(2);
    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();

    cache.setWindowLocations(displayedLocations(
        { cachedQueuedUrl, firstRequestUrl, secondRequestUrl }, openedCollectionScope));
    cache.enqueueMissingWindowLoads(displayedLocation(indexedImageUrl(9), openedCollectionScope),
        kiriview::PredecodeActiveLoads {});
    cache.cacheImage(
        displayedLocation(cachedQueuedUrl, openedCollectionScope), cacheDisplayImage(cacheImage()));

    const std::optional<kiriview::PredecodeRequest> firstRequest
        = cache.takeNextRequest(kiriview::PredecodeActiveLoads {});
    QVERIFY(firstRequest.has_value());
    QCOMPARE(firstRequest->location.imageUrl(), firstRequestUrl);

    const std::optional<kiriview::PredecodeRequest> secondRequest
        = cache.takeNextRequest(kiriview::PredecodeActiveLoads {});
    QVERIFY(secondRequest.has_value());
    QCOMPARE(secondRequest->location.imageUrl(), secondRequestUrl);
    QVERIFY(!cache.takeNextRequest(kiriview::PredecodeActiveLoads {}).has_value());
}

void TestPredecodeCache::cacheStoresAndFindsWindowImages()
{
    kiriview::PredecodeCache cache(80);
    const QUrl url = indexedImageUrl(0);
    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();
    const QImage image = cacheImage();

    const kiriview::DisplayedImageLocation location = displayedLocation(url, openedCollectionScope);
    cache.setWindowLocations({ location });
    kiriview::StaticDisplayImagePayload payload
        = staticDisplayTestImagePayload(image, image, kiriview::DisplayImageQuality::FirstDisplay,
            kiriview::StaticImageReaderTransform {
                QImageIOHandler::TransformationRotate90,
            });
    payload.sourceIdentity = QStringLiteral("file:///tmp/predecode-source.jpg");
    payload.embeddedMetadata.cameraMake = QStringLiteral("Kiri Camera");
    cache.cacheImage(location, std::move(payload));

    const std::optional<kiriview::PredecodedImage> found = cache.findImage(location);
    QVERIFY(found.has_value());
    QCOMPARE(found->displayImage.image.size(), image.size());
    QCOMPARE(found->displayImage.originalSize, image.size());
    QCOMPARE(
        found->displayImage.sourceIdentity, QStringLiteral("file:///tmp/predecode-source.jpg"));
    QCOMPARE(found->displayImage.quality, kiriview::DisplayImageQuality::FirstDisplay);
    QCOMPARE(found->displayImage.imageReaderTransform.transformations,
        QImageIOHandler::TransformationRotate90);
    QCOMPARE(found->displayImage.embeddedMetadata.cameraMake, QStringLiteral("Kiri Camera"));
    QCOMPARE(found->embeddedMetadata.cameraMake, QStringLiteral("Kiri Camera"));
    QCOMPARE(found->location.imageUrl(), url);
    QCOMPARE(found->location.openedCollectionScope().rootUrl(), openedCollectionScope.rootUrl());
    QVERIFY(found->displayImage.refinementSource != nullptr);
}

void TestPredecodeCache::cacheFindsImagesByUrlAndOpenedCollectionScope()
{
    kiriview::PredecodeCache cache(160);
    const QUrl url = indexedImageUrl(0);
    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();
    const kiriview::DisplayedImageLocation directLocation
        = kiriview::DisplayedImageLocation::fromUrl(url);
    const kiriview::DisplayedImageLocation openedCollectionLocation
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(url, openedCollectionScope);

    cache.setWindowLocations({ directLocation, openedCollectionLocation });
    cache.cacheImage(directLocation, cacheDisplayImage(cacheImage(), true));
    cache.cacheImage(openedCollectionLocation, cacheDisplayImage(cacheImage(), true));

    const std::optional<kiriview::PredecodedImage> direct = cache.findImage(directLocation);
    QVERIFY(direct.has_value());
    QVERIFY(direct->location == directLocation);

    const std::optional<kiriview::PredecodedImage> openedCollection
        = cache.findImage(openedCollectionLocation);
    QVERIFY(openedCollection.has_value());
    QVERIFY(openedCollection->location == openedCollectionLocation);
}

void TestPredecodeCache::queueAndActiveWorkDistinguishOpenedCollectionScope()
{
    kiriview::PredecodeCache cache(160);
    const QUrl url = indexedImageUrl(0);
    const kiriview::OpenedCollectionScopeLocation firstScope = comicBookArchiveCollection();
    const kiriview::OpenedCollectionScopeLocation secondScope
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            QUrl::fromLocalFile(QStringLiteral("/books/other.cbz")),
            QUrl(QStringLiteral("zip:///books/other.cbz/")),
            kiriview::OpenedCollectionScopeKind::ComicBookArchive);
    const kiriview::DisplayedImageLocation firstLocation
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(url, firstScope);
    const kiriview::DisplayedImageLocation secondLocation
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(url, secondScope);

    cache.setWindowLocations({ secondLocation });
    cache.cacheImage(firstLocation, cacheDisplayImage(cacheImage(), true));
    const kiriview::PredecodeActiveLoads firstScopeActive
        = kiriview::PredecodeActiveLoads::fromLocations({ firstLocation });
    cache.enqueueMissingWindowLoads(kiriview::DisplayedImageLocation {}, firstScopeActive);

    QVERIFY(cache.hasImage(firstLocation));
    QVERIFY(!cache.hasImage(secondLocation));
    QVERIFY(cache.isInFlight(firstLocation, firstScopeActive));
    QVERIFY(cache.isInFlight(secondLocation, firstScopeActive));
    const std::optional<kiriview::PredecodeRequest> request
        = cache.takeNextRequest(firstScopeActive);
    QVERIFY(request.has_value());
    QVERIFY(request->location == secondLocation);
}

void TestPredecodeCache::cacheRetainsDisplayedImagesBeforeAdjacentImages()
{
    kiriview::PredecodeCache cache(160);
    const QUrl primaryDisplayedUrl = indexedImageUrl(0);
    const QUrl secondaryDisplayedUrl = indexedImageUrl(1);
    const QUrl adjacentUrl = indexedImageUrl(2);
    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();
    const QImage image = cacheImage();

    const auto primaryLocation = displayedLocation(primaryDisplayedUrl, openedCollectionScope);
    const auto secondaryLocation = displayedLocation(secondaryDisplayedUrl, openedCollectionScope);
    const auto adjacentLocation = displayedLocation(adjacentUrl, openedCollectionScope);
    cache.setDisplayedLocations({ primaryLocation, secondaryLocation });
    cache.setWindowLocations({ primaryLocation, secondaryLocation, adjacentLocation });
    cache.cacheDisplayedImage(true, secondaryLocation, cacheDisplayImage(image));
    cache.cacheImage(adjacentLocation, cacheDisplayImage(image));
    cache.cacheDisplayedImage(true, primaryLocation, cacheDisplayImage(image));

    QVERIFY(cache.hasImage(primaryLocation));
    QVERIFY(cache.hasImage(secondaryLocation));
    QVERIFY(!cache.hasImage(adjacentLocation));
}

void TestPredecodeCache::cacheRetainsRecentDisplayedImagesBeforeAdjacentImages()
{
    kiriview::PredecodeCache cache(160);
    const QUrl recentDisplayedUrl = indexedImageUrl(0);
    const QUrl currentDisplayedUrl = indexedImageUrl(1);
    const QUrl adjacentUrl = indexedImageUrl(2);
    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();
    const QImage image = cacheImage();

    const auto recentLocation = displayedLocation(recentDisplayedUrl, openedCollectionScope);
    const auto currentLocation = displayedLocation(currentDisplayedUrl, openedCollectionScope);
    const auto adjacentLocation = displayedLocation(adjacentUrl, openedCollectionScope);
    cache.setDisplayedLocations({ recentLocation });
    cache.cacheDisplayedImage(true, recentLocation, cacheDisplayImage(image));
    cache.setDisplayedLocations({ currentLocation });
    cache.setWindowLocations({ currentLocation, adjacentLocation });
    cache.cacheImage(adjacentLocation, cacheDisplayImage(image));
    cache.cacheDisplayedImage(true, currentLocation, cacheDisplayImage(image));

    QVERIFY(cache.hasImage(currentLocation));
    QVERIFY(cache.hasImage(recentLocation));
    QVERIFY(!cache.hasImage(adjacentLocation));
}

void TestPredecodeCache::cacheKeepsOnlyFourRecentDisplayedImages()
{
    kiriview::PredecodeCache cache(400);
    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();
    const QImage image = cacheImage();

    for (int index = 0; index < 6; ++index) {
        const QUrl url = indexedImageUrl(index);
        const auto location = displayedLocation(url, openedCollectionScope);
        cache.setDisplayedLocations({ location });
        cache.cacheDisplayedImage(true, location, cacheDisplayImage(image));
    }

    QVERIFY(cache.hasImage(displayedLocation(indexedImageUrl(5), openedCollectionScope)));
    QVERIFY(cache.hasImage(displayedLocation(indexedImageUrl(4), openedCollectionScope)));
    QVERIFY(cache.hasImage(displayedLocation(indexedImageUrl(3), openedCollectionScope)));
    QVERIFY(cache.hasImage(displayedLocation(indexedImageUrl(2), openedCollectionScope)));
    QVERIFY(cache.hasImage(displayedLocation(indexedImageUrl(1), openedCollectionScope)));
    QVERIFY(!cache.hasImage(displayedLocation(indexedImageUrl(0), openedCollectionScope)));
}

void TestPredecodeCache::cacheRejectsUncacheableAndOversizedImages()
{
    kiriview::PredecodeCache cache(160);
    const QUrl url = indexedImageUrl(0);
    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();

    const auto location = displayedLocation(url, openedCollectionScope);
    cache.setWindowLocations({ location });
    const QImage image = cacheImage();
    cache.cacheDisplayedImage(false, location, cacheDisplayImage(image));
    QVERIFY(!cache.hasImage(location));

    cache.cacheDisplayedImage(true, location, kiriview::StaticDisplayImagePayload {});
    QVERIFY(!cache.hasImage(location));

    const QImage largeImage = tooLargeImage();
    cache.cacheImage(location, cacheDisplayImage(largeImage));
    QVERIFY(!cache.hasImage(location));
}

void TestPredecodeCache::cacheRejectsProvisionalPreviewPayloads()
{
    kiriview::PredecodeCache cache(160);
    const QUrl url = indexedImageUrl(0);
    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();
    const auto location = displayedLocation(url, openedCollectionScope);
    cache.setWindowLocations({ location });

    kiriview::StaticDisplayImagePayload thumbnail = cacheDisplayImage(cacheImage());
    thumbnail.quality = kiriview::DisplayImageQuality::ThumbnailPreview;
    thumbnail.previewOrigin = kiriview::DisplayImagePreviewOrigin::XdgThumbnail;
    cache.cacheImage(location, std::move(thumbnail));
    QVERIFY(!cache.hasImage(location));

    kiriview::StaticDisplayImagePayload previewOrigin = cacheDisplayImage(cacheImage(), true);
    previewOrigin.previewOrigin = kiriview::DisplayImagePreviewOrigin::RawEmbeddedThumbnail;
    cache.cacheDisplayedImage(true, location, std::move(previewOrigin));
    QVERIFY(!cache.hasImage(location));
}

void TestPredecodeCache::cacheEvictsLowestPriorityImagesWhenBudgetIsExceeded()
{
    kiriview::PredecodeCache cache(160);
    const QUrl firstUrl = indexedImageUrl(0);
    const QUrl secondUrl = indexedImageUrl(1);
    const QUrl thirdUrl = indexedImageUrl(2);
    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();

    const auto firstLocation = displayedLocation(firstUrl, openedCollectionScope);
    const auto secondLocation = displayedLocation(secondUrl, openedCollectionScope);
    const auto thirdLocation = displayedLocation(thirdUrl, openedCollectionScope);
    cache.setWindowLocations({ firstLocation, secondLocation, thirdLocation });
    const QImage image = cacheImage();
    cache.cacheImage(thirdLocation, cacheDisplayImage(image));
    cache.cacheImage(firstLocation, cacheDisplayImage(image));
    cache.cacheImage(secondLocation, cacheDisplayImage(image));

    QVERIFY(cache.hasImage(firstLocation));
    QVERIFY(cache.hasImage(secondLocation));
    QVERIFY(!cache.hasImage(thirdLocation));
}

void TestPredecodeCache::cacheRetainsWarmImagesAcrossWindowReprioritization()
{
    kiriview::PredecodeCache cache(480);
    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();
    std::vector<QUrl> urls;
    for (int index = 1; index <= 6; ++index) {
        urls.push_back(indexedImageUrl(index));
    }

    const auto locations = displayedLocations(urls, openedCollectionScope);
    cache.setWindowLocations(locations);
    for (const QUrl& url : urls) {
        cache.cacheImage(
            displayedLocation(url, openedCollectionScope), cacheDisplayImage(cacheImage()));
    }

    const std::vector<QUrl> smallerWindow(urls.begin(), urls.end() - 1);
    cache.setWindowLocations(displayedLocations(smallerWindow, openedCollectionScope));
    QVERIFY(cache.findImage(locations.back()).has_value());

    cache.setWindowLocations(locations);
    cache.enqueueMissingWindowLoads(locations.front(), kiriview::PredecodeActiveLoads {});
    while (const std::optional<kiriview::PredecodeRequest> request
        = cache.takeNextRequest(kiriview::PredecodeActiveLoads {})) {
        QVERIFY(request->location != locations.back());
    }

    cache.clear();
    QVERIFY(!cache.findImage(locations.back()).has_value());
}

void TestPredecodeCache::cacheRefreshesWarmImageRecencyOnLookup()
{
    kiriview::PredecodeCache cache(160);
    const QUrl firstWarmUrl = indexedImageUrl(1);
    const QUrl secondWarmUrl = indexedImageUrl(2);
    const QUrl windowUrl = indexedImageUrl(3);
    const kiriview::OpenedCollectionScopeLocation openedCollectionScope
        = comicBookArchiveCollection();

    const auto firstWarmLocation = displayedLocation(firstWarmUrl, openedCollectionScope);
    const auto secondWarmLocation = displayedLocation(secondWarmUrl, openedCollectionScope);
    const auto windowLocation = displayedLocation(windowUrl, openedCollectionScope);
    cache.setWindowLocations({ firstWarmLocation, secondWarmLocation });
    cache.cacheImage(firstWarmLocation, cacheDisplayImage(cacheImage()));
    cache.cacheImage(secondWarmLocation, cacheDisplayImage(cacheImage()));
    cache.setWindowLocations({});
    QVERIFY(cache.findImage(firstWarmLocation).has_value());

    cache.setWindowLocations({ windowLocation });
    cache.cacheImage(windowLocation, cacheDisplayImage(cacheImage()));

    QVERIFY(cache.hasImage(windowLocation));
    QVERIFY(cache.hasImage(firstWarmLocation));
    QVERIFY(!cache.hasImage(secondWarmLocation));
}

QTEST_GUILESS_MAIN(TestPredecodeCache)

#include "tst_predecodecache.moc"
