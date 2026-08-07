// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "thumbnail/thumbnailgeneration.h"

#include "decoding/imagedecodeworkspace.h"
#include "decoding/imagesourcedata.h"

#include <QBuffer>
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QMetaObject>
#include <QObject>
#include <QSize>
#include <QString>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <memory>
#include <optional>
#include <utility>

namespace {
using Bucket = kiriview::ActiveNavigationThumbnailDemandBucket;
using Status = kiriview::ThumbnailGenerationStatus;

void drainQueuedCalls()
{
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();
}

QByteArray encodedPngData()
{
    QImage image(QSize(4, 3), QImage::Format_RGBA8888);
    image.fill(QColor(Qt::green));

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, QByteArrayLiteral("png"));
    if (!writer.write(image)) {
        return {};
    }
    return data;
}

QByteArray fixtureData(const QString& fileName)
{
    QFile file(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR "/../fixtures/") + fileName);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray {};
}

kiriview::ThumbnailGenerationRequest generationRequest(Bucket bucket = Bucket::Normal)
{
    kiriview::ThumbnailGenerationRequest request;
    request.localPathBytes = QByteArrayLiteral("/missing/source.png");
    request.sourceUrl = QUrl::fromLocalFile(QStringLiteral("/missing/source.png"));
    request.sourceLabel = QStringLiteral("source.png");
    request.requestedBucket = bucket;
    request.cacheInstallEnabled = false;
    return request;
}

struct ImageCleanupObservation
{
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> budget;
    uchar* pixels = nullptr;
    bool cleanupCalled = false;
    bool reservationHeldDuringCleanup = false;

    ~ImageCleanupObservation() { delete[] pixels; }
};

void observeImageCleanup(void* context)
{
    auto* observation = static_cast<ImageCleanupObservation*>(context);
    observation->cleanupCalled = true;
    observation->reservationHeldDuringCleanup = observation->budget->reservedByteCount() > 0;
    delete[] observation->pixels;
    observation->pixels = nullptr;
}

}

class TestThumbnailGeneration : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void injectedBytesLoaderProvidesGenerationBytes();
    void injectedDecoderReceivesLoadedBytesAndBucketEdge();
    void injectedScalingPolicySuppliesDecoderEdge();
    void openedCollectionIdentityFailureSkipsBytesLoader();
    void injectedCacheHitSkipsBytesLoader();
    void injectedCacheInstallPublishesInstalledPath();
    void directVideoInvalidRequestPublishesFailureWithoutLoadingBytes();
    void directVideoResourceLimitPreservesTypedFailure();
    void defaultImageBytesLoaderRejectsSourceOverBudget();
    void defaultImageDecoderRejectsApngOverWorkspaceBudget();
    void generatedApngRetainsWorkspaceUntilResultRelease();
    void failedApngGenerationDestroysImageBeforeWorkspaceRelease();
};

void TestThumbnailGeneration::injectedBytesLoaderProvidesGenerationBytes()
{
    kiriview::ThumbnailGenerationDependencies dependencies;
    int bytesLoadCount = 0;
    dependencies.bytesLoader
        = [&bytesLoadCount](const kiriview::ThumbnailGenerationRequest&, QString*) {
              ++bytesLoadCount;
              return encodedPngData();
          };

    const kiriview::ThumbnailGenerationResult result
        = kiriview::generateThumbnail(generationRequest(), std::move(dependencies));

    QCOMPARE(bytesLoadCount, 1);
    QCOMPARE(result.status, Status::Ready);
    QCOMPARE(result.requestedBucket, Bucket::Normal);
    QCOMPARE(result.image.size(), QSize(4, 3));
    QCOMPARE(result.image.format(), QImage::Format_RGBA8888);
    QVERIFY(result.installedCachePath.isEmpty());
}

void TestThumbnailGeneration::defaultImageBytesLoaderRejectsSourceOverBudget()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("oversized.png"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(QByteArray(32, 'x')), qint64(32));
    file.close();

    auto budget = std::make_shared<kiriview::ImageSourceDataBudget>(16, 16);
    kiriview::ThumbnailGenerationDependencies dependencies;
    dependencies.sourceDataBudget = budget;
    int decodeCount = 0;
    dependencies.imageDecoder = [&decodeCount](QByteArray, int) {
        ++decodeCount;
        return kiriview::ThumbnailGenerationImageDecodeResult {
            {},
            QImage(QSize(1, 1), QImage::Format_RGBA8888),
        };
    };
    kiriview::ThumbnailGenerationRequest request = generationRequest();
    request.localPathBytes = QFile::encodeName(path);
    request.sourceUrl = QUrl::fromLocalFile(path);

    const kiriview::ThumbnailGenerationResult result
        = kiriview::generateThumbnail(std::move(request), std::move(dependencies));

    QCOMPARE(result.status, Status::Failed);
    QVERIFY(!result.errorString.isEmpty());
    QCOMPARE(decodeCount, 0);
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestThumbnailGeneration::defaultImageDecoderRejectsApngOverWorkspaceBudget()
{
    const QByteArray apng = fixtureData(QStringLiteral("animated-smoke.apng"));
    QVERIFY(!apng.isEmpty());
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(1, 1);
    kiriview::ThumbnailGenerationDependencies dependencies;
    dependencies.bytesLoader = [apng](const kiriview::ThumbnailGenerationRequest&, QString*) {
        return kiriview::ImageSourceData(apng);
    };
    dependencies.workspaceBudget = budget;

    const kiriview::ThumbnailGenerationResult result
        = kiriview::generateThumbnail(generationRequest(), std::move(dependencies));

    QCOMPARE(result.status, kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded);
    QVERIFY(!result.errorString.isEmpty());
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestThumbnailGeneration::generatedApngRetainsWorkspaceUntilResultRelease()
{
    const QByteArray apng = fixtureData(QStringLiteral("animated-smoke.apng"));
    QVERIFY(!apng.isEmpty());
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        256 * 1024 * 1024, 256 * 1024 * 1024);
    kiriview::ThumbnailGenerationDependencies dependencies;
    dependencies.bytesLoader = [apng](const kiriview::ThumbnailGenerationRequest&, QString*) {
        return kiriview::ImageSourceData(apng);
    };
    dependencies.workspaceBudget = budget;

    {
        const kiriview::ThumbnailGenerationResult result
            = kiriview::generateThumbnail(generationRequest(), dependencies);
        QCOMPARE(result.status, kiriview::ThumbnailGenerationStatus::Ready);
        QVERIFY(!result.image.isNull());
        QVERIFY(budget->reservedByteCount() > 0);
    }

    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestThumbnailGeneration::failedApngGenerationDestroysImageBeforeWorkspaceRelease()
{
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(16, 16);
    ImageCleanupObservation observation { budget, new uchar[4] { 0, 255, 0, 255 } };
    kiriview::ThumbnailGenerationDependencies dependencies;
    dependencies.workspaceBudget = budget;
    dependencies.bytesLoader = [](const kiriview::ThumbnailGenerationRequest&, QString*) {
        return kiriview::ImageSourceData(QByteArrayLiteral("synthetic"));
    };
    dependencies.imageDecoder = [budget, &observation](QByteArray, int) {
        kiriview::ImageDecodeWorkspaceLease lease = budget->startLease();
        if (!lease.tryReserve(4)) {
            return kiriview::ThumbnailGenerationImageDecodeResult {};
        }
        QImage image(observation.pixels, 1, 1, 4, QImage::Format_RGBA8888, observeImageCleanup,
            &observation);
        return kiriview::ThumbnailGenerationImageDecodeResult {
            { lease.retainOnly(4), {} },
            std::move(image),
        };
    };
    dependencies.cacheRepository.install
        = [](const kiriview::ThumbnailOriginalIdentity&, Bucket bucket, const QImage&) {
              return kiriview::ThumbnailGenerationCacheInstallResult {
                  false,
                  bucket,
                  {},
                  QStringLiteral("cache install failed"),
              };
          };
    kiriview::ThumbnailGenerationRequest request = generationRequest();
    request.cacheInstallEnabled = true;

    {
        const kiriview::ThumbnailGenerationResult result
            = kiriview::generateThumbnail(request, std::move(dependencies));
        QCOMPARE(result.status, Status::Failed);
        QVERIFY(observation.cleanupCalled);
        QVERIFY(observation.reservationHeldDuringCleanup);
        QCOMPARE(budget->reservedByteCount(), qsizetype(4));
    }

    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestThumbnailGeneration::injectedDecoderReceivesLoadedBytesAndBucketEdge()
{
    const QByteArray bytes("synthetic bytes");
    QByteArray decodedBytes;
    int decodedMaximumLongEdge = 0;

    kiriview::ThumbnailGenerationDependencies dependencies;
    dependencies.bytesLoader
        = [&bytes](const kiriview::ThumbnailGenerationRequest&, QString*) { return bytes; };
    dependencies.imageDecoder
        = [&decodedBytes, &decodedMaximumLongEdge](QByteArray data, int maximumLongEdge) {
              decodedBytes = std::move(data);
              decodedMaximumLongEdge = maximumLongEdge;
              QImage image(QSize(9, 7), QImage::Format_RGB32);
              image.fill(QColor(Qt::yellow));
              return kiriview::ThumbnailGenerationImageDecodeResult { {}, std::move(image) };
          };

    const kiriview::ThumbnailGenerationResult result
        = kiriview::generateThumbnail(generationRequest(Bucket::Large), std::move(dependencies));

    QCOMPARE(decodedBytes, bytes);
    QCOMPARE(decodedMaximumLongEdge, 256);
    QCOMPARE(result.status, Status::Ready);
    QCOMPARE(result.requestedBucket, Bucket::Large);
    QCOMPARE(result.image.size(), QSize(9, 7));
    QCOMPARE(result.image.format(), QImage::Format_RGBA8888);
}

void TestThumbnailGeneration::injectedScalingPolicySuppliesDecoderEdge()
{
    const QByteArray bytes("synthetic bytes");
    Bucket policyBucket = Bucket::None;
    int decodedMaximumLongEdge = 0;

    kiriview::ThumbnailGenerationDependencies dependencies;
    dependencies.bytesLoader
        = [&bytes](const kiriview::ThumbnailGenerationRequest&, QString*) { return bytes; };
    dependencies.maximumLongEdgeForBucket = [&policyBucket](Bucket bucket) {
        policyBucket = bucket;
        return 777;
    };
    dependencies.imageDecoder = [&decodedMaximumLongEdge](QByteArray, int maximumLongEdge) {
        decodedMaximumLongEdge = maximumLongEdge;
        QImage image(QSize(6, 5), QImage::Format_RGB32);
        image.fill(QColor(Qt::cyan));
        return kiriview::ThumbnailGenerationImageDecodeResult { {}, std::move(image) };
    };

    const kiriview::ThumbnailGenerationResult result
        = kiriview::generateThumbnail(generationRequest(Bucket::XLarge), std::move(dependencies));

    QCOMPARE(policyBucket, Bucket::XLarge);
    QCOMPARE(decodedMaximumLongEdge, 777);
    QCOMPARE(result.status, Status::Ready);
    QCOMPARE(result.requestedBucket, Bucket::XLarge);
}

void TestThumbnailGeneration::openedCollectionIdentityFailureSkipsBytesLoader()
{
    kiriview::ThumbnailGenerationRequest request = generationRequest();
    request.openedCollectionScope = kiriview::OpenedCollectionScopeLocation::fromUrls(
        QUrl::fromLocalFile(QStringLiteral("/books/book.cbz")),
        QUrl(QStringLiteral("zip:///books/book.cbz")),
        kiriview::OpenedCollectionScopeKind::ComicBookArchive);
    request.sourceUrl = QUrl(QStringLiteral("zip:///books/book.cbz/page.png"));
    request.cacheInstallEnabled = true;

    kiriview::ThumbnailGenerationDependencies dependencies;
    int bytesLoadCount = 0;
    dependencies.bytesLoader
        = [&bytesLoadCount](const kiriview::ThumbnailGenerationRequest&, QString*) {
              ++bytesLoadCount;
              return encodedPngData();
          };
    dependencies.openedCollectionOriginalIdentityLoader
        = [](const kiriview::ThumbnailGenerationRequest&, QString* errorString) {
              if (errorString != nullptr) {
                  *errorString = QStringLiteral("synthetic identity failure");
              }
              return std::optional<kiriview::ThumbnailOriginalIdentity>();
          };

    const kiriview::ThumbnailGenerationResult result
        = kiriview::generateThumbnail(std::move(request), std::move(dependencies));

    QCOMPARE(bytesLoadCount, 0);
    QCOMPARE(result.status, Status::Failed);
    QCOMPARE(result.requestedBucket, Bucket::Normal);
    QCOMPARE(result.errorString, QStringLiteral("synthetic identity failure"));
}

void TestThumbnailGeneration::injectedCacheHitSkipsBytesLoader()
{
    kiriview::ThumbnailGenerationRequest request = generationRequest();
    request.openedCollectionScope = kiriview::OpenedCollectionScopeLocation::fromUrls(
        QUrl::fromLocalFile(QStringLiteral("/books/book.cbz")),
        QUrl(QStringLiteral("zip:///books/book.cbz")),
        kiriview::OpenedCollectionScopeKind::ComicBookArchive);
    request.sourceUrl = QUrl(QStringLiteral("zip:///books/book.cbz/page.png"));
    request.cacheInstallEnabled = true;

    QImage cachedImage(QSize(16, 12), QImage::Format_RGBA8888);
    cachedImage.fill(QColor(Qt::blue));

    kiriview::ThumbnailGenerationDependencies dependencies;
    int bytesLoadCount = 0;
    dependencies.bytesLoader
        = [&bytesLoadCount](const kiriview::ThumbnailGenerationRequest&, QString*) {
              ++bytesLoadCount;
              return encodedPngData();
          };
    dependencies.openedCollectionOriginalIdentityLoader
        = [](const kiriview::ThumbnailGenerationRequest&, QString*) {
              return std::optional<kiriview::ThumbnailOriginalIdentity>(
                  kiriview::ThumbnailOriginalIdentity::fromNonFileUri(
                      QStringLiteral("x-kiriview://thumbnail/test"), 0, 8, QString()));
          };
    dependencies.cacheRepository.lookup
        = [cachedImage](const kiriview::ThumbnailOriginalIdentity&, Bucket bucket) {
              return std::optional<kiriview::ThumbnailCacheLookupResult>(
                  kiriview::ThumbnailCacheLookupResult {
                      kiriview::ThumbnailCacheLookupStatus::Ready,
                      cachedImage,
                      bucket,
                      bucket,
                      QStringLiteral("/cache/hit.png"),
                      {},
                  });
          };

    const kiriview::ThumbnailGenerationResult result
        = kiriview::generateThumbnail(std::move(request), std::move(dependencies));

    QCOMPARE(bytesLoadCount, 0);
    QCOMPARE(result.status, Status::Ready);
    QCOMPARE(result.requestedBucket, Bucket::Normal);
    QCOMPARE(result.image.size(), cachedImage.size());
    QCOMPARE(result.installedCachePath, QStringLiteral("/cache/hit.png"));
}

void TestThumbnailGeneration::injectedCacheInstallPublishesInstalledPath()
{
    kiriview::ThumbnailGenerationRequest request = generationRequest(Bucket::Large);
    request.cacheInstallEnabled = true;

    kiriview::ThumbnailOriginalIdentity installedIdentity;
    QImage installedImage;

    kiriview::ThumbnailGenerationDependencies dependencies;
    dependencies.bytesLoader
        = [](const kiriview::ThumbnailGenerationRequest&, QString*) { return encodedPngData(); };
    dependencies.cacheRepository.install
        = [&installedIdentity, &installedImage](const kiriview::ThumbnailOriginalIdentity& identity,
              Bucket bucket, const QImage& image) {
              installedIdentity = identity;
              installedImage = image.copy();
              return kiriview::ThumbnailGenerationCacheInstallResult {
                  true,
                  bucket,
                  QStringLiteral("/cache/generated.png"),
                  {},
              };
          };

    const kiriview::ThumbnailGenerationResult result
        = kiriview::generateThumbnail(std::move(request), std::move(dependencies));

    QCOMPARE(result.status, Status::Ready);
    QCOMPARE(result.requestedBucket, Bucket::Large);
    QCOMPARE(result.installedCachePath, QStringLiteral("/cache/generated.png"));
    QCOMPARE(installedIdentity.mode, kiriview::ThumbnailOriginalIdentityMode::LocalPath);
    QCOMPARE(installedIdentity.localPathBytes, QByteArrayLiteral("/missing/source.png"));
    QCOMPARE(installedImage.size(), QSize(4, 3));
    QCOMPARE(installedImage.format(), QImage::Format_RGBA8888);
}

void TestThumbnailGeneration::directVideoInvalidRequestPublishesFailureWithoutLoadingBytes()
{
    QObject owner;
    kiriview::ThumbnailGenerationRequest request = generationRequest(Bucket::Large);
    request.localPathBytes = QByteArrayLiteral("/media/clip.mp4");
    request.sourceUrl = {};
    request.sourceLabel = QStringLiteral("clip.mp4");
    request.sourceKind = kiriview::ThumbnailSourceKind::DirectVideo;
    request.cacheInstallEnabled = true;

    int bytesLoadCount = 0;
    int cacheInstallCount = 0;
    kiriview::ThumbnailGenerationDependencies dependencies;
    dependencies.bytesLoader
        = [&bytesLoadCount](const kiriview::ThumbnailGenerationRequest&, QString*) {
              ++bytesLoadCount;
              return encodedPngData();
          };
    dependencies.cacheRepository.install
        = [&cacheInstallCount](const kiriview::ThumbnailOriginalIdentity&, Bucket, const QImage&) {
              ++cacheInstallCount;
              return kiriview::ThumbnailGenerationCacheInstallResult {};
          };

    kiriview::ThumbnailGenerationResult delivered;
    bool deliveredResult = false;
    kiriview::ThumbnailGenerationProvider provider
        = kiriview::defaultThumbnailGenerationProvider({}, std::move(dependencies));
    kiriview::ImageIoJob job = provider(&owner, std::move(request),
        [&delivered, &deliveredResult](kiriview::ThumbnailGenerationResult result) {
            delivered = std::move(result);
            deliveredResult = true;
        });

    QCOMPARE(bytesLoadCount, 0);
    QVERIFY(job.isActive());

    drainQueuedCalls();

    QVERIFY(deliveredResult);
    QVERIFY(!job.isActive());
    QCOMPARE(delivered.status, Status::Failed);
    QCOMPARE(delivered.requestedBucket, Bucket::Large);
    QVERIFY(!delivered.errorString.isEmpty());
    QCOMPARE(cacheInstallCount, 0);
}

void TestThumbnailGeneration::directVideoResourceLimitPreservesTypedFailure()
{
    QObject owner;
    kiriview::ThumbnailGenerationRequest request = generationRequest(Bucket::Large);
    request.localPathBytes = QByteArrayLiteral("/media/clip.mp4");
    request.sourceUrl = QUrl(QStringLiteral("https://example.invalid/media/clip.mp4"));
    request.sourceLabel = QStringLiteral("clip.mp4");
    request.sourceKind = kiriview::ThumbnailSourceKind::DirectVideo;
    request.cacheInstallEnabled = true;

    constexpr int requestedMaximumLongEdge = 913;
    int bytesLoadCount = 0;
    int cacheInstallCount = 0;
    std::optional<kiriview::VideoThumbnailExtractionRequest> deliveredExtractionRequest;
    kiriview::ThumbnailGenerationDependencies dependencies;
    dependencies.maximumLongEdgeForBucket = [](Bucket) { return requestedMaximumLongEdge; };
    dependencies.bytesLoader
        = [&bytesLoadCount](const kiriview::ThumbnailGenerationRequest&, QString*) {
              ++bytesLoadCount;
              return encodedPngData();
          };
    dependencies.cacheRepository.install
        = [&cacheInstallCount](const kiriview::ThumbnailOriginalIdentity&, Bucket, const QImage&) {
              ++cacheInstallCount;
              return kiriview::ThumbnailGenerationCacheInstallResult {};
          };
    dependencies.videoExtractionProvider
        = [&deliveredExtractionRequest](QObject* receiver,
              kiriview::VideoThumbnailExtractionRequest extractionRequest,
              kiriview::VideoThumbnailExtractionCallback callback) {
              deliveredExtractionRequest = std::move(extractionRequest);
              auto* token = new QObject(receiver);
              kiriview::ImageIoJob job(token, [](QObject* object) { object->deleteLater(); });
              const kiriview::ImageIoJobCompletion completion = job.completion();
              const bool queued = QMetaObject::invokeMethod(
                  token,
                  [completion, callback = std::move(callback)]() mutable {
                      kiriview::VideoThumbnailExtractionResult result;
                      result.failure = kiriview::VideoThumbnailExtractionFailure {
                          kiriview::VideoThumbnailExtractionFailureCause::ResourceLimit,
                          QStringLiteral("provider diagnostic unrelated to the typed cause"),
                      };
                      completion.claimAndDelete([&callback, result = std::move(result)]() mutable {
                          callback(std::move(result));
                      });
                  },
                  Qt::QueuedConnection);
              Q_ASSERT(queued);
              return job;
          };

    kiriview::ThumbnailGenerationResult delivered;
    bool deliveredResult = false;
    kiriview::ThumbnailGenerationProvider provider
        = kiriview::defaultThumbnailGenerationProvider({}, std::move(dependencies));
    kiriview::ImageIoJob job = provider(&owner, request,
        [&delivered, &deliveredResult](kiriview::ThumbnailGenerationResult result) {
            delivered = std::move(result);
            deliveredResult = true;
        });

    QVERIFY(job.isActive());
    QVERIFY(deliveredExtractionRequest.has_value());
    QCOMPARE(deliveredExtractionRequest->sourceUrl, request.sourceUrl);
    QCOMPARE(deliveredExtractionRequest->maximumLongEdge, requestedMaximumLongEdge);
    QCOMPARE(bytesLoadCount, 0);

    drainQueuedCalls();

    QVERIFY(deliveredResult);
    QVERIFY(!job.isActive());
    QCOMPARE(delivered.status, Status::ResourceLimitExceeded);
    QCOMPARE(delivered.requestedBucket, Bucket::Large);
    QVERIFY(!delivered.errorString.isEmpty());
    QCOMPARE(cacheInstallCount, 0);
}

QTEST_GUILESS_MAIN(TestThumbnailGeneration)

#include "tst_thumbnailgeneration.moc"
