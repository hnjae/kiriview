// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>
#include <ranges>

namespace {
ImageSequenceProviderFrameEnvelope exactTestEnvelope(QSizeF logicalSize, QSize payloadSize,
    qint64 payloadBytes, bool hasAlpha, int frame = 0, int frameStart = -1, int frameDuration = -1)
{
    Q_UNUSED(logicalSize)
    Q_UNUSED(payloadSize)
    Q_UNUSED(payloadBytes)
    Q_UNUSED(hasAlpha)
    ImageSequenceProviderFrameEnvelope envelope;
    envelope.setFrame(frame);
    envelope.setFrameStartPosition(frameStart);
    envelope.setFrameDuration(frameDuration);
    return envelope;
}

class TypedProviderSession final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderSession
{
public:
    explicit TypedProviderSession(QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
    {
    }

    void request(const ImageSequenceProviderRequest& request) override
    {
        requests.append(request);
        if (request.kind() == ImageSequenceProviderRequestKind::Metadata) {
            Q_EMIT providerEvent(ImageSequenceProviderEvent::metadataReady(
                request.token(), ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0))));
        }
    }

    void emitFrameReady(ImageSequenceProviderRequestToken token)
    {
        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        ImageSequenceProviderFrameEnvelope envelope = exactTestEnvelope(
            QSizeF(16.0, 8.0), image.size(), image.sizeInBytes(), image.hasAlphaChannel());
        for (const ImageSequenceProviderRequest& request : requests | std::views::reverse) {
            if (request.token() == token) {
                envelope.setDemandRevision(request.demand().demandRevision());
                break;
            }
        }
        auto frame = std::make_unique<ImageFrame>(image);
        Q_EMIT providerEvent(ImageSequenceProviderEvent::frameReady(
            token, new ImageSequenceProviderFrameHandle(std::move(frame), this), envelope));
    }

    QVector<ImageSequenceProviderRequest> requests;
};

class TypedProviderSessionFactory final
{
public:
    ImageSequenceProviderSession* createSession(QObject* parent)
    {
        lastSession = new TypedProviderSession(parent);
        return lastSession;
    }

    QPointer<TypedProviderSession> lastSession;
};

class TypedDescriptorProviderAdapter final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderAdapter
{
public:
    explicit TypedDescriptorProviderAdapter(
        std::shared_ptr<TypedProviderSessionFactory> factory, QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , m_factory(std::move(factory))
    {
    }

    [[nodiscard]] ImageSequenceProviderDescriptor descriptor() const override
    {
        const auto factory = m_factory;
        return ImageSequenceProviderDescriptor(
            {}, ImageSequenceProviderThreadingContract::ThreadSafe, [factory]() {
                return ImageSequenceProviderSessionFactoryResult::created(
                    factory->createSession(nullptr));
            });
    }

private:
    std::shared_ptr<TypedProviderSessionFactory> m_factory;
};
}

class ImageViewportProviderContractTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderContractTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:
    void providerPublicValueTypesValidateTiming();
    void typedProviderFailureHandleReleasesExactlyOnce();
    void providerTypedProtocolValuesValidateShape();
    void typedDescriptorFactoryAndSessionBridgeDeliversTypedRequests();
    void dynamicMaximumClampRestagesCoherentProviderDemand();
    void providerSpreadBudgetAccountsForRetainedPayloadAndResourcePressure();
    void providerSpreadCommitReissuesDemandWhenBudgetIncreases();
    void providerRoleBudgetRejectsPayloadBeforeSpreadOversubscription();
    void providerFactoryRejectsBaseAdapterWithoutSessionFactory();
    void providerFactoryRejectsContradictoryConstructionFacts();
    void providerFactoryRejectsInvalidKnownMetadata();
    void providerFactoryRejectsPublishedKnownMetadataLimits();
    void providerSessionEntryPointsUseSessionAffinity();
    void affinityBoundReleaseDoesNotBlockViewportCleanup();
    void providerThreadSafeSessionEntryPointsUseViewportAffinity();
    void providerSequenceOpensSessionAfterAdapterDestruction();
    void providerSharedSequenceUsesIndependentViewportSessions();
    void providerSessionOpenFailureKeepsReplacementObservable();
    void reassigningSameProviderSequenceStartsNewGeneration();
};

void ImageViewportProviderContractTest::typedProviderFailureHandleReleasesExactlyOnce()
{
    const ImageSequenceProviderRequestToken token = providerRequestTokenForTest(42);
    int releaseCount = 0;
    ImageSequenceProviderFailureHandle handle([&releaseCount]() { ++releaseCount; });
    const ImageSequenceProviderFailureReference reference = handle.reference();
    QVERIFY(reference.isValid());

    const ImageSequenceProviderFailure failure(ImageSequenceProviderFailureCause::Decode, &handle);
    QVERIFY(failure.isValid());
    QCOMPARE(failure.cause(), ImageSequenceProviderFailureCause::Decode);
    QCOMPARE(failure.applicationFailureHandle(), &handle);

    const ImageSequenceProviderSessionFactoryResult factoryResult
        = ImageSequenceProviderSessionFactoryResult::failed(failure);
    QCOMPARE(factoryResult.outcome(), ImageSequenceProviderSessionFactoryOutcome::Failed);
    QCOMPARE(factoryResult.session(), nullptr);
    QCOMPARE(factoryResult.failure().cause(), ImageSequenceProviderFailureCause::Decode);
    QCOMPARE(factoryResult.failure().applicationFailureHandle(), &handle);

    const ImageSequenceProviderEvent event = ImageSequenceProviderEvent::failed(token, failure);
    QVERIFY(event.isValid());
    QCOMPARE(event.failure().cause(), ImageSequenceProviderFailureCause::Decode);
    QCOMPARE(event.failure().applicationFailureHandle()->reference(), reference);

    handle.release();
    handle.release();
    QCOMPARE(releaseCount, 1);
}

void ImageViewportProviderContractTest::providerPublicValueTypesValidateTiming()
{
    const ImageSequenceProviderRequestToken defaultToken;
    QCOMPARE(defaultToken.isValid(), false);
    QCOMPARE(defaultToken, ImageSequenceProviderRequestToken());

    const ImageSequenceProviderRequestToken token = providerRequestTokenForTest(42);
    QCOMPARE(token.isValid(), true);
    QVERIFY(token != defaultToken);
    QCOMPARE(token, providerRequestTokenForTest(42));
    QVERIFY(token != providerRequestTokenForTest(43));

    const ImageSequenceProviderMetadata emptyMetadata;
    QCOMPARE(emptyMetadata.isSpecified(), false);
    QCOMPARE(emptyMetadata.isValid(), false);
    QCOMPARE(emptyMetadata.isStill(), false);
    QCOMPARE(emptyMetadata.isTimedFrameList(), false);
    QCOMPARE(emptyMetadata.frameDurations(), QVector<int>());

    const ImageSequenceProviderMetadata stillMetadata
        = ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0));
    QCOMPARE(stillMetadata.isSpecified(), true);
    QCOMPARE(stillMetadata.isValid(), true);
    QCOMPARE(stillMetadata.isStill(), true);
    QCOMPARE(stillMetadata.isTimedFrameList(), false);
    QCOMPARE(stillMetadata.sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(stillMetadata.frameDurations(), QVector<int>());

    const ImageSequenceProviderMetadata fixedDurationMetadata
        = ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 3, 100);
    QCOMPARE(fixedDurationMetadata.isSpecified(), true);
    QCOMPARE(fixedDurationMetadata.isValid(), true);
    QCOMPARE(fixedDurationMetadata.isStill(), false);
    QCOMPARE(fixedDurationMetadata.isTimedFrameList(), true);
    QCOMPARE(fixedDurationMetadata.frameDurations(), QVector<int>({ 100, 100, 100 }));

    const ImageSequenceProviderMetadata overLimitFixedDurationMetadata
        = ImageSequenceProviderMetadata::fixedDurationFrames(
            QSizeF(16.0, 8.0), ImageSequenceLimits::maximumFrameCount() + 2, 100);
    QCOMPARE(overLimitFixedDurationMetadata.isSpecified(), true);
    QCOMPARE(overLimitFixedDurationMetadata.isTimedFrameList(), true);
    QCOMPARE(overLimitFixedDurationMetadata.frameDurations().size(),
        ImageSequenceLimits::maximumFrameCount() + 1);

    const ImageSequenceProviderMetadata zeroDurationMetadata
        = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 0 });
    QCOMPARE(zeroDurationMetadata.isSpecified(), true);
    QCOMPARE(zeroDurationMetadata.isValid(), false);

    const ImageSequenceProviderMetadata negativeDurationMetadata
        = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, -1 });
    QCOMPARE(negativeDurationMetadata.isSpecified(), true);
    QCOMPARE(negativeDurationMetadata.isValid(), false);

    const ImageSequenceProviderMetadata infiniteSizeMetadata = ImageSequenceProviderMetadata::still(
        QSizeF(std::numeric_limits<double>::infinity(), 8.0));
    QCOMPARE(infiniteSizeMetadata.isSpecified(), true);
    QCOMPARE(infiniteSizeMetadata.isValid(), false);

    const ImageSequenceProviderMetadata fractionalSizeMetadata
        = ImageSequenceProviderMetadata::still(QSizeF(16.5, 8.0));
    QCOMPARE(fractionalSizeMetadata.isSpecified(), true);
    QCOMPARE(fractionalSizeMetadata.isValid(), false);

    const ImageSequenceProviderMetadata invalidFixedDurationMetadata
        = ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 2, 0);
    QCOMPARE(invalidFixedDurationMetadata.isSpecified(), true);
    QCOMPARE(invalidFixedDurationMetadata.isValid(), false);

    const ImageSequenceProviderFrameEnvelope emptyFrameMetadata;
    QCOMPARE(emptyFrameMetadata.isValid(), false);
    QCOMPARE(emptyFrameMetadata.isStillFrame(), false);
    QCOMPARE(emptyFrameMetadata.isTimedFrame(), false);
    QCOMPARE(emptyFrameMetadata.frame(), -1);
    QCOMPARE(emptyFrameMetadata.frameStartPosition(), -1);
    QCOMPARE(emptyFrameMetadata.frameDuration(), -1);

    const ImageSequenceProviderFrameEnvelope stillFrameMetadata
        = ImageSequenceProviderFrameEnvelope::stillFrame();
    QCOMPARE(stillFrameMetadata.isValid(), true);
    QCOMPARE(stillFrameMetadata.isStillFrame(), true);
    QCOMPARE(stillFrameMetadata.isTimedFrame(), false);
    QCOMPARE(stillFrameMetadata.frame(), 0);
    QCOMPARE(stillFrameMetadata.frameStartPosition(), -1);
    QCOMPARE(stillFrameMetadata.frameDuration(), -1);

    const ImageSequenceProviderFrameEnvelope timedFrameMetadata
        = ImageSequenceProviderFrameEnvelope::timedFrame(1, 100, 250);
    QCOMPARE(timedFrameMetadata.isValid(), true);
    QCOMPARE(timedFrameMetadata.isStillFrame(), false);
    QCOMPARE(timedFrameMetadata.isTimedFrame(), true);
    QCOMPARE(timedFrameMetadata.frame(), 1);
    QCOMPARE(timedFrameMetadata.frameStartPosition(), 100);
    QCOMPARE(timedFrameMetadata.frameDuration(), 250);

    const ImageSequenceProviderFrameEnvelope unknownDurationTimedFrameMetadata
        = ImageSequenceProviderFrameEnvelope::timedFrame(1, 100, -1);
    QCOMPARE(unknownDurationTimedFrameMetadata.isValid(), false);
    QCOMPARE(unknownDurationTimedFrameMetadata.isTimedFrame(), true);
    QCOMPARE(unknownDurationTimedFrameMetadata.frame(), 1);
    QCOMPARE(unknownDurationTimedFrameMetadata.frameStartPosition(), 100);
    QCOMPARE(unknownDurationTimedFrameMetadata.frameDuration(), -1);

    QCOMPARE(ImageSequenceProviderFrameEnvelope::timedFrame(-1, 100, 250).isValid(), false);
    QCOMPARE(ImageSequenceProviderFrameEnvelope::timedFrame(1, -1, 250).isValid(), false);
    QCOMPARE(ImageSequenceProviderFrameEnvelope::timedFrame(1, 100, 0).isValid(), false);

    const ImageSequenceProviderMetadata emptyFacts;
    QCOMPARE(emptyFacts.isSpecified(), false);
    QCOMPARE(emptyFacts.isValid(), false);
    QCOMPARE(emptyFacts.hasCompleteModel(), false);
    QCOMPARE(emptyFacts.sourceLogicalSize(), QSizeF());
    QCOMPARE(emptyFacts.frameCount(), -1);
    QCOMPARE(emptyFacts.frameDurations(), QVector<int>());

    const ImageSequenceProviderMetadata logicalSizeFacts
        = ImageSequenceProviderMetadata::withSourceLogicalSize(QSizeF(16.0, 8.0));
    QCOMPARE(logicalSizeFacts.isSpecified(), true);
    QCOMPARE(logicalSizeFacts.isValid(), true);
    QCOMPARE(logicalSizeFacts.hasCompleteModel(), false);
    QCOMPARE(logicalSizeFacts.sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(logicalSizeFacts.frameCount(), -1);
    QCOMPARE(logicalSizeFacts.frameDurations(), QVector<int>());

    const ImageSequenceProviderMetadata stillFacts
        = ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0));
    QCOMPARE(stillFacts.isSpecified(), true);
    QCOMPARE(stillFacts.isValid(), true);
    QCOMPARE(stillFacts.hasCompleteModel(), true);
    QCOMPARE(stillFacts.sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(stillFacts.frameCount(), 1);
    QCOMPARE(stillFacts.frameDurations(), QVector<int>());

    const ImageSequenceProviderMetadata countFacts
        = ImageSequenceProviderMetadata::timedFrameCount(QSizeF(16.0, 8.0), 3);
    QCOMPARE(countFacts.isSpecified(), true);
    QCOMPARE(countFacts.isValid(), true);
    QCOMPARE(countFacts.hasCompleteModel(), false);
    QCOMPARE(countFacts.sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(countFacts.frameCount(), 3);
    QCOMPARE(countFacts.frameDurations(), QVector<int>());

    const ImageSequenceProviderMetadata fixedFacts
        = ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 3, 100);
    QCOMPARE(fixedFacts.isSpecified(), true);
    QCOMPARE(fixedFacts.isValid(), true);
    QCOMPARE(fixedFacts.hasCompleteModel(), true);
    QCOMPARE(fixedFacts.frameCount(), 3);
    QCOMPARE(fixedFacts.frameDurations(), QVector<int>({ 100, 100, 100 }));

    const ImageSequenceProviderMetadata listFacts
        = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 });
    QCOMPARE(listFacts.isSpecified(), true);
    QCOMPARE(listFacts.isValid(), true);
    QCOMPARE(listFacts.hasCompleteModel(), true);
    QCOMPARE(listFacts.frameCount(), 2);
    QCOMPARE(listFacts.frameDurations(), QVector<int>({ 100, 250 }));

    PlaybackFallbackSession session;
    session.request(
        ImageSequenceProviderRequest::playback(token, ImageViewportPageRole::Primary, 7, 125, {}));
    QCOMPARE(session.frameRequestCount, 1);
    QCOMPARE(session.lastFrameToken, token);
    QCOMPARE(session.lastFrame, 7);
    session.request(
        ImageSequenceProviderRequest::position(token, ImageViewportPageRole::Primary, 349, 8, {}));
    QCOMPARE(session.frameRequestCount, 2);
    QCOMPARE(session.lastFrameToken, token);
    QCOMPARE(session.lastFrame, 8);

    NullSessionFactoryProviderAdapter nullAdapter;
    QCOMPARE(nullAdapter.descriptor().threadingContract(),
        ImageSequenceProviderThreadingContract::AffinityBound);
}

void ImageViewportProviderContractTest::providerTypedProtocolValuesValidateShape()
{
    const ImageSequenceProviderRequestToken token = providerRequestTokenForTest(42);

    ImageSequenceProviderDisplayDemand demand;
    QCOMPARE(demand.demandRevision().isValid(), false);
    QCOMPARE(demand.requestRevision().isValid(), false);
    QCOMPARE(demand.presentationRevision().isValid(), false);
    QCOMPARE(demand.role(), ImageViewportPageRole::Primary);
    QCOMPARE(demand.resolvedFrame(), -1);
    QCOMPARE(demand.requestedPosition(), -1);
    QCOMPARE(demand.maximumTextureSize(), -1);
    QCOMPARE(demand.maximumPayloadBytes(), -1);
    QCOMPARE(demand.displayByteBudget(), -1);
    QCOMPARE(demand.currentPayloadQuality(), ImageViewportPayloadQuality::Unknown);
    QCOMPARE(demand.currentPayloadExactness(), ImageViewportPayloadExactness::Unknown);

    demand.setRole(ImageViewportPageRole::Secondary);
    demand.setResolvedFrame(3);
    demand.setRequestedPosition(125);
    const ImageSequenceProviderRequest metadataRequest
        = ImageSequenceProviderRequest::metadata(token);
    QVERIFY(metadataRequest.isValid());
    QCOMPARE(metadataRequest.kind(), ImageSequenceProviderRequestKind::Metadata);
    QCOMPARE(metadataRequest.token(), token);

    const ImageSequenceProviderRequest frameRequest
        = ImageSequenceProviderRequest::frame(token, ImageViewportPageRole::Secondary, 3, demand);
    QVERIFY(frameRequest.isValid());
    QCOMPARE(frameRequest.kind(), ImageSequenceProviderRequestKind::Frame);
    QCOMPARE(frameRequest.role(), ImageViewportPageRole::Secondary);
    QCOMPARE(frameRequest.frame(), 3);
    QCOMPARE(frameRequest.resolvedFrame(), 3);
    QCOMPARE(frameRequest.requestedPosition(), -1);
    QCOMPARE(frameRequest.demand().resolvedFrame(), 3);

    const ImageSequenceProviderRequest positionRequest = ImageSequenceProviderRequest::position(
        token, ImageViewportPageRole::Secondary, 125, 3, demand);
    QVERIFY(positionRequest.isValid());
    QCOMPARE(positionRequest.kind(), ImageSequenceProviderRequestKind::Position);
    QCOMPARE(positionRequest.requestedPosition(), 125);
    QCOMPARE(positionRequest.resolvedFrame(), 3);

    const ImageSequenceProviderRequest playbackRequest = ImageSequenceProviderRequest::playback(
        token, ImageViewportPageRole::Secondary, 3, 125, demand);
    QVERIFY(playbackRequest.isValid());
    QCOMPARE(playbackRequest.kind(), ImageSequenceProviderRequestKind::Playback);
    QCOMPARE(playbackRequest.frame(), 3);
    QCOMPARE(playbackRequest.requestedPosition(), 125);

    QVERIFY(ImageSequenceProviderRequest::cancel({ token }).isValid());
    QVERIFY(ImageSequenceProviderRequest::close().isValid());
    QVERIFY(!ImageSequenceProviderRequest::metadata({}).isValid());
    QVERIFY(!ImageSequenceProviderRequest::frame({}, ImageViewportPageRole::Primary, 0, demand)
            .isValid());
    QVERIFY(!ImageSequenceProviderRequest::cancel({}).isValid());

    const ImageSequenceProviderFrameEnvelope stillEnvelope
        = exactTestEnvelope(QSizeF(16.0, 8.0), QSize(16, 8), 512, true);
    QVERIFY(stillEnvelope.isValid());
    QCOMPARE(stillEnvelope.frame(), 0);
    QCOMPARE(stillEnvelope.frameStartPosition(), -1);
    QCOMPARE(stillEnvelope.frameDuration(), -1);

    ImageSequenceProviderFrameEnvelope invalidEnvelope = stillEnvelope;
    invalidEnvelope.setFrameStartPosition(0);
    QVERIFY(!invalidEnvelope.isValid());

    const ImageSequenceProviderFrameEnvelope timedEnvelope
        = exactTestEnvelope(QSizeF(16.0, 8.0), QSize(16, 8), 512, true, 2, 250, 125);
    QVERIFY(timedEnvelope.isValid());
    QCOMPARE(timedEnvelope.frame(), 2);
    QCOMPARE(timedEnvelope.frameStartPosition(), 250);
    QCOMPARE(timedEnvelope.frameDuration(), 125);

    auto frame
        = std::make_unique<ImageFrame>(QImage(QSize(16, 8), QImage::Format_ARGB32_Premultiplied));
    ImageSequenceProviderFrameHandle handle(std::move(frame));
    QVERIFY(ImageSequenceProviderEvent::metadataReady(
        token, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)))
            .isValid());
    QVERIFY(ImageSequenceProviderEvent::frameReady(token, &handle, stillEnvelope).isValid());
    QVERIFY(ImageSequenceProviderEvent::waiting(token).isValid());
    QVERIFY(ImageSequenceProviderEvent::progress(token, 0.5).isValid());
    QVERIFY(!ImageSequenceProviderEvent::progress(token, 1.5).isValid());
    QVERIFY(ImageSequenceProviderEvent::endOfSequence(token).isValid());
    QVERIFY(ImageSequenceProviderEvent::unsupported(
        token, ImageSequenceProviderUnsupportedCause::UnsupportedRequest)
            .isValid());
    QVERIFY(!ImageSequenceProviderEvent::unsupported(
        token, static_cast<ImageSequenceProviderUnsupportedCause>(-1))
            .isValid());
    QVERIFY(ImageSequenceProviderEvent::cancelled(token).isValid());
    QVERIFY(ImageSequenceProviderEvent::failed(
        token, ImageSequenceProviderFailure(ImageSequenceProviderFailureCause::ProviderInternal))
            .isValid());
    QCOMPARE(ImageSequenceProviderEvent::staticMetaObject.indexOfProperty("diagnostic"), -1);
    QVERIFY(!ImageSequenceProviderEvent::waiting({}).isValid());

    const ImageSequenceProviderDescriptor descriptor;
    QVERIFY(!descriptor.isValid());
    QCOMPARE(descriptor.threadingContract(), ImageSequenceProviderThreadingContract::AffinityBound);
}

void ImageViewportProviderContractTest::
    typedDescriptorFactoryAndSessionBridgeDeliversTypedRequests()
{
    ImageSequenceFactory factory;
    auto sessionFactory = std::make_shared<TypedProviderSessionFactory>();
    TypedDescriptorProviderAdapter adapter(sessionFactory);

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result);
    QVERIFY(result->sequence());
    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Created);

    ImageViewport item;
    item.setSize(QSizeF(100.0, 50.0));
    useSynchronousProviderEventDeliveryForTest(item);

    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QVERIFY(sessionFactory->lastSession);
    QVERIFY(sessionFactory->lastSession->requests.size() >= 2);
    const ImageSequenceProviderRequest metadataRequest
        = sessionFactory->lastSession->requests.at(0);
    QCOMPARE(metadataRequest.kind(), ImageSequenceProviderRequestKind::Metadata);
    QVERIFY(metadataRequest.token().isValid());

    const ImageSequenceProviderRequest frameRequest = sessionFactory->lastSession->requests.at(1);
    QCOMPARE(frameRequest.kind(), ImageSequenceProviderRequestKind::Frame);
    QCOMPARE(frameRequest.token().isValid(), true);
    QCOMPARE(frameRequest.role(), ImageViewportPageRole::Primary);
    QCOMPARE(frameRequest.frame(), 0);
    QCOMPARE(frameRequest.demand().role(), ImageViewportPageRole::Primary);
    QCOMPARE(frameRequest.demand().resolvedFrame(), 0);
    QCOMPARE(frameRequest.demand().requestedPosition(), -1);
    QCOMPARE(frameRequest.demand().demandRevision().isValid(), true);
    QCOMPARE(frameRequest.demand().requestRevision().isValid(), true);
    QCOMPARE(frameRequest.demand().presentationRevision().isValid(), true);
    QCOMPARE(frameRequest.demand().sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(frameRequest.demand().visibleSourceRect(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(frameRequest.demand().targetDisplaySizePixels(), QSizeF(100.0, 50.0));
    QCOMPARE(frameRequest.demand().effectiveDevicePixelRatio(), 1.0);
    QCOMPARE(
        frameRequest.demand().maximumPayloadBytes(), ImageSequenceLimits::maximumPayloadBytes());
    QCOMPARE(frameRequest.demand().displayByteBudget(), ImageSequenceLimits::maximumPayloadBytes());
    QCOMPARE(frameRequest.demand().allocationGeneration().isValid(), true);

    sessionFactory->lastSession->emitFrameReady(frameRequest.token());

    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Loading);
    QCOMPARE(requestReason(item), ImageViewportRequestReason::RenderWaiting);
    QVERIFY(hasPendingRenderCommitForTest(item));
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Ready);
    QCOMPARE(requestReason(item), ImageViewportRequestReason::Ready);
    QCOMPARE(displayStatus(item), ImageViewportDisplayStatus::Ready);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
}

void ImageViewportProviderContractTest::dynamicMaximumClampRestagesCoherentProviderDemand()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(
        sessionFactory, ImageSequenceProviderMetadata::still(QSizeF(100, 100)));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(9000, 9000));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    QCOMPARE(item.state().presentation().maximumManualZoomPercent(), 72000.0);

    ImageViewportPresentationCommand manual;
    manual.setFitMode(ImageViewportFitMode::Manual);
    manual.setPreferredManualZoomPercent(70000.0);
    QCOMPARE(item.setPresentation(manual).outcome(), ImageViewportCommandOutcome::Accepted);
    const auto before = item.state();
    const int frameRequestsBeforeResize = *frameRequestCount;

    item.setSize(QSizeF(100, 100));

    const auto after = item.state();
    QCOMPARE(after.presentation().maximumManualZoomPercent(), 65536.0);
    QCOMPARE(after.presentation().preferredManualZoomPercent(), 70000.0);
    QCOMPARE(after.presentation().zoomPercent(), 65536.0);
    QVERIFY(after.revisions().request() != before.revisions().request());
    QVERIFY(after.revisions().presentation() != before.revisions().presentation());
    QVERIFY(*frameRequestCount > frameRequestsBeforeResize);
    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderDisplayDemand demand
        = sessionFactory->lastSession()->lastFrameDemand();
    QCOMPARE(demand.sourceLogicalSize(), QSizeF(100, 100));
    QCOMPARE(demand.targetDisplaySizePixels(), QSizeF(65536, 65536));
    QCOMPARE(demand.demandRevision(), after.primary().request().demandRevision());
    QCOMPARE(
        demand.targetDisplaySizePixels(), after.primary().geometry().acceptedItemRect().size());
}

void ImageViewportProviderContractTest::
    providerSpreadBudgetAccountsForRetainedPayloadAndResourcePressure()
{
    const qint64 maximumPayloadBytes = ImageSequenceLimits::maximumPayloadBytes();
    QImage retainedImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    retainedImage.fill(Qt::transparent);
    const std::unique_ptr<ImageFrame> retainedFrame
        = makeImageFrameWithPayloadByteSizeForTest(retainedImage, maximumPayloadBytes);
    ImageSequenceFactory sequenceFactory;
    QScopedPointer<ImageSequenceFactoryResult> retainedResult(
        sequenceFactory.fromFrame(retainedFrame.get()));
    QVERIFY(retainedResult->sequence());

    const auto makeProviderFactory = [] {
        return std::make_shared<CountingProviderSessionFactory>(std::make_shared<int>(0),
            std::make_shared<int>(0), std::make_shared<int>(0), std::make_shared<int>(-1),
            std::make_shared<int>(0), std::shared_ptr<int>(), std::shared_ptr<int>(),
            std::shared_ptr<int>(), std::make_shared<int>(0));
    };
    auto primaryFactory = makeProviderFactory();
    auto secondaryFactory = makeProviderFactory();
    CountingProviderAdapter primaryAdapter(primaryFactory);
    CountingProviderAdapter secondaryAdapter(secondaryFactory);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(
        sequenceFactory.fromProvider(&primaryAdapter));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        sequenceFactory.fromProvider(&secondaryAdapter));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    useSynchronousProviderEventDeliveryForTest(item);
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(retainedResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(item.state().display().status(), ImageViewportDisplayStatus::Ready);

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().display().status(), ImageViewportDisplayStatus::Retained);
    QVERIFY(primaryFactory->lastSession());
    QVERIFY(secondaryFactory->lastSession());
    emitProviderMetadataReady(primaryFactory->lastSession(),
        primaryFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    emitProviderMetadataReady(secondaryFactory->lastSession(),
        secondaryFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));

    const qint64 retainedBudget = maximumPayloadBytes / 2;
    const ImageSequenceProviderDisplayDemand primaryDemand
        = primaryFactory->lastSession()->lastFrameDemand();
    const ImageSequenceProviderDisplayDemand secondaryDemand
        = secondaryFactory->lastSession()->lastFrameDemand();
    QCOMPARE(primaryDemand.displayByteBudget(), retainedBudget);
    QCOMPARE(secondaryDemand.displayByteBudget(), retainedBudget);
    QVERIFY(primaryDemand.allocationGeneration().isValid());
    QCOMPARE(primaryDemand.allocationGeneration(), secondaryDemand.allocationGeneration());

    const ImageViewportAllocationGenerationToken retainedAllocation
        = primaryDemand.allocationGeneration();
    discardRetainedDisplayForResourcePressureForTest(item);

    const ImageSequenceProviderDisplayDemand reallocatedPrimary
        = primaryFactory->lastSession()->lastFrameDemand();
    const ImageSequenceProviderDisplayDemand reallocatedSecondary
        = secondaryFactory->lastSession()->lastFrameDemand();
    QCOMPARE(reallocatedPrimary.displayByteBudget(), maximumPayloadBytes);
    QCOMPARE(reallocatedSecondary.displayByteBudget(), maximumPayloadBytes);
    QVERIFY(reallocatedPrimary.allocationGeneration() != retainedAllocation);
    QCOMPARE(
        reallocatedPrimary.allocationGeneration(), reallocatedSecondary.allocationGeneration());
}

void ImageViewportProviderContractTest::
    providerRoleBudgetRejectsPayloadBeforeSpreadOversubscription()
{
    const qint64 maximumPayloadBytes = ImageSequenceLimits::maximumPayloadBytes();
    QImage retainedImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    retainedImage.fill(Qt::transparent);
    const std::unique_ptr<ImageFrame> retainedFrame
        = makeImageFrameWithPayloadByteSizeForTest(retainedImage, maximumPayloadBytes);
    ImageSequenceFactory sequenceFactory;
    QScopedPointer<ImageSequenceFactoryResult> retainedResult(
        sequenceFactory.fromFrame(retainedFrame.get()));
    QVERIFY(retainedResult->sequence());

    const auto makeProviderFactory = [] {
        return std::make_shared<CountingProviderSessionFactory>(std::make_shared<int>(0),
            std::make_shared<int>(0), std::make_shared<int>(0), std::make_shared<int>(-1),
            std::make_shared<int>(0));
    };
    auto primaryFactory = makeProviderFactory();
    auto secondaryFactory = makeProviderFactory();
    CountingProviderAdapter primaryAdapter(primaryFactory);
    CountingProviderAdapter secondaryAdapter(secondaryFactory);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(
        sequenceFactory.fromProvider(&primaryAdapter));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        sequenceFactory.fromProvider(&secondaryAdapter));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    useSynchronousProviderEventDeliveryForTest(item);
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(retainedResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    emitProviderMetadataReady(primaryFactory->lastSession(),
        primaryFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    emitProviderMetadataReady(secondaryFactory->lastSession(),
        secondaryFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));

    const ImageSequenceProviderDisplayDemand demand
        = primaryFactory->lastSession()->lastFrameDemand();
    QCOMPARE(demand.displayByteBudget(), maximumPayloadBytes / 2);
    QImage payloadImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    payloadImage.fill(Qt::transparent);
    const std::unique_ptr<ImageFrame> oversized
        = makeImageFrameWithPayloadByteSizeForTest(payloadImage, demand.displayByteBudget() + 1);
    ImageSequenceProviderFrameEnvelope envelope = ImageSequenceProviderFrameEnvelope::stillFrame();
    envelope.setDemandRevision(demand.demandRevision());
    emitProviderFrameReady(primaryFactory->lastSession(),
        primaryFactory->lastSession()->lastFrameToken(), oversized.get(), envelope);

    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Unsupported);
    QCOMPARE(item.state().request().reason(), ImageViewportRequestReason::PayloadRejection);
    QCOMPARE(item.state().display().status(), ImageViewportDisplayStatus::Retained);
}

void ImageViewportProviderContractTest::providerSpreadCommitReissuesDemandWhenBudgetIncreases()
{
    const qint64 maximumPayloadBytes = ImageSequenceLimits::maximumPayloadBytes();
    QImage retainedImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    retainedImage.fill(Qt::transparent);
    const std::unique_ptr<ImageFrame> retainedFrame
        = makeImageFrameWithPayloadByteSizeForTest(retainedImage, maximumPayloadBytes);
    ImageSequenceFactory sequenceFactory;
    QScopedPointer<ImageSequenceFactoryResult> retainedResult(
        sequenceFactory.fromFrame(retainedFrame.get()));
    QVERIFY(retainedResult->sequence());

    const auto primaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto makeProviderFactory = [](const std::shared_ptr<int>& frameRequestCount) {
        return std::make_shared<CountingProviderSessionFactory>(std::make_shared<int>(0),
            std::make_shared<int>(0), frameRequestCount, std::make_shared<int>(-1),
            std::make_shared<int>(0));
    };
    auto primaryFactory = makeProviderFactory(primaryFrameRequestCount);
    auto secondaryFactory = makeProviderFactory(secondaryFrameRequestCount);
    CountingProviderAdapter primaryAdapter(primaryFactory);
    CountingProviderAdapter secondaryAdapter(secondaryFactory);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(
        sequenceFactory.fromProvider(&primaryAdapter));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        sequenceFactory.fromProvider(&secondaryAdapter));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    useSynchronousProviderEventDeliveryForTest(item);
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(retainedResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    emitProviderMetadataReady(primaryFactory->lastSession(),
        primaryFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    emitProviderMetadataReady(secondaryFactory->lastSession(),
        secondaryFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));

    const ImageSequenceProviderDisplayDemand primaryDemand
        = primaryFactory->lastSession()->lastFrameDemand();
    const ImageSequenceProviderDisplayDemand secondaryDemand
        = secondaryFactory->lastSession()->lastFrameDemand();
    QCOMPARE(primaryDemand.displayByteBudget(), maximumPayloadBytes / 2);
    QCOMPARE(secondaryDemand.displayByteBudget(), maximumPayloadBytes / 2);
    QCOMPARE(*primaryFrameRequestCount, 1);
    QCOMPARE(*secondaryFrameRequestCount, 1);

    QImage payloadImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    payloadImage.fill(Qt::transparent);
    const qint64 payloadByteSize = payloadImage.sizeInBytes();
    const std::unique_ptr<ImageFrame> primaryFrame
        = makeImageFrameWithPayloadByteSizeForTest(payloadImage, payloadByteSize);
    const std::unique_ptr<ImageFrame> secondaryFrame
        = makeImageFrameWithPayloadByteSizeForTest(payloadImage, payloadByteSize);
    ImageSequenceProviderFrameEnvelope primaryEnvelope
        = ImageSequenceProviderFrameEnvelope::stillFrame();
    primaryEnvelope.setDemandRevision(primaryDemand.demandRevision());
    ImageSequenceProviderFrameEnvelope secondaryEnvelope
        = ImageSequenceProviderFrameEnvelope::stillFrame();
    secondaryEnvelope.setDemandRevision(secondaryDemand.demandRevision());
    emitProviderFrameReady(primaryFactory->lastSession(),
        primaryFactory->lastSession()->lastFrameToken(), primaryFrame.get(), primaryEnvelope);
    emitProviderFrameReady(secondaryFactory->lastSession(),
        secondaryFactory->lastSession()->lastFrameToken(), secondaryFrame.get(), secondaryEnvelope);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(*primaryFrameRequestCount, 2);
    QCOMPARE(*secondaryFrameRequestCount, 2);
    const ImageSequenceProviderDisplayDemand reallocatedPrimary
        = primaryFactory->lastSession()->lastFrameDemand();
    const ImageSequenceProviderDisplayDemand reallocatedSecondary
        = secondaryFactory->lastSession()->lastFrameDemand();
    QCOMPARE(reallocatedPrimary.displayByteBudget(), maximumPayloadBytes - payloadByteSize);
    QCOMPARE(reallocatedSecondary.displayByteBudget(), maximumPayloadBytes - payloadByteSize);
    QVERIFY(reallocatedPrimary.allocationGeneration() != primaryDemand.allocationGeneration());
    QCOMPARE(
        reallocatedPrimary.allocationGeneration(), reallocatedSecondary.allocationGeneration());
}

void ImageViewportProviderContractTest::providerFactoryRejectsBaseAdapterWithoutSessionFactory()
{
    ImageSequenceFactory factory;
    NullSessionFactoryProviderAdapter adapter;

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Rejected);
    QVERIFY(result->errorString().contains(QStringLiteral("session")));
}

void ImageViewportProviderContractTest::providerFactoryRejectsContradictoryConstructionFacts()
{
    auto verifyConstructionFacts
        = [](const ImageSequenceProviderMetadata& metadata,
              ImageViewportCapabilitySupport timedPlaybackSupport,
              ImageViewportCapabilitySupport frameSeekSupport,
              ImageViewportCapabilitySupport positionSeekSupport, bool expectedAccepted) {
              ImageSequenceFactory factory;
              const auto sessionCount = std::make_shared<int>(0);
              const auto metadataRequestCount = std::make_shared<int>(0);
              const auto frameRequestCount = std::make_shared<int>(0);
              const auto lastRequestedFrame = std::make_shared<int>(-1);
              const auto closeCount = std::make_shared<int>(0);
              auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
                  metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
              CountingProviderAdapter adapter(sessionFactory, metadata, timedPlaybackSupport,
                  frameSeekSupport, positionSeekSupport);

              QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));

              QVERIFY(result);
              QCOMPARE(result->sequence() != nullptr, expectedAccepted);
              QCOMPARE(result->outcome(),
                  expectedAccepted ? ImageSequenceFactoryOutcome::Created
                                   : ImageSequenceFactoryOutcome::Rejected);
              QCOMPARE(*sessionCount, 0);
              QCOMPARE(*metadataRequestCount, 0);
              QCOMPARE(*frameRequestCount, 0);
          };

    verifyConstructionFacts(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }),
        ImageViewportCapabilitySupport::False, ImageViewportCapabilitySupport::Unavailable,
        ImageViewportCapabilitySupport::Unavailable, true);
    verifyConstructionFacts(ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageViewportCapabilitySupport::True, ImageViewportCapabilitySupport::Unavailable,
        ImageViewportCapabilitySupport::Unavailable, false);
    verifyConstructionFacts(ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageViewportCapabilitySupport::Unavailable, ImageViewportCapabilitySupport::False,
        ImageViewportCapabilitySupport::Unavailable, false);
    verifyConstructionFacts(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }),
        ImageViewportCapabilitySupport::Unavailable, ImageViewportCapabilitySupport::Unavailable,
        ImageViewportCapabilitySupport::False, true);
    verifyConstructionFacts(ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageViewportCapabilitySupport::Unavailable, ImageViewportCapabilitySupport::Unavailable,
        ImageViewportCapabilitySupport::True, false);
}

void ImageViewportProviderContractTest::providerFactoryRejectsInvalidKnownMetadata()
{
    auto verifyRejectedKnownMetadata
        = [](const ImageSequenceProviderMetadata& metadata, const QString& expectedDiagnostic) {
              ImageSequenceFactory factory;
              const auto sessionCount = std::make_shared<int>(0);
              const auto metadataRequestCount = std::make_shared<int>(0);
              const auto frameRequestCount = std::make_shared<int>(0);
              const auto lastRequestedFrame = std::make_shared<int>(-1);
              const auto closeCount = std::make_shared<int>(0);
              auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
                  metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
              CountingProviderAdapter adapter(sessionFactory, metadata);

              QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));

              QVERIFY(result);
              QCOMPARE(result->sequence(), nullptr);
              QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Rejected);
              QVERIFY(result->errorString().contains(expectedDiagnostic));
              QCOMPARE(*sessionCount, 0);
              QCOMPARE(*metadataRequestCount, 0);
              QCOMPARE(*frameRequestCount, 0);
          };

    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::still(QSizeF(std::numeric_limits<double>::infinity(), 8.0)),
        QStringLiteral("provider metadata is invalid"));
    verifyRejectedKnownMetadata(ImageSequenceProviderMetadata::still(QSizeF(16.5, 8.0)),
        QStringLiteral("provider metadata is invalid"));
    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 0, 100),
        QStringLiteral("provider metadata is invalid"));
    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {}),
        QStringLiteral("provider metadata is invalid"));
    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 0 }),
        QStringLiteral("positive"));
    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, -1 }),
        QStringLiteral("positive"));
}

void ImageViewportProviderContractTest::providerFactoryRejectsPublishedKnownMetadataLimits()
{
    auto verifyRejectedKnownMetadata
        = [](const ImageSequenceProviderMetadata& metadata, const QString& expectedDiagnostic) {
              ImageSequenceFactory factory;
              const auto sessionCount = std::make_shared<int>(0);
              const auto metadataRequestCount = std::make_shared<int>(0);
              const auto frameRequestCount = std::make_shared<int>(0);
              const auto lastRequestedFrame = std::make_shared<int>(-1);
              const auto closeCount = std::make_shared<int>(0);
              auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
                  metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
              CountingProviderAdapter adapter(sessionFactory, metadata);

              QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));

              QVERIFY(result);
              QCOMPARE(result->sequence(), nullptr);
              QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Rejected);
              QVERIFY(result->errorString().contains(expectedDiagnostic));
              QCOMPARE(*sessionCount, 0);
              QCOMPARE(*metadataRequestCount, 0);
              QCOMPARE(*frameRequestCount, 0);
          };

    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::still(QSizeF(
            static_cast<double>(ImageSequenceLimits::maximumSourceLogicalWidth()) + 1.0, 8.0)),
        QStringLiteral("maximumSourceLogicalWidth"));
    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::still(QSizeF(
            16.0, static_cast<double>(ImageSequenceLimits::maximumSourceLogicalHeight()) + 1.0)),
        QStringLiteral("maximumSourceLogicalHeight"));
    verifyRejectedKnownMetadata(ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0),
                                    QVector<int>(ImageSequenceLimits::maximumFrameCount() + 1, 1)),
        QStringLiteral("maximumFrameCount"));
    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::timedFrameList(
            QSizeF(16.0, 8.0), { ImageSequenceLimits::maximumFrameDurationMilliseconds() + 1 }),
        QStringLiteral("maximumFrameDurationMilliseconds"));
    verifyRejectedKnownMetadata(ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0),
                                    { ImageSequenceLimits::maximumTotalDurationMilliseconds(), 1 }),
        QStringLiteral("maximumTotalDurationMilliseconds"));
}

void ImageViewportProviderContractTest::providerSessionEntryPointsUseSessionAffinity()
{
    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });

    const auto metadataRequestThread = std::make_shared<QThread*>(nullptr);
    const auto frameRequestThread = std::make_shared<QThread*>(nullptr);
    const auto playbackRequestThread = std::make_shared<QThread*>(nullptr);
    const auto cancelRequestThread = std::make_shared<QThread*>(nullptr);
    const auto closeThread = std::make_shared<QThread*>(nullptr);
    const auto releaseThread = std::make_shared<QThread*>(nullptr);
    const auto handleDestructionThread = std::make_shared<QThread*>(nullptr);
    auto sessionFactory
        = std::make_shared<AffinityProviderSessionFactory>(&workerThread, metadataRequestThread,
            frameRequestThread, playbackRequestThread, cancelRequestThread, closeThread);
    CountingProviderAdapter adapter(sessionFactory);
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    {
        ImageViewport item;
        item.setSize(QSizeF(100.0, 100.0));
        item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});

        AffinityProviderSession* session = sessionFactory->lastSession();
        QVERIFY(session);
        QCOMPARE(*metadataRequestThread, &workerThread);

        emitProviderMetadataReady(session, session->lastMetadataToken(),
            ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
        drainQueuedProviderResults();
        QCOMPARE(*frameRequestThread, &workerThread);

        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        auto* handle = new ImageSequenceProviderFrameHandle(
            new ImageFrame(image), [releaseThread](ImageFrame* frame) {
                *releaseThread = QThread::currentThread();
                delete frame;
            });
        handle->moveToThread(&workerThread);
        QObject::connect(handle, &QObject::destroyed, handle,
            [handleDestructionThread]() { *handleDestructionThread = QThread::currentThread(); });
        emitProviderFrameHandleReady(session, session->lastFrameToken(), handle,
            ImageSequenceProviderFrameEnvelope::timedFrame(0, 0, 100));
        drainQueuedProviderResults();
        acknowledgePendingPrimaryRenderCommitForTest(item);

        QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
            ImageViewportCommandOutcome::Accepted);
        advancePlaybackForTest(item, 100);
        QCOMPARE(*playbackRequestThread, &workerThread);
        QVERIFY(session->lastPlaybackToken().isValid());

        QCOMPARE(item.stop(ImageViewportPageRole::Primary).outcome(),
            ImageViewportCommandOutcome::Accepted);
        QCOMPARE(*cancelRequestThread, &workerThread);

        QCOMPARE(item.clear().outcome(), ImageViewportCommandOutcome::Accepted);
        QTRY_COMPARE(*releaseThread, &workerThread);
        QTRY_COMPARE(*handleDestructionThread, &workerThread);
    }

    QTRY_COMPARE(*closeThread, &workerThread);
}

void ImageViewportProviderContractTest::affinityBoundReleaseDoesNotBlockViewportCleanup()
{
    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });

    const auto metadataRequestThread = std::make_shared<QThread*>(nullptr);
    const auto frameRequestThread = std::make_shared<QThread*>(nullptr);
    const auto playbackRequestThread = std::make_shared<QThread*>(nullptr);
    const auto cancelRequestThread = std::make_shared<QThread*>(nullptr);
    const auto closeThread = std::make_shared<QThread*>(nullptr);
    const auto releaseCount = std::make_shared<int>(0);
    const auto releaseThread = std::make_shared<QThread*>(nullptr);
    auto sessionFactory
        = std::make_shared<AffinityProviderSessionFactory>(&workerThread, metadataRequestThread,
            frameRequestThread, playbackRequestThread, cancelRequestThread, closeThread);
    CountingProviderAdapter adapter(sessionFactory);
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    {
        ImageViewport item;
        item.setSize(QSizeF(100.0, 100.0));
        item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});

        AffinityProviderSession* session = sessionFactory->lastSession();
        QVERIFY(session);
        emitProviderMetadataReady(session, session->lastMetadataToken(),
            ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
        drainQueuedProviderResults();

        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        auto* handle = new ImageSequenceProviderFrameHandle(
            new ImageFrame(image), [releaseCount, releaseThread](ImageFrame* frame) {
                *releaseThread = QThread::currentThread();
                QThread::msleep(250);
                ++*releaseCount;
                delete frame;
            });
        handle->moveToThread(&workerThread);
        emitProviderFrameHandleReady(session, session->lastFrameToken(), handle,
            ImageSequenceProviderFrameEnvelope::stillFrame());
        drainQueuedProviderResults();
        acknowledgePendingPrimaryRenderCommitForTest(item);

        QElapsedTimer elapsed;
        elapsed.start();
        QCOMPARE(item.clear().outcome(), ImageViewportCommandOutcome::Accepted);
        const qint64 clearElapsedMilliseconds = elapsed.elapsed();

        QVERIFY2(clearElapsedMilliseconds < 100,
            qPrintable(QStringLiteral("clear() blocked for %1 ms").arg(clearElapsedMilliseconds)));
        QTRY_COMPARE(*releaseCount, 1);
        QCOMPARE(*releaseThread, &workerThread);
        QTRY_COMPARE(*closeThread, &workerThread);
    }
}

void ImageViewportProviderContractTest::providerThreadSafeSessionEntryPointsUseViewportAffinity()
{
    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });

    QThread* viewportThread = QThread::currentThread();
    const auto metadataRequestThread = std::make_shared<QThread*>(nullptr);
    const auto frameRequestThread = std::make_shared<QThread*>(nullptr);
    const auto playbackRequestThread = std::make_shared<QThread*>(nullptr);
    const auto cancelRequestThread = std::make_shared<QThread*>(nullptr);
    const auto closeThread = std::make_shared<QThread*>(nullptr);
    const auto releaseThread = std::make_shared<QThread*>(nullptr);
    const auto handleDestructionThread = std::make_shared<QThread*>(nullptr);
    auto sessionFactory
        = std::make_shared<AffinityProviderSessionFactory>(&workerThread, metadataRequestThread,
            frameRequestThread, playbackRequestThread, cancelRequestThread, closeThread);
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageViewportCapabilitySupport::Unavailable, ImageViewportCapabilitySupport::Unavailable,
        ImageViewportCapabilitySupport::Unavailable,
        ImageSequenceProviderThreadingContract::ThreadSafe);
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    {
        ImageViewport item;
        item.setSize(QSizeF(100.0, 100.0));
        item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});

        AffinityProviderSession* session = sessionFactory->lastSession();
        QVERIFY(session);
        QCOMPARE(*metadataRequestThread, viewportThread);

        emitProviderMetadataReady(session, session->lastMetadataToken(),
            ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
        QCOMPARE(*frameRequestThread, nullptr);
        drainQueuedProviderResults();
        QCOMPARE(*frameRequestThread, viewportThread);

        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        auto* handle = new ImageSequenceProviderFrameHandle(
            new ImageFrame(image), [releaseThread](ImageFrame* frame) {
                *releaseThread = QThread::currentThread();
                delete frame;
            });
        handle->moveToThread(&workerThread);
        QObject::connect(handle, &QObject::destroyed, handle,
            [handleDestructionThread]() { *handleDestructionThread = QThread::currentThread(); });
        emitProviderFrameHandleReady(session, session->lastFrameToken(), handle,
            ImageSequenceProviderFrameEnvelope::timedFrame(0, 0, 100));
        drainQueuedProviderResults();
        acknowledgePendingPrimaryRenderCommitForTest(item);

        QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
            ImageViewportCommandOutcome::Accepted);
        advancePlaybackForTest(item, 100);
        QCOMPARE(*playbackRequestThread, viewportThread);
        QVERIFY(session->lastPlaybackToken().isValid());

        QCOMPARE(item.stop(ImageViewportPageRole::Primary).outcome(),
            ImageViewportCommandOutcome::Accepted);
        QCOMPARE(*cancelRequestThread, viewportThread);

        QCOMPARE(item.clear().outcome(), ImageViewportCommandOutcome::Accepted);
        QCOMPARE(*releaseThread, viewportThread);
        QTRY_COMPARE(*handleDestructionThread, &workerThread);
    }

    QTRY_COMPARE(*closeThread, &workerThread);
}

void ImageViewportProviderContractTest::providerSequenceOpensSessionAfterAdapterDestruction()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);

    QScopedPointer<ImageSequenceFactoryResult> result;
    {
        CountingProviderAdapter adapter(sessionFactory);
        result.reset(factory.fromProvider(&adapter));
    }

    QVERIFY(result);
    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Created);
    QVERIFY(result->sequence());
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryFrameCount(item), -1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), -1);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), -1);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), -1);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), -1);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::Unavailable);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::Unavailable);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::Unavailable);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::True);
}

void ImageViewportProviderContractTest::providerSharedSequenceUsesIndependentViewportSessions()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport first;
    ImageViewport second;
    first.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    second.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = first.metaObject();

    QCOMPARE(*sessionCount, 2);
    QCOMPARE(*metadataRequestCount, 2);
    QCOMPARE(*frameRequestCount, 0);
    CountingProviderSession* firstSession = sessionFactory->sessionAt(0);
    CountingProviderSession* secondSession = sessionFactory->sessionAt(1);
    QVERIFY(firstSession);
    QVERIFY(secondSession);
    QVERIFY(firstSession != secondSession);
    QCOMPARE(firstSession->lastMetadataToken(), secondSession->lastMetadataToken());

    emitProviderMetadataReady(firstSession, firstSession->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(requestStatusValue(first), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(first), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(first), 0);
    QCOMPARE(primaryRequestedPosition(first), -1);
    QCOMPARE(primaryFrameCount(first), 1);
    QCOMPARE(requestStatusValue(second), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(second), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(second), -1);
    QCOMPARE(primaryRequestedPosition(second), -1);
    QCOMPARE(primaryFrameCount(second), -1);

    emitProviderMetadataReady(secondSession, secondSession->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(20.0, 10.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(primaryFrameCount(first), 1);
    QCOMPARE(displayedImageSize(first), QSizeF(0.0, 0.0));
    QCOMPARE(requestStatusValue(second), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(second), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(second), 0);
    QCOMPARE(primaryRequestedPosition(second), -1);
    QCOMPARE(primaryFrameCount(second), 1);
}

void ImageViewportProviderContractTest::providerSessionOpenFailureKeepsReplacementObservable()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> previousResult(factory.fromTimedFrameList(&list));
    QVERIFY(previousResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<FailingProviderSessionFactory>(sessionCount);
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageViewportCapabilitySupport::False, ImageViewportCapabilitySupport::False,
        ImageViewportCapabilitySupport::False);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(factory.fromProvider(&adapter));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(ImageViewportPresentationTarget(previousResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::True);
    const ImageViewportRevisionToken readyDisplayRevision
        = revisionTokenProperty(item, "displayRevision");

    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
        PresentationTargetTransitionPolicy {});

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(primaryFrameCount(item), -1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::False);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::False);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::False);
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("session")));

    const ImageViewportRevisionToken failedRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);
}

void ImageViewportProviderContractTest::reassigningSameProviderSequenceStartsNewGeneration()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken initialRequestRevision
        = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*closeCount, 0);

    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QCOMPARE(*sessionCount, 2);
    QCOMPARE(*metadataRequestCount, 2);
    QCOMPARE(*frameRequestCount, 0);
    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    verifyRevisionChanged(item, "requestRevision", initialRequestRevision);
}

QTEST_MAIN(ImageViewportProviderContractTest)

#include "tst_imageviewport_provider_contract.moc"
