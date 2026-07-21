// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "image_test_support.h"
#include "location/imageurl.h"
#include "rendering/imageviewportdecodesource.h"
#include "rendering/imageviewportsequenceprovider.h"

#include <ImageViewport/imagesequence.h>
#include <ImageViewport/imageviewport.h>

#include <QSignalSpy>
#include <QTest>

#include <deque>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

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

    void create(std::optional<kiriview::StaticDisplayImagePayload> predecoded = std::nullopt)
    {
        resource = std::make_shared<kiriview::ImageViewportProviderResource>(
            41, QStringLiteral("fake-location"), source, store, failures, std::move(predecoded));
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
    void readerOrientationProducesNormalizedFrame();
    void validatedPredecodeUsesTheProviderSession();
    void laterDemandRefinesPredecodeThroughSameResource();
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

void TestImageViewportSequenceProvider::validatedPredecodeUsesTheProviderSession()
{
    ProviderFixture fixture;
    fixture.source->knownMetadata = ImageSequenceProviderMetadata::still(QSizeF(128, 64));
    fixture.create(displayPayload(kiriview::DisplayImageQuality::ThumbnailPreview));

    ImageViewport viewport;
    fixture.assign(viewport);

    QTRY_COMPARE(fixture.store->size(), qsizetype(1));
    QCOMPARE(fixture.source->metadataRequests.size(), std::size_t(0));
    QCOMPARE(fixture.source->frameRequests.size(), std::size_t(0));
    QCOMPARE(viewport.state().primary().request().sourceLogicalSize(), QSizeF(128, 64));
}

void TestImageViewportSequenceProvider::laterDemandRefinesPredecodeThroughSameResource()
{
    ProviderFixture fixture;
    fixture.source->knownMetadata = ImageSequenceProviderMetadata::still(QSizeF(128, 64));
    fixture.create(displayPayload(kiriview::DisplayImageQuality::ThumbnailPreview));

    ImageSequenceProviderDisplayDemand firstDemand;
    kiriview::ImageViewportProviderWorkIdentity identity {
        41,
        ImageViewportPageRole::Primary,
        {},
        {},
        QStringLiteral("fake-location"),
    };
    bool previewReady = false;
    fixture.resource->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            firstDemand,
        },
        [&previewReady](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderPreparedFrame result) {
            previewReady = result.isReady();
        });
    QVERIFY(previewReady);
    QCOMPARE(fixture.source->frameRequests.size(), std::size_t(0));

    ImageSequenceProviderDisplayDemand refinementDemand;
    refinementDemand.setCurrentPayloadQuality(ImageViewportPayloadQuality::Preview);
    refinementDemand.setCurrentPayloadExactness(ImageViewportPayloadExactness::NotExact);
    fixture.resource->requestFrame(identity,
        kiriview::ImageViewportProviderFrameRequest {
            0,
            refinementDemand,
        },
        [](kiriview::ImageViewportProviderWorkIdentity,
            kiriview::ImageViewportProviderPreparedFrame) { });
    QCOMPARE(fixture.source->frameRequests.size(), std::size_t(1));
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
