#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

namespace {
ImageSequenceProviderFrameEnvelope exactTestEnvelope(QSizeF logicalSize, QSize payloadSize,
    qint64 payloadBytes, bool hasAlpha, int frame = 0, int frameStart = -1, int frameDuration = -1)
{
    ImageSequenceProviderFrameEnvelope envelope;
    envelope.setSourceLogicalSize(logicalSize);
    envelope.setPayloadRasterSize(QSizeF(payloadSize));
    envelope.setSourceToPayloadScale(QSizeF(
        payloadSize.width() / logicalSize.width(), payloadSize.height() / logicalSize.height()));
    envelope.setPayloadByteSize(payloadBytes);
    envelope.setQuality(ImageViewport::PayloadQuality::Exact);
    envelope.setExactness(ImageViewport::PayloadExactness::ExactForSource);
    envelope.setFrame(frame);
    envelope.setFrameStartPosition(frameStart);
    envelope.setFrameDuration(frameDuration);
    envelope.setHasAlpha(hasAlpha);
    return envelope;
}

class TypedProviderSession final : public ImageSequenceProviderSession
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
            emit providerEvent(ImageSequenceProviderEvent::metadataReady(
                request.token(), ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0))));
        }
    }

    void emitFrameReady(ImageSequenceProviderRequestToken token)
    {
        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        ImageSequenceProviderFrameEnvelope envelope = exactTestEnvelope(
            QSizeF(16.0, 8.0), image.size(), image.sizeInBytes(), image.hasAlphaChannel());
        auto frame = std::make_unique<ImageFrame>(image, envelope);
        emit providerEvent(ImageSequenceProviderEvent::frameReady(
            token, new ImageSequenceProviderFrameHandle(std::move(frame), this), envelope));
    }

    QVector<ImageSequenceProviderRequest> requests;
};

class TypedProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    ImageSequenceProviderSession* createSession(QObject* parent) override
    {
        lastSession = new TypedProviderSession(parent);
        return lastSession;
    }

    QPointer<TypedProviderSession> lastSession;
};

class TypedDescriptorProviderAdapter final : public ImageSequenceProviderAdapter
{
public:
    explicit TypedDescriptorProviderAdapter(
        std::shared_ptr<TypedProviderSessionFactory> factory, QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , m_factory(std::move(factory))
    {
    }

    ImageSequenceProviderDescriptor descriptor() const override
    {
        ImageSequenceProviderDescriptor descriptor;
        descriptor.setSessionFactory(m_factory);
        descriptor.setFrameSeekCapability(ImageSequenceProviderCapabilitySupport::KnownTrue);
        descriptor.setThreadingContract(ImageSequenceProviderThreadingContract::ThreadSafe);
        return descriptor;
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

private slots:
    void providerPublicValueTypesValidateTiming();
    void providerTypedProtocolValuesValidateShape();
    void typedDescriptorFactoryAndSessionBridgeMatchesLegacyPath();
    void providerFactoryRejectsBaseAdapterWithoutSessionFactory();
    void providerFactoryRejectsContradictoryConstructionFacts();
    void providerFactoryRejectsInvalidKnownMetadata();
    void providerFactoryRejectsPublishedKnownMetadataLimits();
    void providerSessionEntryPointsUseSessionAffinity();
    void providerThreadSafeSessionEntryPointsUseControllerAffinity();
    void providerSequenceOpensSessionAfterAdapterDestruction();
    void providerSharedSequenceUsesIndependentViewportSessions();
    void providerSessionOpenFailureKeepsReplacementObservable();
    void reassigningSameProviderSequenceStartsNewGeneration();
};

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
    QCOMPARE(stillMetadata.logicalSize(), QSizeF(16.0, 8.0));
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
            QSizeF(16.0, 8.0), ImageSequenceLimits::maximumTimedListFrameCount() + 2, 100);
    QCOMPARE(overLimitFixedDurationMetadata.isSpecified(), true);
    QCOMPARE(overLimitFixedDurationMetadata.isTimedFrameList(), true);
    QCOMPARE(overLimitFixedDurationMetadata.frameDurations().size(),
        ImageSequenceLimits::maximumTimedListFrameCount() + 1);

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

    const ImageSequenceProviderFrameMetadata emptyFrameMetadata;
    QCOMPARE(emptyFrameMetadata.isValid(), false);
    QCOMPARE(emptyFrameMetadata.isStillFrame(), false);
    QCOMPARE(emptyFrameMetadata.isTimedFrame(), false);
    QCOMPARE(emptyFrameMetadata.frame(), -1);
    QCOMPARE(emptyFrameMetadata.frameStartPosition(), -1);
    QCOMPARE(emptyFrameMetadata.frameDuration(), -1);

    const ImageSequenceProviderFrameMetadata stillFrameMetadata
        = ImageSequenceProviderFrameMetadata::stillFrame();
    QCOMPARE(stillFrameMetadata.isValid(), true);
    QCOMPARE(stillFrameMetadata.isStillFrame(), true);
    QCOMPARE(stillFrameMetadata.isTimedFrame(), false);
    QCOMPARE(stillFrameMetadata.frame(), 0);
    QCOMPARE(stillFrameMetadata.frameStartPosition(), -1);
    QCOMPARE(stillFrameMetadata.frameDuration(), -1);

    const ImageSequenceProviderFrameMetadata timedFrameMetadata
        = ImageSequenceProviderFrameMetadata::timedFrame(1, 100, 250);
    QCOMPARE(timedFrameMetadata.isValid(), true);
    QCOMPARE(timedFrameMetadata.isStillFrame(), false);
    QCOMPARE(timedFrameMetadata.isTimedFrame(), true);
    QCOMPARE(timedFrameMetadata.frame(), 1);
    QCOMPARE(timedFrameMetadata.frameStartPosition(), 100);
    QCOMPARE(timedFrameMetadata.frameDuration(), 250);

    const ImageSequenceProviderFrameMetadata unknownDurationTimedFrameMetadata
        = ImageSequenceProviderFrameMetadata::timedFrame(1, 100);
    QCOMPARE(unknownDurationTimedFrameMetadata.isValid(), true);
    QCOMPARE(unknownDurationTimedFrameMetadata.isTimedFrame(), true);
    QCOMPARE(unknownDurationTimedFrameMetadata.frame(), 1);
    QCOMPARE(unknownDurationTimedFrameMetadata.frameStartPosition(), 100);
    QCOMPARE(unknownDurationTimedFrameMetadata.frameDuration(), -1);

    QCOMPARE(ImageSequenceProviderFrameMetadata::timedFrame(-1, 100).isValid(), false);
    QCOMPARE(ImageSequenceProviderFrameMetadata::timedFrame(1, -1).isValid(), false);
    QCOMPARE(ImageSequenceProviderFrameMetadata::timedFrame(1, 100, 0).isValid(), false);

    const ImageSequenceProviderKnownFacts emptyFacts;
    QCOMPARE(emptyFacts.isSpecified(), false);
    QCOMPARE(emptyFacts.isValid(), false);
    QCOMPARE(emptyFacts.isComplete(), false);
    QCOMPARE(emptyFacts.logicalSize(), QSizeF());
    QCOMPARE(emptyFacts.frameCount(), -1);
    QCOMPARE(emptyFacts.frameDurations(), QVector<int>());

    const ImageSequenceProviderKnownFacts logicalSizeFacts
        = ImageSequenceProviderKnownFacts::logicalSize(QSizeF(16.0, 8.0));
    QCOMPARE(logicalSizeFacts.isSpecified(), true);
    QCOMPARE(logicalSizeFacts.isValid(), true);
    QCOMPARE(logicalSizeFacts.isComplete(), false);
    QCOMPARE(logicalSizeFacts.logicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(logicalSizeFacts.frameCount(), -1);
    QCOMPARE(logicalSizeFacts.frameDurations(), QVector<int>());

    const ImageSequenceProviderKnownFacts stillFacts
        = ImageSequenceProviderKnownFacts::still(QSizeF(16.0, 8.0));
    QCOMPARE(stillFacts.isSpecified(), true);
    QCOMPARE(stillFacts.isValid(), true);
    QCOMPARE(stillFacts.isComplete(), true);
    QCOMPARE(stillFacts.logicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(stillFacts.frameCount(), 1);
    QCOMPARE(stillFacts.frameDurations(), QVector<int>());

    const ImageSequenceProviderKnownFacts countFacts
        = ImageSequenceProviderKnownFacts::timedFrameCount(QSizeF(16.0, 8.0), 3);
    QCOMPARE(countFacts.isSpecified(), true);
    QCOMPARE(countFacts.isValid(), true);
    QCOMPARE(countFacts.isComplete(), false);
    QCOMPARE(countFacts.logicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(countFacts.frameCount(), 3);
    QCOMPARE(countFacts.frameDurations(), QVector<int>());

    const ImageSequenceProviderKnownFacts fixedFacts
        = ImageSequenceProviderKnownFacts::fixedDurationFrames(QSizeF(16.0, 8.0), 3, 100);
    QCOMPARE(fixedFacts.isSpecified(), true);
    QCOMPARE(fixedFacts.isValid(), true);
    QCOMPARE(fixedFacts.isComplete(), true);
    QCOMPARE(fixedFacts.frameCount(), 3);
    QCOMPARE(fixedFacts.frameDurations(), QVector<int>({ 100, 100, 100 }));

    const ImageSequenceProviderKnownFacts listFacts
        = ImageSequenceProviderKnownFacts::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 });
    QCOMPARE(listFacts.isSpecified(), true);
    QCOMPARE(listFacts.isValid(), true);
    QCOMPARE(listFacts.isComplete(), true);
    QCOMPARE(listFacts.frameCount(), 2);
    QCOMPARE(listFacts.frameDurations(), QVector<int>({ 100, 250 }));

    PlaybackFallbackSession session;
    session.request(ImageSequenceProviderRequest::playback(
        token, ImageViewport::PageRole::Primary, 7, 125, {}));
    QCOMPARE(session.frameRequestCount, 1);
    QCOMPARE(session.lastFrameToken, token);
    QCOMPARE(session.lastFrame, 7);
    session.request(ImageSequenceProviderRequest::position(
        token, ImageViewport::PageRole::Primary, 349, 8, {}));
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
    QCOMPARE(demand.role(), ImageViewport::PageRole::Primary);
    QCOMPARE(demand.resolvedFrame(), -1);
    QCOMPARE(demand.requestedPosition(), -1);
    QCOMPARE(demand.maximumTextureSize(), -1);
    QCOMPARE(demand.maximumPayloadBytes(), -1);
    QCOMPARE(demand.displayByteBudget(), -1);
    QCOMPARE(demand.currentPayloadQuality(), ImageViewport::PayloadQuality::Unknown);
    QCOMPARE(demand.currentPayloadExactness(), ImageViewport::PayloadExactness::Unknown);

    demand.setRole(ImageViewport::PageRole::Secondary);
    demand.setResolvedFrame(3);
    demand.setRequestedPosition(125);
    const ImageSequenceProviderRequest metadataRequest
        = ImageSequenceProviderRequest::metadata(token);
    QVERIFY(metadataRequest.isValid());
    QCOMPARE(metadataRequest.kind(), ImageSequenceProviderRequestKind::Metadata);
    QCOMPARE(metadataRequest.token(), token);

    const ImageSequenceProviderRequest frameRequest
        = ImageSequenceProviderRequest::frame(token, ImageViewport::PageRole::Secondary, 3, demand);
    QVERIFY(frameRequest.isValid());
    QCOMPARE(frameRequest.kind(), ImageSequenceProviderRequestKind::Frame);
    QCOMPARE(frameRequest.role(), ImageViewport::PageRole::Secondary);
    QCOMPARE(frameRequest.frame(), 3);
    QCOMPARE(frameRequest.resolvedFrame(), 3);
    QCOMPARE(frameRequest.requestedPosition(), -1);
    QCOMPARE(frameRequest.demand().resolvedFrame(), 3);

    const ImageSequenceProviderRequest positionRequest = ImageSequenceProviderRequest::position(
        token, ImageViewport::PageRole::Secondary, 125, 3, demand);
    QVERIFY(positionRequest.isValid());
    QCOMPARE(positionRequest.kind(), ImageSequenceProviderRequestKind::Position);
    QCOMPARE(positionRequest.requestedPosition(), 125);
    QCOMPARE(positionRequest.resolvedFrame(), 3);

    const ImageSequenceProviderRequest playbackRequest = ImageSequenceProviderRequest::playback(
        token, ImageViewport::PageRole::Secondary, 3, 125, demand);
    QVERIFY(playbackRequest.isValid());
    QCOMPARE(playbackRequest.kind(), ImageSequenceProviderRequestKind::Playback);
    QCOMPARE(playbackRequest.frame(), 3);
    QCOMPARE(playbackRequest.requestedPosition(), 125);

    QVERIFY(ImageSequenceProviderRequest::cancel({ token }).isValid());
    QVERIFY(ImageSequenceProviderRequest::close().isValid());
    QVERIFY(!ImageSequenceProviderRequest::metadata({}).isValid());
    QVERIFY(!ImageSequenceProviderRequest::frame({}, ImageViewport::PageRole::Primary, 0, demand)
            .isValid());
    QVERIFY(!ImageSequenceProviderRequest::cancel({}).isValid());

    const ImageSequenceProviderFrameEnvelope stillEnvelope
        = exactTestEnvelope(QSizeF(16.0, 8.0), QSize(16, 8), 512, true);
    QVERIFY(stillEnvelope.isValid());
    QCOMPARE(stillEnvelope.frame(), 0);
    QCOMPARE(stillEnvelope.frameStartPosition(), -1);
    QCOMPARE(stillEnvelope.frameDuration(), -1);
    QCOMPARE(stillEnvelope.quality(), ImageViewport::PayloadQuality::Exact);
    QCOMPARE(stillEnvelope.exactness(), ImageViewport::PayloadExactness::ExactForSource);

    ImageSequenceProviderFrameEnvelope invalidEnvelope = stillEnvelope;
    invalidEnvelope.setSourceToPayloadScale(QSizeF(2.0, 1.0));
    QVERIFY(!invalidEnvelope.isValid());
    invalidEnvelope = stillEnvelope;
    invalidEnvelope.setQuality(ImageViewport::PayloadQuality::Unknown);
    QVERIFY(!invalidEnvelope.isValid());

    const ImageSequenceProviderFrameEnvelope timedEnvelope
        = exactTestEnvelope(QSizeF(16.0, 8.0), QSize(16, 8), 512, true, 2, 250, 125);
    QVERIFY(timedEnvelope.isValid());
    QCOMPARE(timedEnvelope.frame(), 2);
    QCOMPARE(timedEnvelope.frameStartPosition(), 250);
    QCOMPARE(timedEnvelope.frameDuration(), 125);

    auto frame = std::make_unique<ImageFrame>(
        QImage(QSize(16, 8), QImage::Format_ARGB32_Premultiplied), stillEnvelope);
    ImageSequenceProviderFrameHandle handle(std::move(frame));
    QVERIFY(ImageSequenceProviderEvent::metadataReady(
        token, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)))
            .isValid());
    QVERIFY(ImageSequenceProviderEvent::frameReady(token, &handle, stillEnvelope).isValid());
    QVERIFY(ImageSequenceProviderEvent::waiting(token).isValid());
    QVERIFY(ImageSequenceProviderEvent::progress(token, 0.5).isValid());
    QVERIFY(!ImageSequenceProviderEvent::progress(token, 1.5).isValid());
    QVERIFY(ImageSequenceProviderEvent::endOfSequence(token).isValid());
    QVERIFY(ImageSequenceProviderEvent::unsupported(token,
        ImageSequenceProviderUnsupportedCause::UnsupportedRequest, QStringLiteral("unsupported"))
            .isValid());
    QVERIFY(!ImageSequenceProviderEvent::unsupported(
        token, static_cast<ImageSequenceProviderUnsupportedCause>(-1), QStringLiteral("bad"))
            .isValid());
    QVERIFY(ImageSequenceProviderEvent::cancelled(token).isValid());
    QVERIFY(ImageSequenceProviderEvent::failed(token, QStringLiteral("failed")).isValid());
    QVERIFY(!ImageSequenceProviderEvent::waiting({}).isValid());

    const ImageSequenceProviderDescriptor descriptor;
    QVERIFY(!descriptor.isValid());
    QCOMPARE(descriptor.threadingContract(), ImageSequenceProviderThreadingContract::AffinityBound);
}

void ImageViewportProviderContractTest::typedDescriptorFactoryAndSessionBridgeMatchesLegacyPath()
{
    ImageSequenceFactory factory;
    auto sessionFactory = std::make_shared<TypedProviderSessionFactory>();
    TypedDescriptorProviderAdapter adapter(sessionFactory);

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result);
    QVERIFY(result->sequence());
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Created);

    ImageViewport item;
    item.setSize(QSizeF(100.0, 50.0));
    useSynchronousProviderEventDeliveryForTest(item);

    item.setSequence(result->sequence());

    QVERIFY(sessionFactory->lastSession);
    QVERIFY(sessionFactory->lastSession->requests.size() >= 2);
    const ImageSequenceProviderRequest metadataRequest
        = sessionFactory->lastSession->requests.at(0);
    QCOMPARE(metadataRequest.kind(), ImageSequenceProviderRequestKind::Metadata);
    QVERIFY(metadataRequest.token().isValid());

    const ImageSequenceProviderRequest frameRequest = sessionFactory->lastSession->requests.at(1);
    QCOMPARE(frameRequest.kind(), ImageSequenceProviderRequestKind::Frame);
    QCOMPARE(frameRequest.token().isValid(), true);
    QCOMPARE(frameRequest.role(), ImageViewport::PageRole::Primary);
    QCOMPARE(frameRequest.frame(), 0);
    QCOMPARE(frameRequest.demand().role(), ImageViewport::PageRole::Primary);
    QCOMPARE(frameRequest.demand().resolvedFrame(), 0);
    QCOMPARE(frameRequest.demand().requestedPosition(), -1);
    QCOMPARE(frameRequest.demand().demandRevision().isValid(), false);

    sessionFactory->lastSession->emitFrameReady(frameRequest.token());

    QCOMPARE(requestStatus(item), ImageViewport::RequestStatus::Loading);
    QCOMPARE(requestReason(item), ImageViewport::RequestReason::UploadPending);
    QVERIFY(hasPendingRenderCommitForTest(item));
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(requestStatus(item), ImageViewport::RequestStatus::Ready);
    QCOMPARE(requestReason(item), ImageViewport::RequestReason::Ready);
    QCOMPARE(displayStatus(item), ImageViewport::DisplayStatus::Ready);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
}

void ImageViewportProviderContractTest::providerFactoryRejectsBaseAdapterWithoutSessionFactory()
{
    ImageSequenceFactory factory;
    NullSessionFactoryProviderAdapter adapter;

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
    QVERIFY(result->errorString().contains(QStringLiteral("session")));
}

void ImageViewportProviderContractTest::providerFactoryRejectsContradictoryConstructionFacts()
{
    auto verifyRejectedConstructionFacts
        = [](const ImageSequenceProviderMetadata& metadata,
              ImageSequenceProviderAdapter::CapabilitySupport timedPlaybackSupport,
              ImageSequenceProviderAdapter::CapabilitySupport frameSeekSupport,
              ImageSequenceProviderAdapter::CapabilitySupport positionSeekSupport) {
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
              QCOMPARE(result->sequence(), nullptr);
              QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
              QVERIFY(result->errorString().contains(QStringLiteral("provider metadata")));
              QCOMPARE(*sessionCount, 0);
              QCOMPARE(*metadataRequestCount, 0);
              QCOMPARE(*frameRequestCount, 0);
          };

    verifyRejectedConstructionFacts(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }),
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable);
    verifyRejectedConstructionFacts(ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable);
    verifyRejectedConstructionFacts(ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable);
    verifyRejectedConstructionFacts(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }),
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse);
    verifyRejectedConstructionFacts(ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredTrue);
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
              QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
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
              QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
              QVERIFY(result->errorString().contains(expectedDiagnostic));
              QCOMPARE(*sessionCount, 0);
              QCOMPARE(*metadataRequestCount, 0);
              QCOMPARE(*frameRequestCount, 0);
          };

    verifyRejectedKnownMetadata(ImageSequenceProviderMetadata::still(
                                    QSizeF(ImageSequenceLimits::maximumLogicalWidth() + 1, 8.0)),
        QStringLiteral("maximumLogicalWidth"));
    verifyRejectedKnownMetadata(ImageSequenceProviderMetadata::still(
                                    QSizeF(16.0, ImageSequenceLimits::maximumLogicalHeight() + 1)),
        QStringLiteral("maximumLogicalHeight"));
    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0),
            QVector<int>(ImageSequenceLimits::maximumTimedListFrameCount() + 1, 1)),
        QStringLiteral("maximumTimedListFrameCount"));
    verifyRejectedKnownMetadata(ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0),
                                    { ImageSequenceLimits::maximumFrameDuration() + 1 }),
        QStringLiteral("maximumFrameDuration"));
    verifyRejectedKnownMetadata(ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0),
                                    { ImageSequenceLimits::maximumTotalSequenceDuration(), 1 }),
        QStringLiteral("maximumTotalSequenceDuration"));
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
        item.setSequence(result->sequence());

        AffinityProviderSession* session = sessionFactory->lastSession();
        QVERIFY(session);
        QCOMPARE(*metadataRequestThread, &workerThread);

        emitProviderMetadataReady(session, session->lastMetadataToken(),
            ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
        drainQueuedProviderResults();
        QCOMPARE(*frameRequestThread, &workerThread);

        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        ImageFrame frame(image);
        emitProviderFrameReady(session, session->lastFrameToken(), &frame,
            ImageSequenceProviderFrameMetadata::timedFrame(0, 0, 100));
        drainQueuedProviderResults();
        acknowledgePendingPrimaryRenderCommitForTest(item);

        QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
        advancePlaybackForTest(item, 100);
        QCOMPARE(*playbackRequestThread, &workerThread);
        QVERIFY(session->lastPlaybackToken().isValid());

        QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
        QCOMPARE(*cancelRequestThread, &workerThread);
    }

    QTRY_COMPARE(*closeThread, &workerThread);
}

void ImageViewportProviderContractTest::providerThreadSafeSessionEntryPointsUseControllerAffinity()
{
    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });

    QThread* controllerThread = QThread::currentThread();
    const auto metadataRequestThread = std::make_shared<QThread*>(nullptr);
    const auto frameRequestThread = std::make_shared<QThread*>(nullptr);
    const auto playbackRequestThread = std::make_shared<QThread*>(nullptr);
    const auto cancelRequestThread = std::make_shared<QThread*>(nullptr);
    const auto closeThread = std::make_shared<QThread*>(nullptr);
    auto sessionFactory
        = std::make_shared<AffinityProviderSessionFactory>(&workerThread, metadataRequestThread,
            frameRequestThread, playbackRequestThread, cancelRequestThread, closeThread);
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderThreadingContract::ThreadSafe);
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    {
        ImageViewport item;
        item.setSize(QSizeF(100.0, 100.0));
        item.setSequence(result->sequence());

        AffinityProviderSession* session = sessionFactory->lastSession();
        QVERIFY(session);
        QCOMPARE(*metadataRequestThread, controllerThread);

        emitProviderMetadataReady(session, session->lastMetadataToken(),
            ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
        QCOMPARE(*frameRequestThread, nullptr);
        drainQueuedProviderResults();
        QCOMPARE(*frameRequestThread, controllerThread);

        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        ImageFrame frame(image);
        emitProviderFrameReady(session, session->lastFrameToken(), &frame,
            ImageSequenceProviderFrameMetadata::timedFrame(0, 0, 100));
        drainQueuedProviderResults();
        acknowledgePendingPrimaryRenderCommitForTest(item);

        QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
        advancePlaybackForTest(item, 100);
        QCOMPARE(*playbackRequestThread, controllerThread);
        QVERIFY(session->lastPlaybackToken().isValid());

        QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
        QCOMPARE(*cancelRequestThread, controllerThread);
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
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Created);
    QVERIFY(result->sequence());
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryFrameCount(item), -1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), -1);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), -1);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), -1);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), -1);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewport::CapabilitySupport::Unavailable);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewport::CapabilitySupport::Unavailable);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewport::CapabilitySupport::Unavailable);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(
        primaryTimedPlaybackSupport(item), ImageViewport::CapabilitySupport::True);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewport::CapabilitySupport::True);
    QCOMPARE(
        primaryPositionSeekSupport(item), ImageViewport::CapabilitySupport::True);
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
    first.setSequence(result->sequence());
    second.setSequence(result->sequence());
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
    QCOMPARE(
        requestStatusValue(first), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(first),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(first), 0);
    QCOMPARE(primaryRequestedPosition(first), -1);
    QCOMPARE(primaryFrameCount(first), 1);
    QCOMPARE(requestStatusValue(second),
        enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(second),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(second), -1);
    QCOMPARE(primaryRequestedPosition(second), -1);
    QCOMPARE(primaryFrameCount(second), -1);

    emitProviderMetadataReady(secondSession, secondSession->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(20.0, 10.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(primaryFrameCount(first), 1);
    QCOMPARE(displayedImageSize(first), QSizeF(0.0, 0.0));
    QCOMPARE(requestStatusValue(second),
        enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(second),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
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
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(factory.fromProvider(&adapter));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(previousResult->sequence());
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(
        primaryTimedPlaybackSupport(item), ImageViewport::CapabilitySupport::True);
    const ImageViewportRevisionToken readyDisplayRevision = revisionTokenProperty(item, "displayRevision");

    item.setSequence(replacementResult->sequence());

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(primaryFrameCount(item), -1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(
        primaryTimedPlaybackSupport(item), ImageViewport::CapabilitySupport::False);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewport::CapabilitySupport::False);
    QCOMPARE(
        primaryPositionSeekSupport(item), ImageViewport::CapabilitySupport::False);
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("session")));

    const ImageViewportRevisionToken failedRequestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken initialRequestRevision = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*closeCount, 0);

    item.setSequence(result->sequence());

    QCOMPARE(*sessionCount, 2);
    QCOMPARE(*metadataRequestCount, 2);
    QCOMPARE(*frameRequestCount, 0);
    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    verifyRevisionChanged(item, "requestRevision", initialRequestRevision);
}

QTEST_MAIN(ImageViewportProviderContractTest)

#include "tst_imageviewport_provider_contract.moc"
