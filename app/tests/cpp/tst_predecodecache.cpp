// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "cache/imagecachepolicy.h"
#include "image_test_support.h"
#include "predecode/predecodecache.h"

#include <QByteArrayView>
#include <QColor>
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

kiriview::ImageSourceRevision testSourceRevision()
{
    return kiriview::ImageSourceRevision::fromData(QByteArrayView("image-test-support"));
}

kiriview::PredecodeImageKey predecodeKey(const kiriview::DisplayedImageLocation& location)
{
    return { location, testSourceRevision() };
}

std::vector<kiriview::PredecodeImageKey> predecodeKeys(
    const std::vector<kiriview::DisplayedImageLocation>& locations)
{
    std::vector<kiriview::PredecodeImageKey> keys;
    keys.reserve(locations.size());
    for (const kiriview::DisplayedImageLocation& location : locations) {
        keys.push_back(predecodeKey(location));
    }
    return keys;
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

kiriview::PredecodeActiveLoads activeLoads(
    std::vector<kiriview::DisplayedImageLocation> locations, quint64 lifecycleScope = 7)
{
    std::vector<kiriview::PredecodeWorkKey> keys;
    keys.reserve(locations.size());
    for (const kiriview::DisplayedImageLocation& location : locations) {
        keys.push_back(kiriview::PredecodeWorkKey {
            kiriview::PredecodeImageKey { location, {} }, lifecycleScope });
    }
    return kiriview::PredecodeActiveLoads::fromWorkKeys(keys);
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
    void cacheReusesOnlyMatchingSourceRevision();
    void queueAndActiveWorkDistinguishSourceRevision();
    void activeLoadsRejectUnscopedUnknownIdentity();
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
    cache.enqueueMissingWindowLoads(displayedLocation(displayedUrl, openedCollectionScope),
        kiriview::PredecodeActiveLoads {}, 7);

    QVERIFY(cache.isInFlight(
        kiriview::PredecodeWorkKey {
            kiriview::PredecodeImageKey {
                displayedLocation(secondQueuedUrl, openedCollectionScope), {} },
            7,
        },
        kiriview::PredecodeActiveLoads {}));

    const std::optional<kiriview::PredecodeRequest> firstRequest = cache.takeNextRequest(
        activeLoads({ displayedLocation(indexedImageUrl(9), openedCollectionScope) }));
    QVERIFY(firstRequest.has_value());
    QCOMPARE(firstRequest->location.imageUrl(), firstQueuedUrl);

    const std::optional<kiriview::PredecodeRequest> secondRequest
        = cache.takeNextRequest(kiriview::PredecodeActiveLoads {});
    QVERIFY(secondRequest.has_value());
    QCOMPARE(secondRequest->location.imageUrl(), secondQueuedUrl);
    QCOMPARE(
        secondRequest->location.openedCollectionScope().rootUrl(), openedCollectionScope.rootUrl());
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

    cache.setWindowKeys(predecodeKeys(displayedLocations(
        { cachedQueuedUrl, firstRequestUrl, secondRequestUrl }, openedCollectionScope)));
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

    const std::optional<kiriview::PredecodedImage> found = cache.findCandidate(location);
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

    const std::optional<kiriview::PredecodedImage> direct = cache.findCandidate(directLocation);
    QVERIFY(direct.has_value());
    QVERIFY(direct->location == directLocation);

    const std::optional<kiriview::PredecodedImage> openedCollection
        = cache.findCandidate(openedCollectionLocation);
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
    const kiriview::PredecodeActiveLoads firstScopeActive = activeLoads({ firstLocation });
    cache.enqueueMissingWindowLoads(kiriview::DisplayedImageLocation {}, firstScopeActive, 7);

    QVERIFY(cache.findCandidate(firstLocation).has_value());
    QVERIFY(!cache.findCandidate(secondLocation).has_value());
    QVERIFY(cache.isInFlight(
        kiriview::PredecodeWorkKey { kiriview::PredecodeImageKey { firstLocation, {} }, 7 },
        firstScopeActive));
    QVERIFY(cache.isInFlight(
        kiriview::PredecodeWorkKey { kiriview::PredecodeImageKey { secondLocation, {} }, 7 },
        firstScopeActive));
    const std::optional<kiriview::PredecodeRequest> request
        = cache.takeNextRequest(firstScopeActive);
    QVERIFY(request.has_value());
    QVERIFY(request->location == secondLocation);
}

void TestPredecodeCache::cacheReusesOnlyMatchingSourceRevision()
{
    kiriview::PredecodeCache cache(160);
    const kiriview::DisplayedImageLocation location
        = displayedLocation(indexedImageUrl(0), comicBookArchiveCollection());
    const kiriview::ImageSourceRevision firstRevision
        = kiriview::ImageSourceRevision::fromData(QByteArrayView("first"));
    const kiriview::ImageSourceRevision secondRevision
        = kiriview::ImageSourceRevision::fromData(QByteArrayView("second"));

    QImage firstImage = cacheImage();
    firstImage.fill(Qt::red);
    kiriview::StaticDisplayImagePayload firstPayload = cacheDisplayImage(firstImage);
    firstPayload.sourceRevision = firstRevision;
    cache.cacheImage(location, std::move(firstPayload));

    const std::optional<kiriview::PredecodedImage> reused
        = cache.findImage(kiriview::PredecodeImageKey { location, firstRevision });
    QVERIFY(reused.has_value());
    QCOMPARE(reused->displayImage.image.pixelColor(0, 0), QColor(Qt::red));
    QVERIFY(!cache.findImage(kiriview::PredecodeImageKey { location, secondRevision }).has_value());

    QImage secondImage = cacheImage();
    secondImage.fill(Qt::blue);
    kiriview::StaticDisplayImagePayload secondPayload = cacheDisplayImage(secondImage);
    secondPayload.sourceRevision = secondRevision;
    cache.cacheImage(location, std::move(secondPayload));

    const std::optional<kiriview::PredecodedImage> refreshed
        = cache.findImage(kiriview::PredecodeImageKey { location, secondRevision });
    QVERIFY(refreshed.has_value());
    QCOMPARE(refreshed->displayImage.image.pixelColor(0, 0), QColor(Qt::blue));
}

void TestPredecodeCache::queueAndActiveWorkDistinguishSourceRevision()
{
    kiriview::PredecodeCache cache(160);
    const kiriview::DisplayedImageLocation location
        = displayedLocation(indexedImageUrl(0), comicBookArchiveCollection());
    const kiriview::PredecodeImageKey older {
        location,
        kiriview::ImageSourceRevision::fromData(QByteArrayView("older")),
    };
    const kiriview::PredecodeImageKey newer {
        location,
        kiriview::ImageSourceRevision::fromData(QByteArrayView("newer")),
    };
    const kiriview::PredecodeActiveLoads active
        = kiriview::PredecodeActiveLoads::fromKeys({ older });

    cache.setWindowKeys({ newer });
    cache.enqueueMissingWindowLoads(kiriview::DisplayedImageLocation {}, active);

    const std::optional<kiriview::PredecodeRequest> request = cache.takeNextRequest(active);
    QVERIFY(request.has_value());
    QVERIFY(request->key() == newer);
}

void TestPredecodeCache::activeLoadsRejectUnscopedUnknownIdentity()
{
    const kiriview::DisplayedImageLocation location
        = displayedLocation(indexedImageUrl(0), comicBookArchiveCollection());

    QCOMPARE(
        kiriview::PredecodeActiveLoads::fromKeys({ kiriview::PredecodeImageKey { location, {} } })
            .size(),
        std::size_t(0));
    QCOMPARE(kiriview::PredecodeActiveLoads::fromWorkKeys(
                 { kiriview::PredecodeWorkKey {
                     kiriview::PredecodeImageKey { location, {} },
                     0,
                 } })
                 .size(),
        std::size_t(0));
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

    QVERIFY(cache.findCandidate(primaryLocation).has_value());
    QVERIFY(cache.findCandidate(secondaryLocation).has_value());
    QVERIFY(!cache.findCandidate(adjacentLocation).has_value());
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

    QVERIFY(cache.findCandidate(currentLocation).has_value());
    QVERIFY(cache.findCandidate(recentLocation).has_value());
    QVERIFY(!cache.findCandidate(adjacentLocation).has_value());
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

    QVERIFY(cache.findCandidate(displayedLocation(indexedImageUrl(5), openedCollectionScope))
            .has_value());
    QVERIFY(cache.findCandidate(displayedLocation(indexedImageUrl(4), openedCollectionScope))
            .has_value());
    QVERIFY(cache.findCandidate(displayedLocation(indexedImageUrl(3), openedCollectionScope))
            .has_value());
    QVERIFY(cache.findCandidate(displayedLocation(indexedImageUrl(2), openedCollectionScope))
            .has_value());
    QVERIFY(cache.findCandidate(displayedLocation(indexedImageUrl(1), openedCollectionScope))
            .has_value());
    QVERIFY(!cache.findCandidate(displayedLocation(indexedImageUrl(0), openedCollectionScope))
            .has_value());
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
    QVERIFY(!cache.findCandidate(location).has_value());

    cache.cacheDisplayedImage(true, location, kiriview::StaticDisplayImagePayload {});
    QVERIFY(!cache.findCandidate(location).has_value());

    const QImage largeImage = tooLargeImage();
    cache.cacheImage(location, cacheDisplayImage(largeImage));
    QVERIFY(!cache.findCandidate(location).has_value());
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
    QVERIFY(!cache.findCandidate(location).has_value());

    kiriview::StaticDisplayImagePayload previewOrigin = cacheDisplayImage(cacheImage(), true);
    previewOrigin.previewOrigin = kiriview::DisplayImagePreviewOrigin::RawEmbeddedThumbnail;
    cache.cacheDisplayedImage(true, location, std::move(previewOrigin));
    QVERIFY(!cache.findCandidate(location).has_value());
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

    QVERIFY(cache.findCandidate(firstLocation).has_value());
    QVERIFY(cache.findCandidate(secondLocation).has_value());
    QVERIFY(!cache.findCandidate(thirdLocation).has_value());
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
    cache.setWindowKeys(predecodeKeys(locations));
    for (const QUrl& url : urls) {
        cache.cacheImage(
            displayedLocation(url, openedCollectionScope), cacheDisplayImage(cacheImage()));
    }

    const std::vector<QUrl> smallerWindow(urls.begin(), urls.end() - 1);
    cache.setWindowKeys(predecodeKeys(displayedLocations(smallerWindow, openedCollectionScope)));
    QVERIFY(cache.findCandidate(locations.back()).has_value());

    cache.setWindowKeys(predecodeKeys(locations));
    cache.enqueueMissingWindowLoads(locations.front(), kiriview::PredecodeActiveLoads {});
    while (const std::optional<kiriview::PredecodeRequest> request
        = cache.takeNextRequest(kiriview::PredecodeActiveLoads {})) {
        QVERIFY(request->location != locations.back());
    }

    cache.clear();
    QVERIFY(!cache.findCandidate(locations.back()).has_value());
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
    QVERIFY(cache.findCandidate(firstWarmLocation).has_value());

    cache.setWindowLocations({ windowLocation });
    cache.cacheImage(windowLocation, cacheDisplayImage(cacheImage()));

    QVERIFY(cache.findCandidate(windowLocation).has_value());
    QVERIFY(cache.findCandidate(firstWarmLocation).has_value());
    QVERIFY(!cache.findCandidate(secondWarmLocation).has_value());
}

QTEST_GUILESS_MAIN(TestPredecodeCache)

#include "tst_predecodecache.moc"
