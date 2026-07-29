// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imagesourcerevision.h"
#include "decoding/kiriimagedecoder.h"
#include "image_test_support.h"
#include "location/imageurl.h"
#include "location/sourcekey.h"
#include "rendering/imageviewportdecodesource.h"
#include "rendering/imageviewportsequenceprovider.h"
#include "rendering/svgdisplaysource.h"

#include <ImageViewport/imagesequence.h>
#include <ImageViewport/imageviewport.h>

#include <QBuffer>
#include <QFile>
#include <QImageWriter>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace kiriview {
class ImageViewportDecodeProviderSourceTestAccess
{
public:
    static void setNextWorkerUnitId(
        ImageViewportDecodeProviderSource& source, quint64 nextWorkerUnitId)
    {
        source.m_nextWorkerUnitId = nextWorkerUnitId;
    }
};
}

namespace {
const QByteArray& authoritativeSeedData()
{
    static const QByteArray data = QByteArrayLiteral("authoritative-seed-data");
    return data;
}

kiriview::StaticDisplayImagePayload authoritativeSeedForUrl(
    const QUrl& url, kiriview::StaticDisplayImagePayload payload)
{
    payload.sourceIdentity = kiriview::sourceKeyForUrl(url).identity;
    payload.sourceRevision = kiriview::ImageSourceRevision::fromData(authoritativeSeedData());
    return payload;
}

kiriview::StaticDisplayImagePayload displayPayload(kiriview::DisplayImageQuality quality,
    QSize rasterSize = {}, QSize originalSize = QSize(128, 64),
    QColor color = QColor(20, 40, 60, 255))
{
    if (rasterSize.isEmpty()) {
        rasterSize = quality == kiriview::DisplayImageQuality::Exact ? originalSize : QSize(32, 16);
    }
    QImage image(rasterSize, QImage::Format_RGBA8888);
    image.fill(color);
    return kiriview::StaticDisplayImagePayload {
        QStringLiteral("fake-source"),
        {},
        originalSize,
        std::move(image),
        quality,
        {},
        {},
        quality == kiriview::DisplayImageQuality::ThumbnailPreview
            ? kiriview::DisplayImagePreviewOrigin::XdgThumbnail
            : kiriview::DisplayImagePreviewOrigin::None,
        kiriview::StaticImageSourceDetailModel::FiniteRaster,
        kiriview::ImageSourceRevision::fromData(QByteArrayView("fake-content")),
        quality == kiriview::DisplayImageQuality::ThumbnailPreview
            ? kiriview::DisplayImageRasterKind::ProvisionalPreview
            : kiriview::DisplayImageRasterKind::AuthoritativeStill,
    };
}

kiriview::ImageLoadFailure loadFailure(quint64 sessionId = 17)
{
    return kiriview::ImageLoadFailure {
        QUrl(QStringLiteral("file:///tmp/failure.png")),
        sessionId,
        kiriview::ImageLoadFailureKind::Decode,
        kiriview::DecodedImageFailureRoute::QtRaster,
        kiriview::DecodedImageFailureOperation::DecodeBlockingDisplayImage,
        QStringLiteral("Could not decode the image"),
        QStringLiteral("fake decoder rejected the payload"),
        kiriview::ImageLoadFailureSeverity::Error,
        false,
    };
}

QByteArray encodedPngData(const QSize& size)
{
    QImage image(size, QImage::Format_RGBA8888);
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

QByteArray animationFixtureData(const QString& fileName)
{
    QFile file(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR "/../fixtures/") + fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

quint32 readBigEndian32(const QByteArray& data, qsizetype offset)
{
    return (static_cast<quint32>(static_cast<unsigned char>(data[offset])) << 24)
        | (static_cast<quint32>(static_cast<unsigned char>(data[offset + 1])) << 16)
        | (static_cast<quint32>(static_cast<unsigned char>(data[offset + 2])) << 8)
        | static_cast<quint32>(static_cast<unsigned char>(data[offset + 3]));
}

void writeBigEndian32(QByteArray* data, qsizetype offset, quint32 value)
{
    (*data)[offset] = static_cast<char>((value >> 24) & 0xff);
    (*data)[offset + 1] = static_cast<char>((value >> 16) & 0xff);
    (*data)[offset + 2] = static_cast<char>((value >> 8) & 0xff);
    (*data)[offset + 3] = static_cast<char>(value & 0xff);
}

quint32 crc32(const QByteArray& data)
{
    quint32 crc = 0xffffffffU;
    for (const unsigned char byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return crc ^ 0xffffffffU;
}

QByteArray apngWithUndecodableLaterRaster()
{
    QByteArray data = animationFixtureData(QStringLiteral("animated-smoke.apng"));
    qsizetype offset = 8;
    while (offset + 12 <= data.size()) {
        const quint32 payloadSize = readBigEndian32(data, offset);
        if (payloadSize > static_cast<quint32>(data.size() - offset - 12)) {
            return {};
        }

        const qsizetype typeOffset = offset + 4;
        const qsizetype payloadOffset = typeOffset + 4;
        const qsizetype crcOffset = payloadOffset + static_cast<qsizetype>(payloadSize);
        if (data.mid(typeOffset, 4) == QByteArrayLiteral("fdAT") && payloadSize > 4) {
            for (qsizetype index = payloadOffset + 4; index < crcOffset; ++index) {
                data[index] = '\0';
            }
            writeBigEndian32(&data, crcOffset, crc32(data.mid(typeOffset, 4 + payloadSize)));
            return data;
        }
        offset = crcOffset + 4;
    }
    return {};
}

kiriview::ImageDecodeDependencies actualDecodeDependencies(
    kiriview::TestSupport::ManualImageDataLoader& dataLoader,
    kiriview::TestSupport::ManualImageWorkerScheduler& workerScheduler)
{
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, [](const QByteArray& data, const kiriview::ImageDecodeRequest& request) {
                return kiriview::decodeImageData(data, request);
            });
    dependencies.workerScheduler = workerScheduler.scheduler();
    return dependencies;
}

void runOutstandingWorkerSchedules(
    kiriview::TestSupport::ManualImageWorkerScheduler& workerScheduler, std::size_t& nextSchedule)
{
    while (nextSchedule < workerScheduler.scheduleCount()) {
        if (workerScheduler.isActive(nextSchedule)) {
            workerScheduler.runWork(nextSchedule);
            workerScheduler.finish(nextSchedule);
        }
        ++nextSchedule;
    }
}

kiriview::ThumbnailCacheLookupResult readyThumbnailLookup()
{
    QImage image(400, 300, QImage::Format_RGBA8888);
    image.fill(QColor(Qt::blue));
    return kiriview::ThumbnailCacheLookupResult {
        kiriview::ThumbnailCacheLookupStatus::Ready,
        std::move(image),
        kiriview::ActiveNavigationThumbnailDemandBucket::XXLarge,
        kiriview::ActiveNavigationThumbnailDemandBucket::XXLarge,
        QStringLiteral("/cache/photo.png"),
        {},
    };
}

class ManualThumbnailLookupProvider
{
public:
    kiriview::ThumbnailCacheLookupProvider provider()
    {
        return [this](QObject*, kiriview::ThumbnailCacheLookupRequest request,
                   kiriview::ThumbnailCacheLookupCallback callback) {
            requests.push_back(std::move(request));
            callbacks.push_back(std::move(callback));
            return kiriview::ImageIoJob();
        };
    }

    void finish(std::size_t index, kiriview::ThumbnailCacheLookupResult result)
    {
        QVERIFY(index < callbacks.size());
        QVERIFY(callbacks.at(index));
        callbacks.at(index)(std::move(result));
    }

    std::vector<kiriview::ThumbnailCacheLookupRequest> requests;
    std::vector<kiriview::ThumbnailCacheLookupCallback> callbacks;
};

class RefiningDisplaySource final : public kiriview::StaticImageDisplaySource
{
public:
    explicit RefiningDisplaySource(QSize sourceSize = QSize(800, 600),
        kiriview::StaticImageSourceDetailModel detailModel
        = kiriview::StaticImageSourceDetailModel::FiniteRaster)
        : m_sourceSize(sourceSize)
        , m_detailModel(detailModel)
    {
    }

    QSize imageSize() const override { return m_sourceSize; }
    qsizetype byteCost() const override
    {
        return qsizetype(m_sourceSize.width()) * m_sourceSize.height() * 4;
    }
    kiriview::StaticImageSourceDetailModel detailModel() const override { return m_detailModel; }
    bool supportsRasterDisplayRefinement() const override { return true; }

    kiriview::StaticImageDisplayDecodeResult decodeRasterDisplayImage(
        const QSize& rasterSize) const override
    {
        ++refinementCount;
        lastRefinementSize = rasterSize;
        if (failRefinement) {
            return { {},
                { QStringLiteral("Could not refine the image"),
                    QStringLiteral("fake refinement failed") } };
        }
        const QSize resultSize = refinementResultSize.isEmpty() ? rasterSize : refinementResultSize;
        if (refinementBytesPerLine > 0) {
            refinementStorage = QByteArray(refinementBytesPerLine * resultSize.height(), char(0));
            QImage image(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- QImage byte API.
                reinterpret_cast<uchar*>(refinementStorage.data()), resultSize.width(),
                resultSize.height(), refinementBytesPerLine, QImage::Format_RGBA8888_Premultiplied);
            image.fill(QColor(Qt::red));
            return { std::move(image), {} };
        }
        QImage image(resultSize, QImage::Format_RGBA8888);
        image.fill(QColor(Qt::red));
        return { std::move(image), {} };
    }

    kiriview::StaticImageDisplayDecodeResult decodeBlockingDisplayImage(int) const override
    {
        return {};
    }

    mutable int refinementCount = 0;
    mutable QSize lastRefinementSize;
    bool failRefinement = false;
    QSize refinementResultSize;
    qsizetype refinementBytesPerLine = 0;
    mutable QByteArray refinementStorage;

private:
    QSize m_sourceSize;
    kiriview::StaticImageSourceDetailModel m_detailModel;
};

kiriview::StaticDisplayImagePayload firstDisplayPayload(
    const std::shared_ptr<RefiningDisplaySource>& refinementSource,
    QSize rasterSize = QSize(200, 150))
{
    QImage image(rasterSize, QImage::Format_RGBA8888);
    image.fill(QColor(Qt::yellow));
    return kiriview::StaticDisplayImagePayload {
        QStringLiteral("seeded-first-display"),
        {},
        refinementSource->imageSize(),
        std::move(image),
        kiriview::DisplayImageQuality::FirstDisplay,
        {},
        refinementSource,
        kiriview::DisplayImagePreviewOrigin::None,
        refinementSource->detailModel(),
        kiriview::ImageSourceRevision::fromData(authoritativeSeedData()),
        kiriview::DisplayImageRasterKind::AuthoritativeStill,
    };
}

void hostViewport(QQuickWindow& window, ImageViewport& viewport)
{
    window.resize(96, 64);
    viewport.setParentItem(window.contentItem());
    viewport.setSize(QSizeF(96, 64));
    window.show();
}

void renderFrame(QQuickWindow& window)
{
    window.update();
    window.grabWindow();
    QCoreApplication::processEvents();
}

template <typename Predicate> bool driveRenderUntil(QQuickWindow& window, Predicate predicate)
{
    constexpr int maximumAttempts = 100;
    constexpr int eventIntervalMilliseconds = 5;
    for (int attempt = 0; attempt < maximumAttempts; ++attempt) {
        if (predicate()) {
            return true;
        }
        renderFrame(window);
        QTest::qWait(eventIntervalMilliseconds);
    }
    return predicate();
}

class FakeImageViewportProviderSource final : public kiriview::ImageViewportProviderSource
{
public:
    struct PendingMetadata
    {
        kiriview::ImageViewportProviderWorkIdentity identity;
        MetadataCompletion completion;
    };

    struct PendingFrame
    {
        kiriview::ImageViewportProviderWorkIdentity identity;
        kiriview::ImageViewportProviderFrameRequest request;
        FrameCompletion completion;
    };

    ImageSequenceProviderMetadata constructionMetadata() const override { return knownMetadata; }

    void requestMetadata(const kiriview::ImageViewportProviderWorkIdentity& identity,
        MetadataCompletion completion) override
    {
        metadataRequests.push_back(identity);
        if (automaticMetadata.has_value()) {
            completion(identity, *automaticMetadata);
            return;
        }
        pendingMetadata.push_back({ identity, std::move(completion) });
    }

    void requestFrame(const kiriview::ImageViewportProviderWorkIdentity& identity,
        kiriview::ImageViewportProviderFrameRequest request, FrameCompletion completion) override
    {
        frameRequests.push_back(request);
        frameIdentities.push_back(identity);
        if (automaticFrame.has_value()) {
            completion(identity, *automaticFrame);
            return;
        }
        pendingFrames.push_back({ identity, std::move(request), std::move(completion) });
    }

    void cancel(const QVector<ImageSequenceProviderRequestToken>& tokens) override
    {
        cancelledTokenSets.push_back(tokens);
    }

    void close() override { ++closeCount; }

    void completeNextMetadata(kiriview::ImageViewportProviderMetadataResult result)
    {
        QVERIFY(!pendingMetadata.empty());
        PendingMetadata pending = std::move(pendingMetadata.front());
        pendingMetadata.pop_front();
        pending.completion(pending.identity, std::move(result));
    }

    void completeNextFrame(kiriview::ImageViewportProviderFrameResult result)
    {
        QVERIFY(!pendingFrames.empty());
        PendingFrame pending = std::move(pendingFrames.front());
        pendingFrames.pop_front();
        pending.completion(pending.identity, std::move(result));
    }

    void emitNextProvisionalFrame(kiriview::ImageViewportProviderFrameResult result)
    {
        QVERIFY(!pendingFrames.empty());
        PendingFrame& pending = pendingFrames.front();
        pending.completion(pending.identity, std::move(result));
    }

    ImageSequenceProviderMetadata knownMetadata;
    std::optional<kiriview::ImageViewportProviderMetadataResult> automaticMetadata;
    std::optional<kiriview::ImageViewportProviderFrameResult> automaticFrame;
    std::vector<kiriview::ImageViewportProviderWorkIdentity> metadataRequests;
    std::vector<kiriview::ImageViewportProviderFrameRequest> frameRequests;
    std::vector<kiriview::ImageViewportProviderWorkIdentity> frameIdentities;
    std::deque<PendingMetadata> pendingMetadata;
    std::deque<PendingFrame> pendingFrames;
    std::vector<QVector<ImageSequenceProviderRequestToken>> cancelledTokenSets;
    int closeCount = 0;
};

struct ProviderFixture
{
    explicit ProviderFixture(qsizetype displayStoreByteBudget = 1024 * 1024)
        : store(std::make_shared<kiriview::DisplayImageStore>(displayStoreByteBudget))
    {
    }

    std::shared_ptr<FakeImageViewportProviderSource> source
        = std::make_shared<FakeImageViewportProviderSource>();
    std::shared_ptr<kiriview::DisplayImageStore> store;
    std::shared_ptr<kiriview::ImageViewportFailureRegistry> failures
        = std::make_shared<kiriview::ImageViewportFailureRegistry>();
    std::shared_ptr<kiriview::ImageViewportProviderResource> resource;
    std::unique_ptr<kiriview::ImageViewportSequenceProvider> adapter;
    QScopedPointer<ImageSequenceFactoryResult> factoryResult;

    void create()
    {
        resource = std::make_shared<kiriview::ImageViewportProviderResource>(
            41, QStringLiteral("fake-location"), source, store, failures);
        adapter = std::make_unique<kiriview::ImageViewportSequenceProvider>(resource);
        ImageSequenceFactory factory;
        factoryResult.reset(factory.fromProvider(adapter.get()));
        QVERIFY(factoryResult);
        QCOMPARE(factoryResult->outcome(), ImageSequenceFactoryOutcome::Created);
        QVERIFY(factoryResult->sequence());
    }

    void assign(ImageViewport& viewport)
    {
        viewport.setSize(QSizeF(96, 64));
        QCOMPARE(
            viewport
                .setPresentationTarget(ImageViewportPresentationTarget(factoryResult->sequence()),
                    PresentationTargetTransitionPolicy {})
                .outcome(),
            ImageViewportCommandOutcome::Accepted);
    }
};
}

class TestImageViewportSequenceProvider : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void metadataAndStillFrameFlowThroughProvider();
    void productionDecodeStartsOnlyForProviderDemand();
    void deferredDecodeSourcePreservesDemandUntilResolution();
    void deferredDecodeSourceAuthoritativeSeedFlushesQueuedDemandSynchronously();
    void deferredDecodeSourceInvalidationDropsLateResolution_data();
    void deferredDecodeSourceInvalidationDropsLateResolution();
    void deferredDecodeSourceSuppressesProvisionalFrameUntilAuthoritativeTerminal();
    void providerResourceSeparatesWorkAndDisplayReuseIdentity();
    void providerResourcePreservesDistinctTimedFramePixelsAndHandles();
    void apngFirstFramePublishesBeforeLaterRasterFailure();
    void actualApngFramesPreservePixelsThroughProviderResource();
    void animationFrameCompletionAfterInvalidationIsDropped_data();
    void animationFrameCompletionAfterInvalidationIsDropped();
    void foregroundThumbnailRemainsProvisionalUntilAuthoritativeTerminal_data();
    void foregroundThumbnailRemainsProvisionalUntilAuthoritativeTerminal();
    void readerOrientationProducesNormalizedFrame();
    void mismatchedRefinementSourceCannotSeedAuthoritativeState();
    void staleAuthoritativeSeedLoadsAndDecodesCurrentSource();
    void seededSufficientFirstDisplayCompletesInitialWithoutRefinement();
    void seededInsufficientFirstDisplayRefinesBeforeInitialTerminal_data();
    void seededInsufficientFirstDisplayRefinesBeforeInitialTerminal();
    void seededRequireExactRefinesFullSourceBeforeInitialTerminal();
    void scalableSourceRefinesBeyondIntrinsicRaster();
    void scalableSourceRejectsRequireExact();
    void payloadRejectionMapsThroughProvider();
    void publicAndPrivatePayloadLimitsHaveDistinctOutcomes();
    void actualRefinementByteCostPreservesLimitOutcome_data();
    void actualRefinementByteCostPreservesLimitOutcome();
    void refinementFailureIsNotReclassifiedAsResourceExhaustion();
    void invalidRefinementGeometryIsProviderInternal();
    void initialDetailWaitPublishesFirstDisplayFallbackOnce();
    void initialDetailDeadlineInvalidationDropsQueuedFallback();
    void newerInitialRefinementBoundsTimedOutWork();
    void synchronousInitialRefinementCompletesExactlyOnce();
    void initialRefinementRespectsDisplayStoreEntryBudget();
    void concurrentRefinementUsesRequestAdmissibleWorkerResult();
    void provisionalFrameDoesNotBecomeCurrentStillDisplayImage();
    void authoritativeStillPayloadLookupKeepsDisplayedRevision();
    void metadataCompletionAfterInvalidationIsDropped_data();
    void metadataCompletionAfterInvalidationIsDropped();
    void refinementWorkerCompletionAndInvalidationAreOwnedBySource();
    void workerUnitIdentitySkipsLiveIdsAfterWrap();
    void outOfOrderRefinementCannotDowngradeReusableAuthoritativeImage();
    void animationDemandRequestsOnlyTheSelectedFrame();
    void staleCompletionMayCacheButCannotPublish();
    void componentFrameHandlePinsStoreUntilRelease();
    void failureReferenceResolvesAndRetiresWithHandle();
};

void TestImageViewportSequenceProvider::productionDecodeStartsOnlyForProviderDemand()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    const QUrl url(QStringLiteral("file:///tmp/provider-demand.png"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(71,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder()));

    QVERIFY(dataLoader.empty());
    source->requestMetadata(
        kiriview::ImageViewportProviderWorkIdentity {
            71,
            ImageViewportPageRole::Primary,
            ImageSequenceProviderRequestToken {},
            ImageViewportDemandRevisionToken {},
            QStringLiteral("provider-demand"),
        },
        [](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderMetadataResult) { });

    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
}

void TestImageViewportSequenceProvider::deferredDecodeSourcePreservesDemandUntilResolution()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(dataLoader,
            kiriview::TestSupport::staticImageDataDecoder(
                kiriview::TestSupport::testImage(128, 64)));
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        std::move(dependencies), kiriview::ImageViewportProvisionalPreviewPolicy::Allow);
    const kiriview::ImageViewportProviderWorkIdentity identity {
        91,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("deferred-provider-demand"),
    };
    std::vector<kiriview::ImageViewportProviderWorkIdentity> metadataIdentities;
    std::vector<kiriview::ImageViewportProviderMetadataResult> metadataResults;
    std::vector<kiriview::ImageViewportProviderWorkIdentity> frameIdentities;
    std::vector<kiriview::ImageViewportProviderFrameResult> frameResults;

    source->requestMetadata(identity,
        [&metadataIdentities, &metadataResults](
            kiriview::ImageViewportProviderWorkIdentity completedIdentity,
            kiriview::ImageViewportProviderMetadataResult result) {
            metadataIdentities.push_back(std::move(completedIdentity));
            metadataResults.push_back(std::move(result));
        });
    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            {},
        },
        [&frameIdentities, &frameResults](
            kiriview::ImageViewportProviderWorkIdentity completedIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            frameIdentities.push_back(std::move(completedIdentity));
            frameResults.push_back(std::move(result));
        });

    QVERIFY(dataLoader.empty());
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(0));
    QCOMPARE(metadataResults.size(), std::size_t(0));
    QCOMPARE(frameResults.size(), std::size_t(0));

    const QUrl resolvedUrl(QStringLiteral("file:///tmp/deferred-provider-demand.png"));
    QVERIFY(source->resolveSession(kiriview::ImageLoadSession(91,
        kiriview::ImageLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(resolvedUrl, {})),
        kiriview::DisplayedImageLocation::fromUrl(resolvedUrl))));
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, resolvedUrl);

    const QUrl rejectedUrl(QStringLiteral("file:///tmp/rejected-late-session.png"));
    QVERIFY(!source->resolveSession(kiriview::ImageLoadSession(92,
        kiriview::ImageLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(rejectedUrl, {})),
        kiriview::DisplayedImageLocation::fromUrl(rejectedUrl))));
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));

    dataLoader.finishFrontLoad(QByteArrayLiteral("decoded-image-data"));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);

    QCOMPARE(metadataResults.size(), std::size_t(1));
    QCOMPARE(metadataIdentities.size(), std::size_t(1));
    QVERIFY(metadataIdentities.front() == identity);
    QVERIFY(metadataResults.front().metadata.has_value());
    QCOMPARE(frameResults.size(), std::size_t(1));
    QCOMPARE(frameIdentities.size(), std::size_t(1));
    QVERIFY(frameIdentities.front() == identity);
    QVERIFY(frameResults.front().displayImage.has_value());
    QVERIFY(!frameResults.front().isProvisional());
}

void TestImageViewportSequenceProvider::
    deferredDecodeSourceAuthoritativeSeedFlushesQueuedDemandSynchronously()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder()));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        92,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("seeded-deferred-provider"),
    };
    std::vector<kiriview::ImageViewportProviderMetadataResult> metadataResults;
    std::vector<kiriview::ImageViewportProviderFrameResult> frameResults;
    source->requestMetadata(identity,
        [&metadataResults](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderMetadataResult result) {
            metadataResults.push_back(std::move(result));
        });
    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            {},
        },
        [&frameResults](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            frameResults.push_back(std::move(result));
        });

    const QUrl url(QStringLiteral("file:///tmp/seeded-deferred-provider.png"));
    QVERIFY(source->resolveSession(kiriview::ImageLoadSession(92,
                                       kiriview::ImageLoadRequest::fromExternalSource(
                                           kiriview::resolvedNavigationSource(url, {})),
                                       kiriview::DisplayedImageLocation::fromUrl(url)),
        authoritativeSeedForUrl(url, displayPayload(kiriview::DisplayImageQuality::Exact))));

    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    QVERIFY(!dataLoader.hasActiveLoadForUrl(url));
    QCOMPARE(metadataResults.size(), std::size_t(1));
    QVERIFY(metadataResults.front().metadata.has_value());
    QCOMPARE(frameResults.size(), std::size_t(1));
    QVERIFY(frameResults.front().displayImage.has_value());
    QVERIFY(!frameResults.front().isProvisional());
}

void TestImageViewportSequenceProvider::deferredDecodeSourceInvalidationDropsLateResolution_data()
{
    QTest::addColumn<bool>("closeSource");

    QTest::newRow("cancel") << false;
    QTest::newRow("close") << true;
}

void TestImageViewportSequenceProvider::deferredDecodeSourceInvalidationDropsLateResolution()
{
    QFETCH(bool, closeSource);

    ProviderFixture tokenFixture;
    tokenFixture.source->knownMetadata = ImageSequenceProviderMetadata::still(QSizeF(32, 16));
    tokenFixture.create();
    ImageViewport tokenViewport;
    tokenFixture.assign(tokenViewport);
    QTRY_COMPARE(tokenFixture.source->frameIdentities.size(), std::size_t(1));
    const ImageSequenceProviderRequestToken requestToken
        = tokenFixture.source->frameIdentities.front().requestToken;
    QVERIFY(requestToken.isValid());

    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::TestSupport::imageDecodeDependenciesFor(dataLoader,
            kiriview::TestSupport::staticImageDataDecoder(
                kiriview::TestSupport::testImage(128, 64))));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        93,
        ImageViewportPageRole::Primary,
        requestToken,
        {},
        QStringLiteral("invalidated-deferred-provider"),
    };
    int metadataCompletionCount = 0;
    int frameCompletionCount = 0;
    source->requestMetadata(identity,
        [&metadataCompletionCount](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderMetadataResult) { ++metadataCompletionCount; });
    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            {},
        },
        [&frameCompletionCount](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult) { ++frameCompletionCount; });
    QVERIFY(dataLoader.empty());

    if (closeSource) {
        source->close();
    } else {
        source->cancel({ requestToken });
    }

    const QUrl resolvedUrl(QStringLiteral("file:///tmp/invalidated-deferred-provider.png"));
    const bool resolved
        = source->resolveSession(kiriview::ImageLoadSession(93,
                                     kiriview::ImageLoadRequest::fromExternalSource(
                                         kiriview::resolvedNavigationSource(resolvedUrl, {})),
                                     kiriview::DisplayedImageLocation::fromUrl(resolvedUrl)),
            displayPayload(kiriview::DisplayImageQuality::Exact));

    QCOMPARE(resolved, !closeSource);
    QCOMPARE(metadataCompletionCount, 0);
    QCOMPARE(frameCompletionCount, 0);
    QVERIFY(dataLoader.empty());
}

void TestImageViewportSequenceProvider::
    deferredDecodeSourceSuppressesProvisionalFrameUntilAuthoritativeTerminal()
{
    const QByteArray data = encodedPngData(QSize(800, 600));
    QVERIFY(!data.isEmpty());
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    ManualThumbnailLookupProvider thumbnailLookup;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(dataLoader,
            kiriview::TestSupport::staticImageDataDecoder(
                kiriview::TestSupport::testImage(800, 600)));
    dependencies.thumbnailPreviewLookupProvider = thumbnailLookup.provider();
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        std::move(dependencies), kiriview::ImageViewportProvisionalPreviewPolicy::Suppress);
    const kiriview::ImageViewportProviderWorkIdentity identity {
        94,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("suppressed-provisional"),
    };
    std::vector<kiriview::ImageViewportProviderMetadataResult> metadataResults;
    std::vector<kiriview::ImageViewportProviderWorkIdentity> frameIdentities;
    std::vector<kiriview::ImageViewportProviderFrameResult> frameResults;
    source->requestMetadata(identity,
        [&metadataResults](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderMetadataResult result) {
            metadataResults.push_back(std::move(result));
        });
    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            {},
        },
        [&frameIdentities, &frameResults](
            kiriview::ImageViewportProviderWorkIdentity completedIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            frameIdentities.push_back(std::move(completedIdentity));
            frameResults.push_back(std::move(result));
        });

    const QUrl url(QStringLiteral("file:///tmp/suppressed-provisional.png"));
    QVERIFY(source->resolveSession(kiriview::ImageLoadSession(94,
        kiriview::ImageLoadRequest::fromExternalSource(kiriview::resolvedNavigationSource(url, {})),
        kiriview::DisplayedImageLocation::fromUrl(url))));
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(data);
    QCOMPARE(thumbnailLookup.requests.size(), std::size_t(1));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));

    thumbnailLookup.finish(0, readyThumbnailLookup());
    QCOMPARE(metadataResults.size(), std::size_t(1));
    QVERIFY(metadataResults.front().metadata.has_value());
    QCOMPARE(frameResults.size(), std::size_t(0));

    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QCOMPARE(frameResults.size(), std::size_t(1));
    QCOMPARE(frameIdentities.size(), std::size_t(1));
    QVERIFY(frameIdentities.front() == identity);
    QVERIFY(frameResults.front().displayImage.has_value());
    QVERIFY(!frameResults.front().isProvisional());
}

void TestImageViewportSequenceProvider::providerResourceSeparatesWorkAndDisplayReuseIdentity()
{
    const auto sourceWithAutomaticFrame = []() {
        auto source = std::make_shared<FakeImageViewportProviderSource>();
        source->automaticFrame = kiriview::ImageViewportProviderFrameResult::ready(
            displayPayload(kiriview::DisplayImageQuality::Exact),
            ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("png"));
        return source;
    };
    const auto requestFrame
        = [](const std::shared_ptr<kiriview::ImageViewportProviderResource>& resource,
              kiriview::ImageViewportProviderWorkIdentity identity) {
              kiriview::ImageViewportProviderPreparedFrame prepared;
              resource->requestFrame(identity,
                  kiriview::ImageViewportProviderFrameRequest {
                      0,
                      {},
                  },
                  [&prepared](kiriview::ImageViewportProviderWorkIdentity,
                      kiriview::ImageViewportProviderPreparedFrame result) {
                      prepared = std::move(result);
                  });
              return prepared;
          };

    auto sharedStore = std::make_shared<kiriview::DisplayImageStore>(1024 * 1024);
    auto firstSource = sourceWithAutomaticFrame();
    auto secondSource = sourceWithAutomaticFrame();
    auto firstResource = std::make_shared<kiriview::ImageViewportProviderResource>(
        101, QStringLiteral("work-a"), firstSource, sharedStore);
    auto secondResource = std::make_shared<kiriview::ImageViewportProviderResource>(
        102, QStringLiteral("work-b"), secondSource, sharedStore);
    QVERIFY(firstResource->bindDisplayLocationIdentity(QStringLiteral("resolved-display")));
    QVERIFY(secondResource->bindDisplayLocationIdentity(QStringLiteral("resolved-display")));
    QVERIFY(!firstResource->bindDisplayLocationIdentity(QStringLiteral("replacement-display")));

    const kiriview::ImageViewportProviderPreparedFrame firstPrepared = requestFrame(firstResource,
        kiriview::ImageViewportProviderWorkIdentity {
            101,
            ImageViewportPageRole::Primary,
            {},
            {},
            QStringLiteral("work-a"),
        });
    const kiriview::ImageViewportProviderPreparedFrame secondPrepared = requestFrame(secondResource,
        kiriview::ImageViewportProviderWorkIdentity {
            102,
            ImageViewportPageRole::Primary,
            {},
            {},
            QStringLiteral("work-b"),
        });

    QVERIFY(firstPrepared.isReady());
    QVERIFY(secondPrepared.isReady());
    QCOMPARE(firstPrepared.storeEntryId, secondPrepared.storeEntryId);
    QCOMPARE(sharedStore->size(), qsizetype(1));
    QCOMPARE(firstSource->frameIdentities.front().locationIdentity, QStringLiteral("work-a"));
    QCOMPARE(secondSource->frameIdentities.front().locationIdentity, QStringLiteral("work-b"));

    auto legacyStore = std::make_shared<kiriview::DisplayImageStore>(1024 * 1024);
    auto firstLegacyResource = std::make_shared<kiriview::ImageViewportProviderResource>(
        103, QStringLiteral("legacy-display"), sourceWithAutomaticFrame(), legacyStore);
    auto secondLegacyResource = std::make_shared<kiriview::ImageViewportProviderResource>(
        104, QStringLiteral("legacy-display"), sourceWithAutomaticFrame(), legacyStore);
    const kiriview::ImageViewportProviderPreparedFrame firstLegacyPrepared
        = requestFrame(firstLegacyResource,
            kiriview::ImageViewportProviderWorkIdentity {
                103,
                ImageViewportPageRole::Primary,
                {},
                {},
                QStringLiteral("legacy-display"),
            });
    const kiriview::ImageViewportProviderPreparedFrame secondLegacyPrepared
        = requestFrame(secondLegacyResource,
            kiriview::ImageViewportProviderWorkIdentity {
                104,
                ImageViewportPageRole::Primary,
                {},
                {},
                QStringLiteral("legacy-display"),
            });

    QVERIFY(firstLegacyPrepared.isReady());
    QCOMPARE(firstLegacyPrepared.storeEntryId, secondLegacyPrepared.storeEntryId);
    QVERIFY(!firstLegacyResource->bindDisplayLocationIdentity(QStringLiteral("too-late")));

    auto unsupportedSource = std::make_shared<FakeImageViewportProviderSource>();
    unsupportedSource->automaticFrame = kiriview::ImageViewportProviderFrameResult::unsupported(
        ImageSequenceProviderUnsupportedCause::PayloadRejection);
    auto unsupportedResource = std::make_shared<kiriview::ImageViewportProviderResource>(
        105, QStringLiteral("unsupported-work"), unsupportedSource, legacyStore);
    const kiriview::ImageViewportProviderPreparedFrame unsupportedPrepared
        = requestFrame(unsupportedResource,
            kiriview::ImageViewportProviderWorkIdentity {
                105,
                ImageViewportPageRole::Primary,
                {},
                {},
                QStringLiteral("unsupported-work"),
            });
    QVERIFY(unsupportedPrepared.isUnsupported());
    QVERIFY(!unsupportedResource->bindDisplayLocationIdentity(QStringLiteral("too-late")));
}

void TestImageViewportSequenceProvider::
    providerResourcePreservesDistinctTimedFramePixelsAndHandles()
{
    auto source = std::make_shared<FakeImageViewportProviderSource>();
    auto store = std::make_shared<kiriview::DisplayImageStore>(1024 * 1024);
    auto resource = std::make_shared<kiriview::ImageViewportProviderResource>(
        111, QStringLiteral("timed-work"), source, store);
    QVERIFY(resource->bindDisplayLocationIdentity(QStringLiteral("timed-display")));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        111,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("timed-work"),
    };
    const auto requestFrame = [&resource, &source, &identity](int frame, QColor color) {
        kiriview::ImageViewportProviderPreparedFrame prepared;
        resource->requestFrame(identity,
            kiriview::ImageViewportProviderFrameRequest {
                frame,
                {},
            },
            [&prepared](kiriview::ImageViewportProviderWorkIdentity,
                kiriview::ImageViewportProviderPreparedFrame result) {
                prepared = std::move(result);
            });
        kiriview::StaticDisplayImagePayload payload
            = displayPayload(kiriview::DisplayImageQuality::Exact, {}, QSize(128, 64), color);
        payload.rasterKind = kiriview::DisplayImageRasterKind::TimedFrame;
        source->completeNextFrame(
            kiriview::ImageViewportProviderFrameResult::ready(std::move(payload),
                ImageSequenceProviderFrameEnvelope::timedFrame(frame, frame * 20, 20),
                QStringLiteral("test-animation")));
        return prepared;
    };

    const kiriview::ImageViewportProviderPreparedFrame first = requestFrame(0, Qt::red);
    const kiriview::ImageViewportProviderPreparedFrame second = requestFrame(1, Qt::blue);

    QVERIFY(first.isReady());
    QVERIFY(second.isReady());
    QVERIFY(first.storeEntryId != second.storeEntryId);
    QCOMPARE(store->entry(first.storeEntryId)->image.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(store->entry(second.storeEntryId)->image.pixelColor(0, 0), QColor(Qt::blue));
    ImageSequenceProviderFrameHandle* firstHandle = resource->acquireFrameHandle(first);
    ImageSequenceProviderFrameHandle* secondHandle = resource->acquireFrameHandle(second);
    QVERIFY(firstHandle != nullptr);
    QVERIFY(secondHandle != nullptr);
    QVERIFY(firstHandle->frame() != nullptr);
    QVERIFY(secondHandle->frame() != nullptr);
    firstHandle->release();
    secondHandle->release();
}

void TestImageViewportSequenceProvider::apngFirstFramePublishesBeforeLaterRasterFailure()
{
    const QByteArray data = apngWithUndecodableLaterRaster();
    QVERIFY(!data.isEmpty());

    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    const QUrl url(QStringLiteral("file:///tmp/provider-first-frame.apng"));
    const QString locationIdentity = QStringLiteral("provider-first-frame");
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(112,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        actualDecodeDependencies(dataLoader, workerScheduler));
    auto store = std::make_shared<kiriview::DisplayImageStore>(1024 * 1024);
    auto resource = std::make_shared<kiriview::ImageViewportProviderResource>(
        112, locationIdentity, source, store);
    QVERIFY(resource->bindDisplayLocationIdentity(locationIdentity));

    std::vector<kiriview::ImageViewportProviderPreparedFrame> preparedFrames;
    const kiriview::ImageViewportProviderWorkIdentity identity {
        112,
        ImageViewportPageRole::Primary,
        {},
        {},
        locationIdentity,
    };
    const auto requestFrame = [&resource, &identity, &preparedFrames](int frame) {
        resource->requestFrame(identity,
            kiriview::ImageViewportProviderFrameRequest {
                frame,
                {},
            },
            [&preparedFrames](kiriview::ImageViewportProviderWorkIdentity,
                kiriview::ImageViewportProviderPreparedFrame prepared) {
                preparedFrames.push_back(std::move(prepared));
            });
    };

    requestFrame(0);
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(data);
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);

    QVERIFY2(preparedFrames.size() == 1,
        "The retained APNG first frame must publish without completing later animation work");
    QVERIFY(preparedFrames.front().isReady());
    QVERIFY(preparedFrames.front().envelope.isTimedFrame());
    QCOMPARE(preparedFrames.front().envelope.frame(), 0);
    const std::optional<kiriview::DisplayImageStoreEntry> stored
        = store->entry(preparedFrames.front().storeEntryId);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->image.pixelColor(0, 0), QColor(Qt::red));

    requestFrame(1);
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);
    workerScheduler.finish(1);
    QCOMPARE(preparedFrames.size(), std::size_t(2));
    QVERIFY(!preparedFrames.back().isReady());
    QCOMPARE(preparedFrames.back().failureCause, ImageSequenceProviderFailureCause::Decode);
    QCOMPARE(store->size(), qsizetype(1));
}

void TestImageViewportSequenceProvider::actualApngFramesPreservePixelsThroughProviderResource()
{
    const QByteArray data = animationFixtureData(QStringLiteral("animated-smoke.apng"));
    QVERIFY(!data.isEmpty());

    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    const QUrl url(QStringLiteral("file:///tmp/provider-distinct-frames.apng"));
    const QString locationIdentity = QStringLiteral("provider-distinct-frames");
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(113,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        actualDecodeDependencies(dataLoader, workerScheduler));
    auto store = std::make_shared<kiriview::DisplayImageStore>(1024 * 1024);
    auto resource = std::make_shared<kiriview::ImageViewportProviderResource>(
        113, locationIdentity, source, store);
    QVERIFY(resource->bindDisplayLocationIdentity(locationIdentity));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        113,
        ImageViewportPageRole::Primary,
        {},
        {},
        locationIdentity,
    };

    std::vector<kiriview::ImageViewportProviderPreparedFrame> preparedFrames;
    const auto requestFrame = [&resource, &identity, &preparedFrames](int frame) {
        resource->requestFrame(identity,
            kiriview::ImageViewportProviderFrameRequest {
                frame,
                {},
            },
            [&preparedFrames](kiriview::ImageViewportProviderWorkIdentity,
                kiriview::ImageViewportProviderPreparedFrame prepared) {
                preparedFrames.push_back(std::move(prepared));
            });
    };

    requestFrame(0);
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(data);
    std::size_t nextSchedule = 0;
    runOutstandingWorkerSchedules(workerScheduler, nextSchedule);
    QCOMPARE(preparedFrames.size(), std::size_t(1));

    requestFrame(1);
    runOutstandingWorkerSchedules(workerScheduler, nextSchedule);
    QCOMPARE(preparedFrames.size(), std::size_t(2));

    const kiriview::ImageViewportProviderPreparedFrame& first = preparedFrames.at(0);
    const kiriview::ImageViewportProviderPreparedFrame& second = preparedFrames.at(1);
    QVERIFY(first.isReady());
    QVERIFY(second.isReady());
    QVERIFY(first.envelope.isTimedFrame());
    QVERIFY(second.envelope.isTimedFrame());
    QCOMPARE(first.envelope.frame(), 0);
    QCOMPARE(second.envelope.frame(), 1);
    QVERIFY(first.storeEntryId != second.storeEntryId);

    ImageSequenceProviderFrameHandle* firstHandle = resource->acquireFrameHandle(first);
    ImageSequenceProviderFrameHandle* secondHandle = resource->acquireFrameHandle(second);
    QVERIFY(firstHandle != nullptr);
    QVERIFY(secondHandle != nullptr);
    QVERIFY(firstHandle->frame() != nullptr);
    QVERIFY(secondHandle->frame() != nullptr);
    QVERIFY(firstHandle->frame() != secondHandle->frame());

    const std::optional<kiriview::DisplayImageStoreEntry> firstStored
        = store->entry(first.storeEntryId);
    const std::optional<kiriview::DisplayImageStoreEntry> secondStored
        = store->entry(second.storeEntryId);
    QVERIFY(firstStored.has_value());
    QVERIFY(secondStored.has_value());
    QCOMPARE(firstStored->image.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(secondStored->image.pixelColor(0, 0), QColor(Qt::blue));

    firstHandle->release();
    secondHandle->release();
}

void TestImageViewportSequenceProvider::animationFrameCompletionAfterInvalidationIsDropped_data()
{
    QTest::addColumn<bool>("closeResource");

    QTest::newRow("cancel") << false;
    QTest::newRow("close") << true;
}

void TestImageViewportSequenceProvider::animationFrameCompletionAfterInvalidationIsDropped()
{
    QFETCH(bool, closeResource);

    ProviderFixture tokenFixture;
    tokenFixture.source->knownMetadata = ImageSequenceProviderMetadata::still(QSizeF(32, 16));
    tokenFixture.create();
    ImageViewport tokenViewport;
    tokenFixture.assign(tokenViewport);
    QTRY_COMPARE(tokenFixture.source->frameIdentities.size(), std::size_t(1));
    const ImageSequenceProviderRequestToken requestToken
        = tokenFixture.source->frameIdentities.front().requestToken;
    QVERIFY(requestToken.isValid());

    const QByteArray data = animationFixtureData(QStringLiteral("animated-smoke.apng"));
    QVERIFY(!data.isEmpty());
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    const QUrl url(QStringLiteral("file:///tmp/provider-late-animation-frame.apng"));
    const QString locationIdentity = QStringLiteral("provider-late-animation-frame");
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(114,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        actualDecodeDependencies(dataLoader, workerScheduler));
    auto store = std::make_shared<kiriview::DisplayImageStore>(1024 * 1024);
    auto resource = std::make_shared<kiriview::ImageViewportProviderResource>(
        114, locationIdentity, source, store);
    QVERIFY(resource->bindDisplayLocationIdentity(locationIdentity));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        114,
        ImageViewportPageRole::Primary,
        requestToken,
        {},
        locationIdentity,
    };

    std::vector<kiriview::ImageViewportProviderPreparedFrame> preparedFrames;
    const auto requestFrame = [&resource, &identity, &preparedFrames](int frame) {
        resource->requestFrame(identity,
            kiriview::ImageViewportProviderFrameRequest {
                frame,
                {},
            },
            [&preparedFrames](kiriview::ImageViewportProviderWorkIdentity,
                kiriview::ImageViewportProviderPreparedFrame prepared) {
                preparedFrames.push_back(std::move(prepared));
            });
    };

    requestFrame(0);
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(data);
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QCOMPARE(preparedFrames.size(), std::size_t(1));

    requestFrame(1);
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);
    if (closeResource) {
        resource->close();
    } else {
        resource->cancel({ requestToken });
    }
    workerScheduler.finish(1);

    QCOMPARE(preparedFrames.size(), std::size_t(1));
}

void TestImageViewportSequenceProvider::
    foregroundThumbnailRemainsProvisionalUntilAuthoritativeTerminal_data()
{
    QTest::addColumn<bool>("authoritativeSuccess");

    QTest::newRow("success") << true;
    QTest::newRow("failure") << false;
}

void TestImageViewportSequenceProvider::
    foregroundThumbnailRemainsProvisionalUntilAuthoritativeTerminal()
{
    QFETCH(bool, authoritativeSuccess);

    const QByteArray data = encodedPngData(QSize(800, 600));
    QVERIFY(!data.isEmpty());
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    ManualThumbnailLookupProvider thumbnailLookup;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(dataLoader,
            authoritativeSuccess
                ? kiriview::TestSupport::staticImageDataDecoder(
                      kiriview::TestSupport::testImage(800, 600))
                : kiriview::ImageDataDecoder(
                      [](const QByteArray&, const kiriview::ImageDecodeRequest&) {
                          return kiriview::TestSupport::failedTestImageDecodeResult();
                      }));
    dependencies.thumbnailPreviewLookupProvider = thumbnailLookup.provider();
    dependencies.workerScheduler = workerScheduler.scheduler();

    const QUrl url(QStringLiteral("file:///tmp/provisional-photo.png"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(72,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies));
    auto store = std::make_shared<kiriview::DisplayImageStore>(8 * 1024 * 1024);
    auto resource = std::make_shared<kiriview::ImageViewportProviderResource>(72,
        QStringLiteral("provisional-photo"), source, store,
        std::make_shared<kiriview::ImageViewportFailureRegistry>());
    kiriview::ImageViewportSequenceProvider adapter(resource);
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> factoryResult(factory.fromProvider(&adapter));
    QVERIFY(factoryResult);
    QCOMPARE(factoryResult->outcome(), ImageSequenceFactoryOutcome::Created);

    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    QCOMPARE(viewport
                 .setPresentationTarget(ImageViewportPresentationTarget(factoryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(data);
    QTRY_COMPARE(thumbnailLookup.requests.size(), std::size_t(1));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    thumbnailLookup.finish(0, readyThumbnailLookup());

    QVERIFY(driveRenderUntil(window, [&viewport]() {
        return viewport.state().display().status() == ImageViewportDisplayStatus::Ready;
    }));
    QCOMPARE(viewport.state().request().status(), ImageViewportRequestStatus::Loading);
    QCOMPARE(viewport.state().request().reason(), ImageViewportRequestReason::ProviderWaiting);
    QCOMPARE(viewport.state().primary().display().quality(), ImageViewportPayloadQuality::Preview);
    QCOMPARE(viewport.state().primary().display().currentForDemand(), false);
    QVERIFY(
        !resource->currentStillDisplayImage(viewport.state().primary().display().demandRevision())
            .has_value());

    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    if (authoritativeSuccess) {
        QVERIFY(driveRenderUntil(window, [&viewport]() {
            return viewport.state().request().status() == ImageViewportRequestStatus::Ready;
        }));
        QCOMPARE(
            viewport.state().primary().display().quality(), ImageViewportPayloadQuality::Exact);
        QVERIFY(resource->acceptDisplayedStillDisplayImage(
            ImageViewportPageRole::Primary, viewport.state().primary().display().demandRevision()));
        const std::optional<kiriview::StaticDisplayImagePayload> current
            = resource->currentStillDisplayImage(
                viewport.state().primary().display().demandRevision());
        QVERIFY(current.has_value());
        QCOMPARE(current->quality, kiriview::DisplayImageQuality::Exact);
    } else {
        QTRY_COMPARE(viewport.state().request().status(), ImageViewportRequestStatus::Error);
        QCOMPARE(viewport.state().display().status(), ImageViewportDisplayStatus::Empty);
        QVERIFY(!resource
                ->currentStillDisplayImage(viewport.state().primary().display().demandRevision())
                .has_value());
    }
}

void TestImageViewportSequenceProvider::readerOrientationProducesNormalizedFrame()
{
    ProviderFixture fixture;
    fixture.source->knownMetadata = ImageSequenceProviderMetadata::still(QSizeF(16, 32));
    kiriview::StaticDisplayImagePayload payload
        = displayPayload(kiriview::DisplayImageQuality::Exact, QSize(16, 32), QSize(16, 32));
    payload.imageReaderTransform.transformations = QImageIOHandler::TransformationRotate90;
    fixture.source->automaticFrame
        = kiriview::ImageViewportProviderFrameResult::ready(std::move(payload),
            ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("jpg"));
    fixture.create();

    kiriview::ImageViewportProviderPreparedFrame prepared;
    fixture.resource->requestFrame(
        kiriview::ImageViewportProviderWorkIdentity {
            41,
            ImageViewportPageRole::Primary,
            {},
            {},
            QStringLiteral("fake-location"),
        },
        {},
        [&prepared](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderPreparedFrame result) { prepared = std::move(result); });
    QScopedPointer<ImageSequenceProviderFrameHandle> handle(
        fixture.resource->acquireFrameHandle(prepared));
    QVERIFY(handle);
    QVERIFY(handle->frame());
    QVERIFY(handle->frame()->isValid());
    QCOMPARE(handle->frame()->orientationPolicy(), ImageFrame::OrientationPolicy::Rotate90);
    QCOMPARE(handle->frame()->sourceLogicalSize(), QSizeF(16, 32));
    QCOMPARE(handle->frame()->payloadRasterSize(), QSizeF(16, 32));
    handle->release();
}

void TestImageViewportSequenceProvider::metadataAndStillFrameFlowThroughProvider()
{
    ProviderFixture fixture;
    fixture.source->automaticMetadata = kiriview::ImageViewportProviderMetadataResult::ready(
        ImageSequenceProviderMetadata::still(QSizeF(128, 64)));
    fixture.source->automaticFrame = kiriview::ImageViewportProviderFrameResult::ready(
        displayPayload(kiriview::DisplayImageQuality::Exact),
        ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("png"));
    fixture.create();

    ImageViewport viewport;
    fixture.assign(viewport);

    QTRY_COMPARE(fixture.source->metadataRequests.size(), std::size_t(1));
    QTRY_COMPARE(fixture.source->frameRequests.size(), std::size_t(1));
    QCOMPARE(fixture.source->frameRequests.front().frame, 0);
    QCOMPARE(fixture.source->frameIdentities.front().sourceGeneration, quint64(41));
    QCOMPARE(fixture.source->frameIdentities.front().role, ImageViewportPageRole::Primary);
    QVERIFY(fixture.source->frameIdentities.front().requestToken.isValid());
    QVERIFY(fixture.source->frameIdentities.front().demandRevision.isValid());
    QCOMPARE(
        fixture.source->frameIdentities.front().locationIdentity, QStringLiteral("fake-location"));
    QTRY_COMPARE(fixture.store->size(), qsizetype(1));
}

void TestImageViewportSequenceProvider::mismatchedRefinementSourceCannotSeedAuthoritativeState()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    kiriview::StaticDisplayImagePayload mismatchedSeed
        = firstDisplayPayload(refinementSource, QSize(200, 150));
    mismatchedSeed.originalSize = QSize(400, 300);
    QVERIFY(!mismatchedSeed.isValid());
    const QUrl url(QStringLiteral("file:///tmp/mismatched-refinement-source.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(72,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder()),
        std::move(mismatchedSeed));

    QVERIFY(!source->constructionMetadata().isSpecified());
    source->requestMetadata(
        kiriview::ImageViewportProviderWorkIdentity {
            72,
            ImageViewportPageRole::Primary,
            {},
            {},
            QStringLiteral("mismatched-refinement-source"),
        },
        [](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderMetadataResult) { });
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
}

void TestImageViewportSequenceProvider::staleAuthoritativeSeedLoadsAndDecodesCurrentSource()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    const QUrl url(QStringLiteral("file:///tmp/stale-seed.png"));
    int decoderCalls = 0;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(dataLoader,
            [&decoderCalls](const QByteArray&, const kiriview::ImageDecodeRequest& request) {
                ++decoderCalls;
                kiriview::StaticDisplayImagePayload payload = displayPayload(
                    kiriview::DisplayImageQuality::Exact, {}, QSize(128, 64), Qt::blue);
                payload.sourceIdentity = kiriview::sourceKeyForUrl(request.imageUrl()).identity;
                payload.sourceRevision = request.sourceRevision();
                return kiriview::successfulDecodedImageResult(
                    kiriview::StaticDecodedImage { std::move(payload), {} });
            });
    dependencies.workerScheduler = workerScheduler.scheduler();
    kiriview::StaticDisplayImagePayload stale
        = displayPayload(kiriview::DisplayImageQuality::Exact, {}, QSize(128, 64), Qt::red);
    stale.sourceIdentity = kiriview::sourceKeyForUrl(url).identity;
    stale.sourceRevision
        = kiriview::ImageSourceRevision::fromData(QByteArrayView("stale-source-bytes"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(112,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies), std::move(stale));
    std::vector<kiriview::ImageViewportProviderFrameResult> results;

    source->requestFrame(
        kiriview::ImageViewportProviderWorkIdentity {
            112,
            ImageViewportPageRole::Primary,
            {},
            {},
            QStringLiteral("stale-seed"),
        },
        kiriview::ImageViewportProviderFrameRequest {
            0,
            {},
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            results.push_back(std::move(result));
        });

    QCOMPARE(results.size(), std::size_t(0));
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(QByteArrayLiteral("current-source-bytes"));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QCOMPARE(decoderCalls, 1);
    QCOMPARE(results.size(), std::size_t(1));
    QVERIFY(results.front().displayImage.has_value());
    QCOMPARE(results.front().displayImage->image.pixelColor(0, 0), QColor(Qt::blue));
    QCOMPARE(results.front().displayImage->sourceRevision,
        kiriview::ImageSourceRevision::fromData(QByteArrayView("current-source-bytes")));
}

void TestImageViewportSequenceProvider::
    seededSufficientFirstDisplayCompletesInitialWithoutRefinement()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    const QUrl url(QStringLiteral("file:///tmp/seeded-first-display.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(73,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource)));
    kiriview::ImageViewportProviderWorkIdentity identity {
        73,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("seeded-first-display"),
    };

    ImageSequenceProviderDisplayDemand demand;
    demand.setTargetDisplaySizePixels(QSizeF(160, 120));
    std::vector<kiriview::ImageViewportProviderFrameResult> results;
    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            demand,
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            results.push_back(std::move(result));
        });
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(results.size(), std::size_t(1));
    QVERIFY(results.front().displayImage.has_value());
    QVERIFY(!results.front().isProvisional());
    QCOMPARE(results.front().displayImage->quality, kiriview::DisplayImageQuality::BoundedDetail);
    QCOMPARE(results.front().displayImage->image.size(), QSize(200, 150));
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(0));
}

void TestImageViewportSequenceProvider::
    seededInsufficientFirstDisplayRefinesBeforeInitialTerminal_data()
{
    QTest::addColumn<QSizeF>("targetDisplaySizePixels");
    QTest::addColumn<int>("rotationDegrees");
    QTest::addColumn<double>("effectiveDevicePixelRatio");
    QTest::addColumn<QSize>("sourceSize");
    QTest::addColumn<QSize>("seedRasterSize");
    QTest::addColumn<QSize>("expectedRasterSize");

    QTest::newRow("unrotated") << QSizeF(400, 300) << 0 << 1.0 << QSize(800, 600) << QSize(200, 150)
                               << QSize(400, 300);
    QTest::newRow("quarter-turn") << QSizeF(300, 400) << 90 << 1.0 << QSize(800, 600)
                                  << QSize(200, 150) << QSize(400, 300);
    QTest::newRow("physical-target-already-includes-dpr")
        << QSizeF(400, 300) << 0 << 2.0 << QSize(800, 600) << QSize(200, 150) << QSize(400, 300);
    QTest::newRow("fractional-target-rounds-up")
        << QSizeF(400.1, 300.075) << 0 << 1.0 << QSize(800, 600) << QSize(200, 150)
        << QSize(401, 301);
    QTest::newRow("fractional-target-above-integer")
        << QSizeF(100.00000000000001, 100.00000000000001) << 0 << 1.0 << QSize(136, 136)
        << QSize(50, 50) << QSize(101, 101);
    QTest::newRow("source-size-cap")
        << QSizeF(1600, 1200) << 0 << 1.0 << QSize(800, 600) << QSize(200, 150) << QSize(800, 600);
}

void TestImageViewportSequenceProvider::seededInsufficientFirstDisplayRefinesBeforeInitialTerminal()
{
    QFETCH(QSizeF, targetDisplaySizePixels);
    QFETCH(int, rotationDegrees);
    QFETCH(double, effectiveDevicePixelRatio);
    QFETCH(QSize, sourceSize);
    QFETCH(QSize, seedRasterSize);
    QFETCH(QSize, expectedRasterSize);

    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>(sourceSize);
    const QUrl url(QStringLiteral("file:///tmp/seeded-first-display.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(73,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource, seedRasterSize)));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        73,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("seeded-first-display"),
    };
    ImageSequenceProviderDisplayDemand demand;
    demand.setTargetDisplaySizePixels(targetDisplaySizePixels);
    demand.setRotationDegrees(rotationDegrees);
    demand.setEffectiveDevicePixelRatio(effectiveDevicePixelRatio);
    std::vector<kiriview::ImageViewportProviderFrameResult> results;
    std::optional<kiriview::ImageViewportProviderWorkIdentity> completedIdentity;

    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            demand,
        },
        [&results, &completedIdentity](kiriview::ImageViewportProviderWorkIdentity completed,
            kiriview::ImageViewportProviderFrameResult result) {
            completedIdentity = completed;
            results.push_back(std::move(result));
        });
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(results.size(), std::size_t(0));
    QVERIFY(!completedIdentity.has_value());
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));

    workerScheduler.runWork(0);
    QCOMPARE(refinementSource->refinementCount, 1);
    QCOMPARE(refinementSource->lastRefinementSize, expectedRasterSize);
    workerScheduler.finish(0);

    QCOMPARE(results.size(), std::size_t(1));
    QVERIFY(completedIdentity.has_value());
    QVERIFY(*completedIdentity == identity);
    QVERIFY(results.back().displayImage.has_value());
    QVERIFY(!results.back().isProvisional());
    QCOMPARE(results.back().displayImage->quality,
        expectedRasterSize == sourceSize ? kiriview::DisplayImageQuality::Exact
                                         : kiriview::DisplayImageQuality::BoundedDetail);
    QCOMPARE(results.back().displayImage->image.size(), expectedRasterSize);
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
}

void TestImageViewportSequenceProvider::seededRequireExactRefinesFullSourceBeforeInitialTerminal()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    const QUrl url(QStringLiteral("file:///tmp/seeded-exact-detail.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(74,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(
            url, firstDisplayPayload(refinementSource, refinementSource->imageSize())));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        74,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("seeded-exact-detail"),
    };
    ImageSequenceProviderDisplayDemand demand;
    demand.setTargetDisplaySizePixels(QSizeF(160, 120));
    demand.setExactnessPreference(ImageViewportExactnessPreference::RequireExact);
    std::vector<kiriview::ImageViewportProviderFrameResult> results;

    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            demand,
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            results.push_back(std::move(result));
        });
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(results.size(), std::size_t(0));

    workerScheduler.runWork(0);
    QCOMPARE(refinementSource->lastRefinementSize, QSize(800, 600));
    workerScheduler.finish(0);

    QCOMPARE(results.size(), std::size_t(1));
    QVERIFY(results.front().displayImage.has_value());
    QCOMPARE(results.front().displayImage->quality, kiriview::DisplayImageQuality::Exact);
    QCOMPARE(results.front().displayImage->image.size(), QSize(800, 600));
}

void TestImageViewportSequenceProvider::scalableSourceRefinesBeyondIntrinsicRaster()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::TestSupport::ManualTimerScheduler timerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    QString errorString;
    const auto refinementSource = kiriview::SvgDisplaySource::open(
        QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"20\" height=\"10\">"
                          "<rect width=\"20\" height=\"10\" fill=\"red\"/></svg>"),
        &errorString);
    QVERIFY2(refinementSource != nullptr, qPrintable(errorString));
    const kiriview::StaticImageDisplayDecodeResult intrinsic
        = refinementSource->decodeRasterDisplayImage(QSize(20, 10));
    QVERIFY(!intrinsic.image.isNull());
    kiriview::StaticDisplayImagePayload seed {
        QStringLiteral("scalable-source"),
        {},
        refinementSource->imageSize(),
        intrinsic.image,
        kiriview::DisplayImageQuality::FirstDisplay,
        {},
        refinementSource,
        kiriview::DisplayImagePreviewOrigin::None,
        refinementSource->detailModel(),
    };
    const QUrl url(QStringLiteral("file:///tmp/scalable-source.svg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(79,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies), authoritativeSeedForUrl(url, std::move(seed)),
        timerScheduler.scheduler());
    const kiriview::ImageViewportProviderWorkIdentity identity {
        79,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("scalable-source"),
    };
    ImageSequenceProviderDisplayDemand demand;
    demand.setTargetDisplaySizePixels(QSizeF(80, 40));
    std::vector<kiriview::ImageViewportProviderFrameResult> results;

    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            demand,
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            results.push_back(std::move(result));
        });

    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(results.size(), std::size_t(0));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);

    QCOMPARE(results.size(), std::size_t(1));
    QVERIFY(results.front().displayImage.has_value());
    QCOMPARE(results.front().displayImage->image.size(), QSize(80, 40));
    QCOMPARE(results.front().displayImage->quality, kiriview::DisplayImageQuality::BoundedDetail);
}

void TestImageViewportSequenceProvider::scalableSourceRejectsRequireExact()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>(
        QSize(20, 10), kiriview::StaticImageSourceDetailModel::ScalableRasterization);
    const QUrl url(QStringLiteral("file:///tmp/scalable-require-exact.svg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(80,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource, QSize(20, 10))));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        80,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("scalable-require-exact"),
    };
    ImageSequenceProviderDisplayDemand demand;
    demand.setTargetDisplaySizePixels(QSizeF(80, 40));
    demand.setExactnessPreference(ImageViewportExactnessPreference::RequireExact);
    std::vector<kiriview::ImageViewportProviderFrameResult> results;

    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            demand,
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            results.push_back(std::move(result));
        });

    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(results.size(), std::size_t(1));
    QCOMPARE(results.front().unsupportedCause,
        std::optional(ImageSequenceProviderUnsupportedCause::PayloadRejection));
    QVERIFY(!results.front().displayImage.has_value());
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(0));
}

void TestImageViewportSequenceProvider::payloadRejectionMapsThroughProvider()
{
    ProviderFixture fixture;
    fixture.source->knownMetadata = ImageSequenceProviderMetadata::still(QSizeF(128, 64));
    fixture.source->automaticFrame = kiriview::ImageViewportProviderFrameResult::unsupported(
        ImageSequenceProviderUnsupportedCause::PayloadRejection);
    fixture.create();

    ImageViewport viewport;
    fixture.assign(viewport);

    QTRY_COMPARE(viewport.state().request().status(), ImageViewportRequestStatus::Unsupported);
    QCOMPARE(viewport.state().request().reason(), ImageViewportRequestReason::PayloadRejection);
}

void TestImageViewportSequenceProvider::publicAndPrivatePayloadLimitsHaveDistinctOutcomes()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    const QUrl url(QStringLiteral("file:///tmp/distinct-payload-limits.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(81,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(
            url, firstDisplayPayload(refinementSource, refinementSource->imageSize())));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        81,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("distinct-payload-limits"),
    };
    ImageSequenceProviderDisplayDemand publicDemand;
    publicDemand.setExactnessPreference(ImageViewportExactnessPreference::RequireExact);
    publicDemand.setDisplayByteBudget(100 * 75 * 4);
    std::vector<kiriview::ImageViewportProviderFrameResult> publicResults;

    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            publicDemand,
        },
        [&publicResults](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            publicResults.push_back(std::move(result));
        });

    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(publicResults.size(), std::size_t(1));
    QCOMPARE(publicResults.front().unsupportedCause,
        std::optional(ImageSequenceProviderUnsupportedCause::PayloadRejection));

    ImageSequenceProviderDisplayDemand privateDemand;
    privateDemand.setExactnessPreference(ImageViewportExactnessPreference::RequireExact);
    std::vector<kiriview::ImageViewportProviderFrameResult> privateResults;
    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            privateDemand,
            100 * 75 * 4,
        },
        [&privateResults](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            privateResults.push_back(std::move(result));
        });

    QCOMPARE(privateResults.size(), std::size_t(1));
    QCOMPARE(
        privateResults.front().failureCause, ImageSequenceProviderFailureCause::ResourceExhausted);
    QVERIFY(!privateResults.front().unsupportedCause.has_value());
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(0));
}

void TestImageViewportSequenceProvider::actualRefinementByteCostPreservesLimitOutcome_data()
{
    QTest::addColumn<bool>("privateLimit");

    QTest::newRow("public-demand-limit") << false;
    QTest::newRow("private-store-limit") << true;
}

void TestImageViewportSequenceProvider::actualRefinementByteCostPreservesLimitOutcome()
{
    QFETCH(bool, privateLimit);

    constexpr qint64 tightPayloadBytes = 100 * 75 * 4;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    refinementSource->refinementBytesPerLine = 512;
    const QUrl url(QStringLiteral("file:///tmp/actual-refinement-byte-cost.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(84,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource, QSize(40, 30))));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        84,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("actual-refinement-byte-cost"),
    };
    ImageSequenceProviderDisplayDemand demand;
    demand.setTargetDisplaySizePixels(QSizeF(100, 75));
    demand.setCurrentPayloadQuality(ImageViewportPayloadQuality::FirstDisplay);
    demand.setCurrentPayloadExactness(ImageViewportPayloadExactness::NotExact);
    if (!privateLimit) {
        demand.setDisplayByteBudget(tightPayloadBytes);
    }
    std::vector<kiriview::ImageViewportProviderFrameResult> results;

    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            demand,
            privateLimit ? tightPayloadBytes : -1,
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            results.push_back(std::move(result));
        });
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(results.size(), std::size_t(0));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);

    QCOMPARE(results.size(), std::size_t(1));
    if (privateLimit) {
        QCOMPARE(
            results.front().failureCause, ImageSequenceProviderFailureCause::ResourceExhausted);
        QVERIFY(!results.front().unsupportedCause.has_value());
    } else {
        QCOMPARE(results.front().unsupportedCause,
            std::optional(ImageSequenceProviderUnsupportedCause::PayloadRejection));
    }
}

void TestImageViewportSequenceProvider::refinementFailureIsNotReclassifiedAsResourceExhaustion()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    refinementSource->failRefinement = true;
    const QUrl url(QStringLiteral("file:///tmp/refinement-failure-cause.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(82,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource)));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        82,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("refinement-failure-cause"),
    };
    ImageSequenceProviderDisplayDemand demand;
    demand.setTargetDisplaySizePixels(QSizeF(400, 300));
    std::vector<kiriview::ImageViewportProviderFrameResult> results;

    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            demand,
            100 * 75 * 4,
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            results.push_back(std::move(result));
        });
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(results.size(), std::size_t(0));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);

    QCOMPARE(results.size(), std::size_t(1));
    QCOMPARE(results.front().failureCause, ImageSequenceProviderFailureCause::Decode);
    QVERIFY(results.front().failure.has_value());
    QCOMPARE(results.front().failure->kind, kiriview::ImageLoadFailureKind::Decode);
    QCOMPARE(results.front().failure->diagnosticDetail, QStringLiteral("fake refinement failed"));
}

void TestImageViewportSequenceProvider::invalidRefinementGeometryIsProviderInternal()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    refinementSource->refinementResultSize = QSize(800, 200);
    const QUrl url(QStringLiteral("file:///tmp/invalid-refinement-geometry.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(83,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource)));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        83,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("invalid-refinement-geometry"),
    };
    ImageSequenceProviderDisplayDemand demand;
    demand.setTargetDisplaySizePixels(QSizeF(400, 300));
    std::vector<kiriview::ImageViewportProviderFrameResult> results;

    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            demand,
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            results.push_back(std::move(result));
        });
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(results.size(), std::size_t(0));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);

    QCOMPARE(results.size(), std::size_t(1));
    QCOMPARE(results.front().failureCause, ImageSequenceProviderFailureCause::ProviderInternal);
    QVERIFY(results.front().failure.has_value());
    QCOMPARE(results.front().failure->kind, kiriview::ImageLoadFailureKind::Presentation);
}

void TestImageViewportSequenceProvider::initialDetailWaitPublishesFirstDisplayFallbackOnce()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::TestSupport::ManualTimerScheduler timerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    const QUrl url(QStringLiteral("file:///tmp/seeded-initial-detail-timeout.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(75,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource)),
        timerScheduler.scheduler());
    const kiriview::ImageViewportProviderWorkIdentity identity {
        75,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("seeded-initial-detail-timeout"),
    };
    ImageSequenceProviderDisplayDemand demand;
    demand.setTargetDisplaySizePixels(QSizeF(400, 300));
    std::vector<kiriview::ImageViewportProviderFrameResult> results;
    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            demand,
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            results.push_back(std::move(result));
        });

    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(results.size(), std::size_t(0));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(timerScheduler.timerCount(), std::size_t(1));
    QVERIFY(timerScheduler.timerAt(0).active());

    timerScheduler.timerAt(0).fire();
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QCoreApplication::processEvents();
    QCOMPARE(results.size(), std::size_t(1));
    QVERIFY(results.front().displayImage.has_value());
    QCOMPARE(results.front().displayImage->quality, kiriview::DisplayImageQuality::FirstDisplay);
    QCOMPARE(results.front().displayImage->image.size(), QSize(200, 150));

    QCOMPARE(results.size(), std::size_t(1));
}

void TestImageViewportSequenceProvider::initialDetailDeadlineInvalidationDropsQueuedFallback()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::TestSupport::ManualTimerScheduler timerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    const QUrl url(QStringLiteral("file:///tmp/invalidated-initial-detail-timeout.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(76,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource)),
        timerScheduler.scheduler());
    const kiriview::ImageViewportProviderWorkIdentity identity {
        76,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("invalidated-initial-detail-timeout"),
    };
    ImageSequenceProviderDisplayDemand demand;
    demand.setTargetDisplaySizePixels(QSizeF(400, 300));
    int completionCount = 0;
    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            demand,
        },
        [&completionCount](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult) { ++completionCount; });

    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(timerScheduler.timerCount(), std::size_t(1));
    timerScheduler.timerAt(0).fire();
    source->cancel({ identity.requestToken });
    QCoreApplication::processEvents();

    QCOMPARE(completionCount, 0);
    QVERIFY(!workerScheduler.isActive(0));
}

void TestImageViewportSequenceProvider::newerInitialRefinementBoundsTimedOutWork()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::TestSupport::ManualTimerScheduler timerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    const QUrl url(QStringLiteral("file:///tmp/bounded-timed-out-refinement.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(77,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource)),
        timerScheduler.scheduler());
    const kiriview::ImageViewportProviderWorkIdentity identity {
        77,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("bounded-timed-out-refinement"),
    };
    const auto request = [&source, &identity](QSize targetSize, int& completionCount) {
        ImageSequenceProviderDisplayDemand demand;
        demand.setTargetDisplaySizePixels(targetSize);
        source->requestFrame(identity,
            kiriview::ImageViewportProviderFrameRequest {
                0,
                demand,
            },
            [&completionCount](kiriview::ImageViewportProviderWorkIdentity,
                kiriview::ImageViewportProviderFrameResult) { ++completionCount; });
    };
    int completionCount = 0;

    request(QSize(300, 225), completionCount);
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    timerScheduler.timerAt(0).fire();
    QCoreApplication::processEvents();
    QCOMPARE(completionCount, 1);
    QVERIFY(workerScheduler.isActive(0));

    request(QSize(400, 300), completionCount);
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    QVERIFY(!workerScheduler.isActive(0));
    QCOMPARE(completionCount, 1);
    source->close();
}

void TestImageViewportSequenceProvider::synchronousInitialRefinementCompletesExactlyOnce()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler
        = kiriview::ImageWorkerScheduler([](QObject*, kiriview::ImageWorkerOperation work,
                                             kiriview::ImageWorkerCompletion completion) {
              work();
              completion();
              return kiriview::ImageWorkerTask {};
          });
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    const QUrl url(QStringLiteral("file:///tmp/synchronous-initial-refinement.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(78,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource)));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        78,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("synchronous-initial-refinement"),
    };
    ImageSequenceProviderDisplayDemand demand;
    demand.setTargetDisplaySizePixels(QSizeF(400, 300));
    std::vector<kiriview::ImageViewportProviderFrameResult> results;

    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            demand,
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            results.push_back(std::move(result));
        });

    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(results.size(), std::size_t(1));
    QVERIFY(results.front().displayImage.has_value());
    QCOMPARE(results.front().displayImage->quality, kiriview::DisplayImageQuality::BoundedDetail);
    QCOMPARE(results.front().displayImage->image.size(), QSize(400, 300));
}

void TestImageViewportSequenceProvider::initialRefinementRespectsDisplayStoreEntryBudget()
{
    constexpr qsizetype displayStoreEntryBudget = 300 * 225 * 4;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    const QUrl url(QStringLiteral("file:///tmp/seeded-display-store-budget.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(76,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource)));
    auto resource = std::make_shared<kiriview::ImageViewportProviderResource>(76,
        QStringLiteral("seeded-display-store-budget"), source,
        std::make_shared<kiriview::DisplayImageStore>(displayStoreEntryBudget));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        76,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("seeded-display-store-budget"),
    };
    ImageSequenceProviderDisplayDemand demand;
    demand.setTargetDisplaySizePixels(QSizeF(400, 300));
    std::vector<kiriview::ImageViewportProviderPreparedFrame> results;
    resource->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            demand,
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderPreparedFrame result) {
            results.push_back(std::move(result));
        });

    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(results.size(), std::size_t(0));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    QCOMPARE(refinementSource->lastRefinementSize, QSize(300, 225));
    workerScheduler.finish(0);

    QCOMPARE(results.size(), std::size_t(1));
    QVERIFY(results.front().isReady());
    QVERIFY(results.front().authoritativeStillDisplayImage.has_value());
    QCOMPARE(results.front().authoritativeStillDisplayImage->quality,
        kiriview::DisplayImageQuality::BoundedDetail);
    QCOMPARE(results.front().authoritativeStillDisplayImage->image.size(), QSize(300, 225));
}

void TestImageViewportSequenceProvider::concurrentRefinementUsesRequestAdmissibleWorkerResult()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    const QUrl url(QStringLiteral("file:///tmp/concurrent-refinement-budgets.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(77,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource)));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        77,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("concurrent-refinement-budgets"),
    };
    const auto request = [&source, &identity](QSize targetSize, qint64 byteBudget,
                             std::vector<kiriview::ImageViewportProviderFrameResult>& results) {
        ImageSequenceProviderDisplayDemand demand;
        demand.setTargetDisplaySizePixels(targetSize);
        demand.setCurrentPayloadQuality(ImageViewportPayloadQuality::FirstDisplay);
        demand.setCurrentPayloadExactness(ImageViewportPayloadExactness::NotExact);
        source->requestFrame(identity,
            kiriview::ImageViewportProviderFrameRequest {
                0,
                demand,
                byteBudget,
            },
            [&results](kiriview::ImageViewportProviderWorkIdentity,
                kiriview::ImageViewportProviderFrameResult result) {
                results.push_back(std::move(result));
            });
    };
    std::vector<kiriview::ImageViewportProviderFrameResult> largerResults;
    std::vector<kiriview::ImageViewportProviderFrameResult> smallerResults;

    request(QSize(400, 300), 400 * 300 * 4, largerResults);
    request(QSize(300, 225), 300 * 225 * 4, smallerResults);
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(0);
    workerScheduler.runWork(1);
    workerScheduler.finish(0);
    workerScheduler.finish(1);

    QCOMPARE(largerResults.size(), std::size_t(1));
    QVERIFY(largerResults.front().displayImage.has_value());
    QCOMPARE(largerResults.front().displayImage->image.size(), QSize(400, 300));
    QCOMPARE(smallerResults.size(), std::size_t(1));
    QVERIFY(smallerResults.front().displayImage.has_value());
    QCOMPARE(
        smallerResults.front().displayImage->quality, kiriview::DisplayImageQuality::BoundedDetail);
    QCOMPARE(smallerResults.front().displayImage->image.size(), QSize(300, 225));
}

void TestImageViewportSequenceProvider::provisionalFrameDoesNotBecomeCurrentStillDisplayImage()
{
    ProviderFixture fixture;
    fixture.source->knownMetadata = ImageSequenceProviderMetadata::still(QSizeF(128, 64));
    fixture.create();

    kiriview::ImageViewportProviderWorkIdentity identity {
        41,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("fake-location"),
    };
    std::vector<kiriview::ImageViewportProviderPreparedFrame> prepared;
    fixture.resource->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            {},
        },
        [&prepared](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderPreparedFrame result) {
            prepared.push_back(std::move(result));
        });
    QCOMPARE(fixture.source->pendingFrames.size(), std::size_t(1));

    fixture.source->emitNextProvisionalFrame(
        kiriview::ImageViewportProviderFrameResult::provisional(
            displayPayload(kiriview::DisplayImageQuality::ThumbnailPreview),
            ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("jpg")));
    QCOMPARE(prepared.size(), std::size_t(1));
    QVERIFY(prepared.front().isReady());
    QVERIFY(prepared.front().isProvisional());
    QVERIFY(!fixture.resource->currentStillDisplayImage(identity.demandRevision).has_value());
    QCOMPARE(fixture.source->pendingFrames.size(), std::size_t(1));

    fixture.source->completeNextFrame(kiriview::ImageViewportProviderFrameResult::ready(
        displayPayload(kiriview::DisplayImageQuality::Exact),
        ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("jpg")));
    QCOMPARE(prepared.size(), std::size_t(2));
    QVERIFY(prepared.back().isReady());
    QVERIFY(!prepared.back().isProvisional());
    QVERIFY(!fixture.resource->currentStillDisplayImage(identity.demandRevision).has_value());
}

void TestImageViewportSequenceProvider::authoritativeStillPayloadLookupKeepsDisplayedRevision()
{
    ProviderFixture fixture;
    fixture.source->knownMetadata = ImageSequenceProviderMetadata::still(QSizeF(128, 64));
    fixture.create();

    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    fixture.assign(viewport);
    QTRY_COMPARE(fixture.source->pendingFrames.size(), std::size_t(1));
    const kiriview::ImageViewportProviderWorkIdentity initialIdentity
        = fixture.source->pendingFrames.front().identity;

    fixture.source->completeNextFrame(kiriview::ImageViewportProviderFrameResult::ready(
        displayPayload(kiriview::DisplayImageQuality::Exact, QSize(128, 64), QSize(128, 64)),
        ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("png")));
    QVERIFY(driveRenderUntil(window, [&viewport]() {
        return viewport.state().request().status() == ImageViewportRequestStatus::Ready;
    }));
    const ImageViewportDemandRevisionToken displayedRevision
        = viewport.state().primary().display().demandRevision();
    QCOMPARE(displayedRevision, initialIdentity.demandRevision);
    QVERIFY(!fixture.resource->currentStillDisplayImage(displayedRevision).has_value());
    QVERIFY(fixture.resource->acceptDisplayedStillDisplayImage(
        ImageViewportPageRole::Primary, displayedRevision));
    const std::optional<kiriview::StaticDisplayImagePayload> displayed
        = fixture.resource->currentStillDisplayImage(displayedRevision);
    QVERIFY(displayed.has_value());
    QCOMPARE(displayed->quality, kiriview::DisplayImageQuality::Exact);

    ImageViewportPresentationCommand command;
    command.setExactnessPreference(ImageViewportExactnessPreference::RequireExact);
    QCOMPARE(viewport.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
    QTRY_COMPARE(fixture.source->pendingFrames.size(), std::size_t(1));
    const kiriview::ImageViewportProviderWorkIdentity rejectedIdentity
        = fixture.source->pendingFrames.front().identity;
    QVERIFY(rejectedIdentity.demandRevision != displayedRevision);

    fixture.source->completeNextFrame(kiriview::ImageViewportProviderFrameResult::ready(
        displayPayload(kiriview::DisplayImageQuality::BoundedDetail, QSize(64, 32), QSize(128, 64)),
        ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("png")));
    QCoreApplication::processEvents();

    QCOMPARE(viewport.state().primary().display().demandRevision(), displayedRevision);
    const std::optional<kiriview::StaticDisplayImagePayload> preserved
        = fixture.resource->currentStillDisplayImage(displayedRevision);
    QVERIFY(preserved.has_value());
    QCOMPARE(preserved->quality, kiriview::DisplayImageQuality::Exact);
    QCOMPARE(preserved->image.size(), QSize(128, 64));

    fixture.resource->close();
    QVERIFY(fixture.resource->currentStillDisplayImage(displayedRevision).has_value());
}

void TestImageViewportSequenceProvider::metadataCompletionAfterInvalidationIsDropped_data()
{
    QTest::addColumn<bool>("closeResource");

    QTest::newRow("cancel") << false;
    QTest::newRow("close") << true;
}

void TestImageViewportSequenceProvider::metadataCompletionAfterInvalidationIsDropped()
{
    QFETCH(bool, closeResource);

    ProviderFixture tokenFixture;
    tokenFixture.source->knownMetadata = ImageSequenceProviderMetadata::still(QSizeF(32, 16));
    tokenFixture.create();
    ImageViewport tokenViewport;
    tokenFixture.assign(tokenViewport);
    QTRY_COMPARE(tokenFixture.source->frameIdentities.size(), std::size_t(1));
    const ImageSequenceProviderRequestToken requestToken
        = tokenFixture.source->frameIdentities.front().requestToken;
    QVERIFY(requestToken.isValid());

    auto source = std::make_shared<FakeImageViewportProviderSource>();
    auto resource = std::make_shared<kiriview::ImageViewportProviderResource>(81,
        QStringLiteral("metadata-lifecycle"), source,
        std::make_shared<kiriview::DisplayImageStore>(1024 * 1024));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        81,
        ImageViewportPageRole::Primary,
        requestToken,
        {},
        QStringLiteral("metadata-lifecycle"),
    };
    int completionCount = 0;
    resource->requestMetadata(identity,
        [&completionCount](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderMetadataResult) { ++completionCount; });
    QCOMPARE(source->pendingMetadata.size(), std::size_t(1));

    if (closeResource) {
        resource->close();
    } else {
        resource->cancel({ requestToken });
    }
    source->completeNextMetadata(kiriview::ImageViewportProviderMetadataResult::ready(
        ImageSequenceProviderMetadata::still(QSizeF(32, 16))));

    QCOMPARE(completionCount, 0);
}

void TestImageViewportSequenceProvider::refinementWorkerCompletionAndInvalidationAreOwnedBySource()
{
    ProviderFixture tokenFixture;
    tokenFixture.source->knownMetadata = ImageSequenceProviderMetadata::still(QSizeF(800, 600));
    tokenFixture.create();
    ImageViewport tokenViewport;
    tokenFixture.assign(tokenViewport);
    QTRY_COMPARE(tokenFixture.source->frameIdentities.size(), std::size_t(1));
    const ImageSequenceProviderRequestToken requestToken
        = tokenFixture.source->frameIdentities.front().requestToken;
    QVERIFY(requestToken.isValid());

    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    const QUrl url(QStringLiteral("file:///tmp/refinement-lifecycle.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(74,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource)));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        74,
        ImageViewportPageRole::Primary,
        requestToken,
        {},
        QStringLiteral("refinement-lifecycle"),
    };
    const auto request = [](QSize targetSize) {
        ImageSequenceProviderDisplayDemand refinementDemand;
        refinementDemand.setTargetDisplaySizePixels(targetSize);
        refinementDemand.setCurrentPayloadQuality(ImageViewportPayloadQuality::FirstDisplay);
        refinementDemand.setCurrentPayloadExactness(ImageViewportPayloadExactness::NotExact);
        return kiriview::ImageViewportProviderFrameRequest {
            0,
            refinementDemand,
        };
    };
    int completionCount = 0;

    source->requestFrame(identity, request(QSize(300, 225)),
        [&completionCount](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult) { ++completionCount; });
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QCOMPARE(completionCount, 1);
    QVERIFY(!workerScheduler.isActive(0));

    source->requestFrame(identity, request(QSize(400, 300)),
        [&completionCount](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult) { ++completionCount; });
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);
    source->cancel({ identity.requestToken });
    QVERIFY(!workerScheduler.isActive(1));
    workerScheduler.finish(1);
    QCOMPARE(completionCount, 1);

    source->requestFrame(identity, request(QSize(500, 375)),
        [&completionCount](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult) { ++completionCount; });
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(3));
    workerScheduler.runWork(2);
    source->close();
    QVERIFY(!workerScheduler.isActive(2));
    workerScheduler.finish(2);
    QCOMPARE(completionCount, 1);
}

void TestImageViewportSequenceProvider::workerUnitIdentitySkipsLiveIdsAfterWrap()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    const QUrl url(QStringLiteral("file:///tmp/refinement-worker-id-wrap.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(75,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource)));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        75,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("refinement-worker-id-wrap"),
    };
    const auto request = [&source, &identity](QSize targetSize, int& completionCount) {
        ImageSequenceProviderDisplayDemand demand;
        demand.setTargetDisplaySizePixels(targetSize);
        demand.setCurrentPayloadQuality(ImageViewportPayloadQuality::FirstDisplay);
        demand.setCurrentPayloadExactness(ImageViewportPayloadExactness::NotExact);
        source->requestFrame(identity,
            kiriview::ImageViewportProviderFrameRequest {
                0,
                demand,
            },
            [&completionCount](kiriview::ImageViewportProviderWorkIdentity,
                kiriview::ImageViewportProviderFrameResult) { ++completionCount; });
    };
    int completionCount = 0;

    kiriview::ImageViewportDecodeProviderSourceTestAccess::setNextWorkerUnitId(*source, 1);
    request(QSize(300, 225), completionCount);
    kiriview::ImageViewportDecodeProviderSourceTestAccess::setNextWorkerUnitId(
        *source, std::numeric_limits<quint64>::max());
    request(QSize(400, 300), completionCount);
    request(QSize(500, 375), completionCount);

    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(3));
    QVERIFY(workerScheduler.isActive(0));
    QVERIFY(workerScheduler.isActive(1));
    QVERIFY(workerScheduler.isActive(2));

    for (std::size_t index = 0; index < workerScheduler.scheduleCount(); ++index) {
        workerScheduler.runWork(index);
        workerScheduler.finish(index);
    }
    QCOMPARE(completionCount, 3);
}

void TestImageViewportSequenceProvider::
    outOfOrderRefinementCannotDowngradeReusableAuthoritativeImage()
{
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = kiriview::TestSupport::imageDecodeDependenciesFor(
            dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    auto refinementSource = std::make_shared<RefiningDisplaySource>();
    const QUrl url(QStringLiteral("file:///tmp/refinement-order.jpg"));
    auto source = std::make_shared<kiriview::ImageViewportDecodeProviderSource>(
        kiriview::ImageLoadSession(76,
            kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(url, {})),
            kiriview::DisplayedImageLocation::fromUrl(url)),
        std::move(dependencies),
        authoritativeSeedForUrl(url, firstDisplayPayload(refinementSource)));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        76,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("refinement-order"),
    };
    const auto requestRefinement
        = [&source, &identity](QSize targetSize,
              std::vector<kiriview::ImageViewportProviderFrameResult>& completions) {
              ImageSequenceProviderDisplayDemand demand;
              demand.setTargetDisplaySizePixels(targetSize);
              demand.setCurrentPayloadQuality(ImageViewportPayloadQuality::FirstDisplay);
              demand.setCurrentPayloadExactness(ImageViewportPayloadExactness::NotExact);
              source->requestFrame(identity,
                  kiriview::ImageViewportProviderFrameRequest {
                      0,
                      demand,
                  },
                  [&completions](kiriview::ImageViewportProviderWorkIdentity,
                      kiriview::ImageViewportProviderFrameResult result) {
                      completions.push_back(std::move(result));
                  });
          };
    std::vector<kiriview::ImageViewportProviderFrameResult> completions;

    requestRefinement(QSize(800, 600), completions);
    requestRefinement(QSize(400, 300), completions);
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(authoritativeSeedData());
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(0);
    workerScheduler.runWork(1);
    workerScheduler.finish(0);
    workerScheduler.finish(1);

    QCOMPARE(completions.size(), std::size_t(2));
    QVERIFY(completions.at(0).displayImage.has_value());
    QVERIFY(completions.at(1).displayImage.has_value());
    QCOMPARE(completions.at(0).displayImage->quality, kiriview::DisplayImageQuality::Exact);

    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            {},
        },
        [&completions](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            completions.push_back(std::move(result));
        });
    QCOMPARE(completions.size(), std::size_t(3));
    QVERIFY(completions.back().displayImage.has_value());
    QCOMPARE(completions.back().displayImage->quality, kiriview::DisplayImageQuality::Exact);
    QCOMPARE(completions.back().displayImage->image.size(), QSize(800, 600));
}

void TestImageViewportSequenceProvider::animationDemandRequestsOnlyTheSelectedFrame()
{
    ProviderFixture fixture;
    fixture.source->knownMetadata = ImageSequenceProviderMetadata::timedFrameList(
        QSizeF(48, 24), QVector<int> { 30, 40, 50 });
    QCOMPARE(fixture.source->knownMetadata.frameDurations(), QVector<int>({ 30, 40, 50 }));
    fixture.source->automaticFrame = kiriview::ImageViewportProviderFrameResult::ready(
        displayPayload(kiriview::DisplayImageQuality::Exact, QSize(48, 24), QSize(48, 24)),
        ImageSequenceProviderFrameEnvelope::timedFrame(0, 0, 30), QStringLiteral("gif"));
    fixture.create();

    ImageViewport viewport;
    fixture.assign(viewport);
    QTRY_COMPARE(fixture.source->frameRequests.size(), std::size_t(1));
    QCOMPARE(fixture.source->frameRequests.back().frame, 0);

    QCOMPARE(viewport.seek(ImageViewportPageRole::Primary, 2).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QTRY_COMPARE(fixture.source->frameRequests.size(), std::size_t(2));
    QCOMPARE(fixture.source->frameRequests.back().frame, 2);
}

void TestImageViewportSequenceProvider::staleCompletionMayCacheButCannotPublish()
{
    ProviderFixture fixture;
    fixture.source->knownMetadata = ImageSequenceProviderMetadata::still(QSizeF(128, 64));
    fixture.create();

    ImageViewport viewport;
    fixture.assign(viewport);
    QTRY_COMPARE(fixture.source->pendingFrames.size(), std::size_t(1));

    QCOMPARE(viewport.clear().outcome(), ImageViewportCommandOutcome::Accepted);
    QTRY_VERIFY(fixture.source->closeCount > 0);
    fixture.source->completeNextFrame(kiriview::ImageViewportProviderFrameResult::ready(
        displayPayload(kiriview::DisplayImageQuality::Exact),
        ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("png")));

    QTRY_COMPARE(fixture.store->size(), qsizetype(1));
    QCOMPARE(viewport.state().request().status(), ImageViewportRequestStatus::NoRequest);
    QCOMPARE(viewport.state().display().status(), ImageViewportDisplayStatus::Empty);
}

void TestImageViewportSequenceProvider::componentFrameHandlePinsStoreUntilRelease()
{
    constexpr qsizetype frameByteCost = 32 * 16 * 4;
    ProviderFixture fixture(frameByteCost);
    fixture.source->knownMetadata = ImageSequenceProviderMetadata::still(QSizeF(128, 64));
    fixture.source->automaticFrame = kiriview::ImageViewportProviderFrameResult::ready(
        displayPayload(kiriview::DisplayImageQuality::BoundedDetail),
        ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("png"));
    fixture.create();

    ImageViewport viewport;
    fixture.assign(viewport);
    QTRY_COMPARE(fixture.store->size(), qsizetype(1));

    const auto acquireCompetingEntry = [&fixture]() {
        const kiriview::StaticDisplayImagePayload payload
            = displayPayload(kiriview::DisplayImageQuality::BoundedDetail);
        return fixture.store->acquireReusable(
            kiriview::DisplayImageEntry {
                payload.image,
                payload.originalSize,
                payload.image.size(),
                payload.quality,
                kiriview::DisplayImageRetentionPriority::Visible,
            },
            kiriview::DisplayImageReuseKey {
                QStringLiteral("competing-reuse"),
                QStringLiteral("competing-source"),
                payload.sourceRevision,
                kiriview::DisplayImageRasterIdentity::authoritativeStill(),
                {},
                payload.originalSize,
                payload.image.size(),
                payload.quality,
                payload.previewOrigin,
                kiriview::DisplayedPageRole::Primary,
            });
    };

    QVERIFY(acquireCompetingEntry().isEmpty());
    QCOMPARE(fixture.store->size(), qsizetype(1));

    QCOMPARE(viewport.clear().outcome(), ImageViewportCommandOutcome::Accepted);
    QTRY_VERIFY(!acquireCompetingEntry().isEmpty());
    QCOMPARE(fixture.store->size(), qsizetype(1));
}

void TestImageViewportSequenceProvider::failureReferenceResolvesAndRetiresWithHandle()
{
    ProviderFixture fixture;
    fixture.source->automaticMetadata = kiriview::ImageViewportProviderMetadataResult::failed(
        ImageSequenceProviderFailureCause::Decode, loadFailure());
    fixture.create();

    ImageViewport viewport;
    fixture.assign(viewport);

    QTRY_COMPARE(viewport.state().request().status(), ImageViewportRequestStatus::Error);
    const ImageViewportFailureSnapshot failure = viewport.state().diagnostics().failure();
    QVERIFY(failure.available());
    QCOMPARE(failure.providerCause(), ImageSequenceProviderFailureCause::Decode);
    QVERIFY(failure.providerReference().isValid());
    const std::optional<kiriview::ImageLoadFailure> resolved
        = fixture.failures->resolve(failure.providerReference());
    QVERIFY(resolved.has_value());
    QCOMPARE(resolved->sessionId, quint64(17));
    QCOMPARE(resolved->diagnosticDetail, QStringLiteral("fake decoder rejected the payload"));
    QCOMPARE(fixture.failures->size(), qsizetype(1));

    QCOMPARE(viewport.clear().outcome(), ImageViewportCommandOutcome::Accepted);
    QTRY_COMPARE(fixture.failures->size(), qsizetype(0));
}

QTEST_MAIN(TestImageViewportSequenceProvider)

#include "tst_imageviewportsequenceprovider.moc"
