// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "image_test_support.h"
#include "location/imageurl.h"
#include "rendering/imageviewportdecodesource.h"
#include "rendering/imageviewportsequenceprovider.h"

#include <ImageViewport/imagesequence.h>
#include <ImageViewport/imageviewport.h>

#include <QBuffer>
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
kiriview::StaticDisplayImagePayload displayPayload(kiriview::DisplayImageQuality quality,
    QSize rasterSize = QSize(32, 16), QSize originalSize = QSize(128, 64))
{
    QImage image(rasterSize, QImage::Format_RGBA8888);
    image.fill(QColor(20, 40, 60, 255));
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
    QSize imageSize() const override { return QSize(800, 600); }
    qsizetype byteCost() const override { return 800 * 600 * 4; }
    bool supportsRasterDisplayRefinement() const override { return true; }

    kiriview::StaticImageDisplayDecodeResult decodeRasterDisplayImage(
        const QSize& rasterSize) const override
    {
        ++refinementCount;
        lastRefinementSize = rasterSize;
        QImage image(rasterSize, QImage::Format_RGBA8888);
        image.fill(QColor(Qt::red));
        return { std::move(image), {} };
    }

    kiriview::StaticImageDisplayDecodeResult decodeBlockingDisplayImage(int) const override
    {
        return {};
    }

    mutable int refinementCount = 0;
    mutable QSize lastRefinementSize;
};

kiriview::StaticDisplayImagePayload firstDisplayPayload(
    const std::shared_ptr<RefiningDisplaySource>& refinementSource)
{
    QImage image(200, 150, QImage::Format_RGBA8888);
    image.fill(QColor(Qt::yellow));
    return kiriview::StaticDisplayImagePayload {
        QStringLiteral("seeded-first-display"),
        {},
        QSize(800, 600),
        std::move(image),
        kiriview::DisplayImageQuality::FirstDisplay,
        {},
        refinementSource,
        kiriview::DisplayImagePreviewOrigin::None,
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
    void foregroundThumbnailRemainsProvisionalUntilAuthoritativeTerminal_data();
    void foregroundThumbnailRemainsProvisionalUntilAuthoritativeTerminal();
    void readerOrientationProducesNormalizedFrame();
    void seededFirstDisplayUsesDecodeSourceForInitialAndRefinementDemand();
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

void TestImageViewportSequenceProvider::
    seededFirstDisplayUsesDecodeSourceForInitialAndRefinementDemand()
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
        std::move(dependencies), firstDisplayPayload(refinementSource));
    kiriview::ImageViewportProviderWorkIdentity identity {
        73,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("seeded-first-display"),
    };

    std::vector<kiriview::ImageViewportProviderFrameResult> results;
    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            {},
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            results.push_back(std::move(result));
        });
    QCOMPARE(results.size(), std::size_t(1));
    QVERIFY(results.front().displayImage.has_value());
    QVERIFY(!results.front().isProvisional());
    QCOMPARE(results.front().displayImage->quality, kiriview::DisplayImageQuality::FirstDisplay);
    QCOMPARE(dataLoader.loadCount(), std::size_t(0));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(0));

    ImageSequenceProviderDisplayDemand refinementDemand;
    refinementDemand.setTargetDisplaySizePixels(QSizeF(400, 300));
    refinementDemand.setCurrentPayloadQuality(ImageViewportPayloadQuality::FirstDisplay);
    refinementDemand.setCurrentPayloadExactness(ImageViewportPayloadExactness::NotExact);
    source->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            refinementDemand,
        },
        [&results](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult result) {
            results.push_back(std::move(result));
        });
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(results.size(), std::size_t(1));

    workerScheduler.runWork(0);
    QCOMPARE(refinementSource->refinementCount, 1);
    QCOMPARE(refinementSource->lastRefinementSize, QSize(400, 300));
    workerScheduler.finish(0);

    QCOMPARE(results.size(), std::size_t(2));
    QVERIFY(results.back().displayImage.has_value());
    QVERIFY(!results.back().isProvisional());
    QCOMPARE(results.back().displayImage->quality, kiriview::DisplayImageQuality::BoundedDetail);
    QCOMPARE(results.back().displayImage->image.size(), QSize(400, 300));
    QCOMPARE(dataLoader.loadCount(), std::size_t(0));
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
        std::move(dependencies), firstDisplayPayload(refinementSource));
    const kiriview::ImageViewportProviderWorkIdentity identity {
        74,
        ImageViewportPageRole::Primary,
        requestToken,
        {},
        QStringLiteral("refinement-lifecycle"),
    };
    ImageSequenceProviderDisplayDemand refinementDemand;
    refinementDemand.setTargetDisplaySizePixels(QSizeF(400, 300));
    refinementDemand.setCurrentPayloadQuality(ImageViewportPayloadQuality::FirstDisplay);
    refinementDemand.setCurrentPayloadExactness(ImageViewportPayloadExactness::NotExact);
    const kiriview::ImageViewportProviderFrameRequest request {
        0,
        refinementDemand,
    };
    int completionCount = 0;

    source->requestFrame(identity, request,
        [&completionCount](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult) { ++completionCount; });
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QCOMPARE(completionCount, 1);
    QVERIFY(!workerScheduler.isActive(0));

    source->requestFrame(identity, request,
        [&completionCount](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderFrameResult) { ++completionCount; });
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);
    source->cancel({ identity.requestToken });
    QVERIFY(!workerScheduler.isActive(1));
    workerScheduler.finish(1);
    QCOMPARE(completionCount, 1);

    source->requestFrame(identity, request,
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
        std::move(dependencies), firstDisplayPayload(refinementSource));
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
        std::move(dependencies), firstDisplayPayload(refinementSource));
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
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(0);
    workerScheduler.runWork(1);
    workerScheduler.finish(0);
    workerScheduler.finish(1);

    QCOMPARE(completions.size(), std::size_t(2));
    QVERIFY(completions.at(0).displayImage.has_value());
    QVERIFY(completions.at(1).displayImage.has_value());
    QCOMPARE(completions.at(0).displayImage->quality, kiriview::DisplayImageQuality::Exact);
    QCOMPARE(completions.at(1).displayImage->quality, kiriview::DisplayImageQuality::BoundedDetail);

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
        displayPayload(kiriview::DisplayImageQuality::Exact),
        ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("png"));
    fixture.create();

    ImageViewport viewport;
    fixture.assign(viewport);
    QTRY_COMPARE(fixture.store->size(), qsizetype(1));

    const auto acquireCompetingEntry = [&fixture]() {
        const kiriview::StaticDisplayImagePayload payload
            = displayPayload(kiriview::DisplayImageQuality::Exact);
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
