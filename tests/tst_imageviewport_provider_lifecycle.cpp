#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

#include <utility>

class ImageViewportProviderLifecycleTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderLifecycleTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void replacementClearsRetainedDisplayDiagnostics();
    void providerTokenOverflowClosesSessionWithoutInvalidRequest();
    void secondaryProviderTokenOverflowClosesSessionWithoutMetadataRequest();
    void providerKnownMetadataTokenOverflowClosesSessionWithoutFrameRequest();
    void providerTokenOverflowDoesNotPoisonReplacementSession();
    void providerTokenOverflowDuringSeekFailsAcceptedRequest();
    void providerAssignmentPublishesBeforeDispatchFailure();
    void providerMetadataDispatchFailureClosesSessionAndReportsProviderFailure();
    void providerFrameDispatchFailureClosesSessionAndReportsProviderFailure();
    void secondaryProviderMetadataDispatchFailureClosesSessionAndReportsProviderFailure();
    void primarySessionOpenFailureStopsSecondarySessionCreationForSpread();
    void secondarySessionOpenFailureIsGenerationTerminalForSpread();
    void providerSessionClosesWhenViewportIsDestroyed();
    void providerDestructionCancelsActiveFrameRequestBeforeClose();
    void providerReplacementCancelsActiveFrameRequestBeforeClose();
    void providerClearCancelsActiveFrameRequestBeforeClose();
    void providerClearCancelsPrimaryAndSecondaryMetadataRequestsBeforeClose();
    void providerClearIgnoresLateSecondaryCallbacks();
    void providerClearDoesNotBlockOnSessionCleanup();
    void providerTransportFakeRunsCancellationCloseAndDispatchSynchronously();
    void providerCancelDeliveryFailurePreservesQueuedRequestState();
    void providerCloseDeliveryFailureRecordsDiagnosticAndPreservesClearState();
    void providerCloseDeliveryFailureRetriesCleanupOnDestructionAndIgnoresLateCallbacks();
    void providerNullSequenceCancelsActiveFrameRequestBeforeClose();
    void providerReplacementIgnoresCancelledMetadataAcknowledgement();
    void providerClearIgnoresCancelledMetadataAcknowledgement();
    void providerClosedGenerationTokenCollisionIsIgnoredAfterClear();
    void providerClearIgnoresCancelledFrameAcknowledgement();
    void providerResultsAreQueuedFromSessionEntryPoint();
    void synchronousProviderEventDeliveryBypassesEventLoopForProtocolTests();
    void providerQueuedMetadataFromClosedGenerationIsIgnoredAfterReplacement();
    void providerFrameResultsAreQueuedFromSessionEntryPoint();
    void providerTerminalResultsAreQueuedFromSessionEntryPoint();
    void providerUnsupportedResultsAreQueuedFromSessionEntryPoint();
};

void ImageViewportProviderLifecycleTest::replacementClearsRetainedDisplayDiagnostics()
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
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromTimedFrameList(&list));
    QVERIFY(firstResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(factory.fromProvider(&adapter));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(ImageViewportPresentationTarget(firstResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);

    item.setSize(QSizeF(0.0, 100.0));
    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);
    item.setSize(QSizeF(100.0, 100.0));
    acknowledgePendingPrimaryRenderFailureForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("render commit failed")));

    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
        PresentationTargetTransitionPolicy {});

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(viewportWarningString(item), QString());
}

void ImageViewportProviderLifecycleTest::providerTokenOverflowClosesSessionWithoutInvalidRequest()
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
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);

    ImageViewport item;
    useSynchronousProviderExecutorForTest(item);
    setNextProviderRequestTokenForTest(item, std::numeric_limits<quint64>::max());
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(sessionFactory->lastSession(), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider request token")));
}

void ImageViewportProviderLifecycleTest::
    secondaryProviderTokenOverflowClosesSessionWithoutMetadataRequest()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromProvider(&adapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    useSynchronousProviderExecutorForTest(item);
    item.setSize(QSizeF(100.0, 100.0));
    setNextProviderRequestTokenForTest(
        item, ImageViewport::PageRole::Secondary, std::numeric_limits<quint64>::max());
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(sessionFactory->lastSession(), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), -1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider request token")));
}

void ImageViewportProviderLifecycleTest::
    providerKnownMetadataTokenOverflowClosesSessionWithoutFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    useSynchronousProviderExecutorForTest(item);
    setNextProviderRequestTokenForTest(item, std::numeric_limits<quint64>::max());
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(sessionFactory->lastSession(), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider request token")));
}

void ImageViewportProviderLifecycleTest::providerTokenOverflowDoesNotPoisonReplacementSession()
{
    ImageSequenceFactory factory;
    const auto firstSessionCount = std::make_shared<int>(0);
    const auto firstMetadataRequestCount = std::make_shared<int>(0);
    const auto firstFrameRequestCount = std::make_shared<int>(0);
    const auto firstLastRequestedFrame = std::make_shared<int>(-1);
    const auto firstCloseCount = std::make_shared<int>(0);
    auto firstSessionFactory = std::make_shared<CountingProviderSessionFactory>(firstSessionCount,
        firstMetadataRequestCount, firstFrameRequestCount, firstLastRequestedFrame,
        firstCloseCount);
    CountingProviderAdapter firstAdapter(firstSessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromProvider(&firstAdapter));
    QVERIFY(firstResult->sequence());

    ImageViewport item;
    setNextProviderRequestTokenForTest(item, std::numeric_limits<quint64>::max());
    item.setPresentationTarget(ImageViewportPresentationTarget(firstResult->sequence()),
        PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*firstSessionCount, 1);
    QCOMPARE(*firstMetadataRequestCount, 0);
    drainQueuedProviderResults();
    QCOMPARE(*firstCloseCount, 1);
    QCOMPARE(firstSessionFactory->lastSession(), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));

    const auto replacementSessionCount = std::make_shared<int>(0);
    const auto replacementMetadataRequestCount = std::make_shared<int>(0);
    const auto replacementFrameRequestCount = std::make_shared<int>(0);
    const auto replacementLastRequestedFrame = std::make_shared<int>(-1);
    const auto replacementCloseCount = std::make_shared<int>(0);
    auto replacementSessionFactory = std::make_shared<CountingProviderSessionFactory>(
        replacementSessionCount, replacementMetadataRequestCount, replacementFrameRequestCount,
        replacementLastRequestedFrame, replacementCloseCount);
    CountingProviderAdapter replacementAdapter(replacementSessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromProvider(&replacementAdapter));
    QVERIFY(replacementResult->sequence());

    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
        PresentationTargetTransitionPolicy {});

    QCOMPARE(*replacementSessionCount, 1);
    QCOMPARE(*replacementMetadataRequestCount, 1);
    QCOMPARE(*replacementFrameRequestCount, 0);
    QCOMPARE(*replacementCloseCount, 0);
    QVERIFY(replacementSessionFactory->lastSession());
    QCOMPARE(replacementSessionFactory->lastSession()->lastMetadataToken(),
        providerRequestTokenForTest(1));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QVERIFY(!viewportErrorString(item).contains(QStringLiteral("provider request token")));
}

void ImageViewportProviderLifecycleTest::providerTokenOverflowDuringSeekFailsAcceptedRequest()
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

    ImageViewport item;
    useSynchronousProviderExecutorForTest(item);
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));

    setNextProviderRequestTokenForTest(item, std::numeric_limits<quint64>::max());
    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(sessionFactory->lastSession(), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider request token")));
}

void ImageViewportProviderLifecycleTest::providerAssignmentPublishesBeforeDispatchFailure()
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

    ImageViewport item;
    useSynchronousProviderExecutorForTest(item);
    failNextProviderCommandDeliveryForTest(item, ImageViewport::PageRole::Primary);
    const QMetaObject* metaObject = item.metaObject();
    const int loadingStatus = enumValue(metaObject, "RequestStatus", "Loading");
    const int providerWaitingReason = enumValue(metaObject, "RequestReason", "ProviderWaiting");
    const int errorStatus = enumValue(metaObject, "RequestStatus", "Error");
    const int providerFailureReason = enumValue(metaObject, "RequestReason", "ProviderFailure");

    struct SignalSnapshot
    {
        QByteArray signal;
        int status = -1;
        int reason = -1;
        bool assigned = false;
    };
    QVector<SignalSnapshot> snapshots;
    auto record = [&](QByteArray signal) {
        snapshots.append({ std::move(signal), requestStatusValue(item), requestReasonValue(item),
            viewportPrimarySequence(item) == result->sequence() });
    };
    QObject::connect(&item, &ImageViewport::stateChanged, [&]() { record("state"); });

    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    int loadingIndex = -1;
    int errorIndex = -1;
    for (qsizetype index = 0; index < snapshots.size(); ++index) {
        const SignalSnapshot& snapshot = snapshots.at(index);
        if (loadingIndex < 0 && snapshot.signal == "state" && snapshot.assigned
            && snapshot.status == loadingStatus && snapshot.reason == providerWaitingReason) {
            loadingIndex = int(index);
        }
        if (errorIndex < 0 && snapshot.signal == "state" && snapshot.assigned
            && snapshot.status == errorStatus && snapshot.reason == providerFailureReason) {
            errorIndex = int(index);
        }
    }

    QVERIFY(loadingIndex >= 0);
    QVERIFY(errorIndex >= 0);
    QVERIFY(loadingIndex < errorIndex);
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(sessionFactory->lastSession(), nullptr);
    QCOMPARE(viewportPrimarySequence(item), result->sequence());
    QCOMPARE(requestStatusValue(item), errorStatus);
    QCOMPARE(requestReasonValue(item), providerFailureReason);
}

void ImageViewportProviderLifecycleTest::
    providerMetadataDispatchFailureClosesSessionAndReportsProviderFailure()
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

    ImageViewport item;
    useSynchronousProviderExecutorForTest(item);
    failNextProviderCommandDeliveryForTest(item, ImageViewport::PageRole::Primary);
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider command delivery failed")));

    QCOMPARE(*closeCount, 1);
    QCOMPARE(sessionFactory->lastSession(), nullptr);
}

void ImageViewportProviderLifecycleTest::
    providerFrameDispatchFailureClosesSessionAndReportsProviderFailure()
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

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();
    QVERIFY(sessionFactory->lastSession());
    QCOMPARE(*metadataRequestCount, 1);

    failNextProviderCommandDeliveryForTest(item, ImageViewport::PageRole::Primary);
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider command delivery failed")));

    QCOMPARE(*closeCount, 1);
    QCOMPARE(sessionFactory->lastSession(), nullptr);
}

void ImageViewportProviderLifecycleTest::
    secondaryProviderMetadataDispatchFailureClosesSessionAndReportsProviderFailure()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame primaryFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    useSynchronousProviderExecutorForTest(item);
    failNextProviderCommandDeliveryForTest(item, ImageViewport::PageRole::Secondary);
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*secondarySessionCount, 1);
    QCOMPARE(*secondaryMetadataRequestCount, 0);
    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider command delivery failed")));

    QCOMPARE(*secondaryCloseCount, 1);
    QCOMPARE(secondarySessionFactory->lastSession(), nullptr);
}

void ImageViewportProviderLifecycleTest::
    primarySessionOpenFailureStopsSecondarySessionCreationForSpread()
{
    ImageSequenceFactory factory;

    const auto primarySessionCount = std::make_shared<int>(0);
    auto primarySessionFactory
        = std::make_shared<FailingProviderSessionFactory>(primarySessionCount);
    CountingProviderAdapter primaryAdapter(primarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromProvider(&primaryAdapter));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*primarySessionCount, 1);
    QCOMPARE(*secondarySessionCount, 0);
    QCOMPARE(*secondaryMetadataRequestCount, 0);
    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(*secondaryCloseCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider session creation failed")));
}

void ImageViewportProviderLifecycleTest::secondarySessionOpenFailureIsGenerationTerminalForSpread()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame primaryFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    auto secondarySessionFactory
        = std::make_shared<FailingProviderSessionFactory>(secondarySessionCount);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*secondarySessionCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider session creation failed")));

    QCOMPARE(item.seek(ImageViewport::PageRole::Secondary, 0).outcome(),
        ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.play(ImageViewport::PageRole::Secondary).outcome(),
        ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));

    QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderLifecycleTest::providerSessionClosesWhenViewportIsDestroyed()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto sessionFactory
        = std::make_shared<CountingProviderSessionFactory>(sessionCount, metadataRequestCount,
            frameRequestCount, lastRequestedFrame, closeCount, std::shared_ptr<int>(),
            std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount, lastCancelledToken);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageSequenceProviderRequestToken metadataToken;
    {
        ImageViewport item;
        item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});
        QCOMPARE(*sessionCount, 1);
        QCOMPARE(*metadataRequestCount, 1);
        QVERIFY(sessionFactory->lastSession());
        metadataToken = sessionFactory->lastSession()->lastMetadataToken();
        QVERIFY(metadataToken.isValid());
        QCOMPARE(*cancelRequestCount, 0);
        QCOMPARE(*closeCount, 0);
    }

    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledToken, metadataToken);
    QCOMPARE(*closeCount, 1);
}

void ImageViewportProviderLifecycleTest::providerDestructionCancelsActiveFrameRequestBeforeClose()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto sessionFactory
        = std::make_shared<CountingProviderSessionFactory>(sessionCount, metadataRequestCount,
            frameRequestCount, lastRequestedFrame, closeCount, std::shared_ptr<int>(),
            std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount, lastCancelledToken);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageSequenceProviderRequestToken frameToken;
    {
        ImageViewport item;
        item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});

        QVERIFY(sessionFactory->lastSession());
        emitProviderMetadataReady(sessionFactory->lastSession(),
            sessionFactory->lastSession()->lastMetadataToken(),
            ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
        drainQueuedProviderResults();

        frameToken = sessionFactory->lastSession()->lastFrameToken();
        QVERIFY(frameToken.isValid());
        QCOMPARE(*frameRequestCount, 1);
        QCOMPARE(*cancelRequestCount, 0);
        QCOMPARE(*closeCount, 0);
    }

    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledToken, frameToken);
    QCOMPARE(*closeCount, 1);
}

void ImageViewportProviderLifecycleTest::providerReplacementCancelsActiveFrameRequestBeforeClose()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto sessionFactory
        = std::make_shared<CountingProviderSessionFactory>(sessionCount, metadataRequestCount,
            frameRequestCount, lastRequestedFrame, closeCount, std::shared_ptr<int>(),
            std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount, lastCancelledToken);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QImage replacementImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::transparent);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 1);

    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);

    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledToken, frameToken);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
}

void ImageViewportProviderLifecycleTest::providerClearCancelsActiveFrameRequestBeforeClose()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto sessionFactory
        = std::make_shared<CountingProviderSessionFactory>(sessionCount, metadataRequestCount,
            frameRequestCount, lastRequestedFrame, closeCount, std::shared_ptr<int>(),
            std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount, lastCancelledToken);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    useSynchronousProviderExecutorForTest(item);
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 1);

    QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);

    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledToken, frameToken);
    QCOMPARE(*closeCount, 1);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
}

void ImageViewportProviderLifecycleTest::
    providerClearCancelsPrimaryAndSecondaryMetadataRequestsBeforeClose()
{
    ImageSequenceFactory factory;
    const auto primarySessionCount = std::make_shared<int>(0);
    const auto primaryMetadataRequestCount = std::make_shared<int>(0);
    const auto primaryFrameRequestCount = std::make_shared<int>(0);
    const auto primaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto primaryCloseCount = std::make_shared<int>(0);
    const auto primaryCancelRequestCount = std::make_shared<int>(0);
    const auto primaryLastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto primarySessionFactory
        = std::make_shared<CountingProviderSessionFactory>(primarySessionCount,
            primaryMetadataRequestCount, primaryFrameRequestCount, primaryLastRequestedFrame,
            primaryCloseCount, std::shared_ptr<int>(), std::shared_ptr<int>(),
            std::shared_ptr<int>(), primaryCancelRequestCount, primaryLastCancelledToken);
    CountingProviderAdapter primaryAdapter(primarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromProvider(&primaryAdapter));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    const auto secondaryCancelRequestCount = std::make_shared<int>(0);
    const auto secondaryLastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto secondarySessionFactory
        = std::make_shared<CountingProviderSessionFactory>(secondarySessionCount,
            secondaryMetadataRequestCount, secondaryFrameRequestCount, secondaryLastRequestedFrame,
            secondaryCloseCount, std::shared_ptr<int>(), std::shared_ptr<int>(),
            std::shared_ptr<int>(), secondaryCancelRequestCount, secondaryLastCancelledToken);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();
    QVERIFY(primarySessionFactory->lastSession());
    QVERIFY(secondarySessionFactory->lastSession());
    const ImageSequenceProviderRequestToken primaryMetadataToken
        = primarySessionFactory->lastSession()->lastMetadataToken();
    const ImageSequenceProviderRequestToken secondaryMetadataToken
        = secondarySessionFactory->lastSession()->lastMetadataToken();
    QVERIFY(primaryMetadataToken.isValid());
    QVERIFY(secondaryMetadataToken.isValid());
    QCOMPARE(*primarySessionCount, 1);
    QCOMPARE(*primaryMetadataRequestCount, 1);
    QCOMPARE(*secondarySessionCount, 1);
    QCOMPARE(*secondaryMetadataRequestCount, 1);

    QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);

    drainQueuedProviderResults();
    QCOMPARE(*primaryCancelRequestCount, 1);
    QCOMPARE(*primaryLastCancelledToken, primaryMetadataToken);
    QCOMPARE(*primaryCloseCount, 1);
    QCOMPARE(*secondaryCancelRequestCount, 1);
    QCOMPARE(*secondaryLastCancelledToken, secondaryMetadataToken);
    QCOMPARE(*secondaryCloseCount, 1);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
}

void ImageViewportProviderLifecycleTest::providerClearIgnoresLateSecondaryCallbacks()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto sessionFactory
        = std::make_shared<CountingProviderSessionFactory>(sessionCount, metadataRequestCount,
            frameRequestCount, lastRequestedFrame, closeCount, std::shared_ptr<int>(),
            std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount, lastCancelledToken);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromProvider(&adapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();
    QPointer<CountingProviderSession> secondarySession = sessionFactory->lastSession();
    QVERIFY(secondarySession);
    const ImageSequenceProviderRequestToken metadataToken = secondarySession->lastMetadataToken();
    QVERIFY(metadataToken.isValid());

    emitProviderMetadataReady(
        secondarySession, metadataToken, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken frameToken = secondarySession->lastFrameToken();
    QVERIFY(frameToken.isValid());
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(secondaryRequestedFrame(item), 0);

    QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageViewportRevisionToken clearedRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken clearedDisplayRevision
        = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken clearedCommandRevision
        = revisionTokenProperty(item, "commandRevision");
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QImage lateImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    lateImage.fill(Qt::transparent);
    ImageFrame lateFrame(lateImage);
    emitProviderWaiting(secondarySession, metadataToken);
    emitProviderProgress(secondarySession, metadataToken, 0.5);
    emitProviderMetadataReady(
        secondarySession, metadataToken, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    emitProviderFrameReady(secondarySession, frameToken, &lateFrame);
    emitProviderCancelled(secondarySession, frameToken, QStringLiteral("late cancellation"));
    emitProviderUnsupported(secondarySession, frameToken,
        ImageSequenceProviderSession::UnsupportedCause::PayloadRejection,
        QStringLiteral("late unsupported"));
    emitProviderFailed(secondarySession, frameToken, QStringLiteral("late failure"));
    emitProviderEndOfSequence(secondarySession, frameToken);

    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledToken, frameToken);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), clearedRequestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), clearedDisplayRevision);
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), clearedCommandRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(secondaryRequestedFrame(item), -1);
    QCOMPARE(secondaryDisplayedFrame(item), -1);
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(viewportWarningString(item), QString());
}

void ImageViewportProviderLifecycleTest::providerClearDoesNotBlockOnSessionCleanup()
{
    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });

    ImageSequenceFactory factory;
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<SlowCleanupProviderSessionFactory>(
        &workerThread, cancelRequestCount, closeCount, 250);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    QVERIFY(sessionFactory->lastSession());
    QVERIFY(sessionFactory->lastSession()->lastMetadataToken().isValid());

    QElapsedTimer elapsed;
    elapsed.start();
    QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);
    const qint64 clearElapsedMilliseconds = elapsed.elapsed();

    QVERIFY2(clearElapsedMilliseconds < 100,
        qPrintable(QStringLiteral("clear() blocked for %1 ms").arg(clearElapsedMilliseconds)));
    QTRY_COMPARE(*cancelRequestCount, 1);
    QTRY_COMPARE(*closeCount, 1);
}

void ImageViewportProviderLifecycleTest::
    providerTransportFakeRunsCancellationCloseAndDispatchSynchronously()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    useSynchronousProviderExecutorForTest(item);
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 0);

    QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(sessionFactory->lastSession(), nullptr);

    failNextProviderCommandDeliveryForTest(item, ImageViewport::PageRole::Primary);
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    QCOMPARE(*sessionCount, 2);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 2);
    QCOMPARE(sessionFactory->lastSession(), nullptr);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
}

void ImageViewportProviderLifecycleTest::providerCancelDeliveryFailurePreservesQueuedRequestState()
{
    struct Snapshot
    {
        int requestStatus = -1;
        int requestReason = -1;
        int displayStatus = -1;
        int playbackPhase = -1;
        int requestedFrame = -1;
        int requestedPosition = -1;
        int displayedFrame = -1;
        int displayedPosition = -1;
        ImageViewportRevisionToken requestRevision;
        ImageViewportRevisionToken displayRevision;
        QSizeF displayedImageSize;
        QString errorString;
        QString warningString;
        int frameRequestCount = 0;
        int cancelRequestCount = 0;
        quint64 cancelTokenValue = 0;
        ProviderTransportDiagnosticForTest diagnostic;
    };

    const auto runScenario = [](bool failCancelDelivery, Snapshot& snapshot) {
        ImageSequenceFactory factory;
        const auto sessionCount = std::make_shared<int>(0);
        const auto metadataRequestCount = std::make_shared<int>(0);
        const auto frameRequestCount = std::make_shared<int>(0);
        const auto lastRequestedFrame = std::make_shared<int>(-1);
        const auto cancelRequestCount = std::make_shared<int>(0);
        const auto closeCount = std::make_shared<int>(0);
        auto sessionFactory
            = std::make_shared<CountingProviderSessionFactory>(sessionCount, metadataRequestCount,
                frameRequestCount, lastRequestedFrame, closeCount, std::shared_ptr<int>(),
                std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount);
        CountingProviderAdapter adapter(sessionFactory,
            ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
        QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
        QVERIFY(result->sequence());

        ImageViewport item;
        item.setSize(QSizeF(100.0, 100.0));
        item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});
        QVERIFY(sessionFactory->lastSession());
        QCOMPARE(*frameRequestCount, 1);

        QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
        firstImage.fill(Qt::transparent);
        ImageFrame firstFrame(firstImage);
        emitTimedProviderFrameReady(sessionFactory->lastSession(), &firstFrame, 0, 0);
        acknowledgePendingPrimaryRenderCommitForTest(item);

        QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);
        QCOMPARE(*frameRequestCount, 2);
        QCOMPARE(*cancelRequestCount, 0);
        const ImageSequenceProviderRequestToken cancelToken
            = sessionFactory->lastSession()->lastFrameToken();
        QVERIFY(cancelToken.isValid());
        if (failCancelDelivery) {
            failNextProviderCommandDeliveryForTest(item, ImageViewport::PageRole::Primary);
        }
        QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);

        snapshot.requestStatus = requestStatusValue(item);
        snapshot.requestReason = requestReasonValue(item);
        snapshot.displayStatus = displayStatusValue(item);
        snapshot.playbackPhase = playbackPhaseValue(item);
        snapshot.requestedFrame = primaryRequestedFrame(item);
        snapshot.requestedPosition = primaryRequestedPosition(item);
        snapshot.displayedFrame = primaryDisplayedFrame(item);
        snapshot.displayedPosition = primaryDisplayedPosition(item);
        snapshot.requestRevision = revisionTokenProperty(item, "requestRevision");
        snapshot.displayRevision = revisionTokenProperty(item, "displayRevision");
        snapshot.displayedImageSize = displayedImageSize(item);
        snapshot.errorString = viewportErrorString(item);
        snapshot.warningString = viewportWarningString(item);
        snapshot.frameRequestCount = *frameRequestCount;
        snapshot.cancelRequestCount = *cancelRequestCount;
        snapshot.cancelTokenValue = providerRequestTokenValueForTest(cancelToken);
        snapshot.diagnostic = lastProviderTransportDiagnosticForTest(item);
    };

    Snapshot delivered;
    Snapshot failed;
    runScenario(false, delivered);
    runScenario(true, failed);

    QCOMPARE(failed.requestStatus, delivered.requestStatus);
    QCOMPARE(failed.requestReason, delivered.requestReason);
    QCOMPARE(failed.displayStatus, delivered.displayStatus);
    QCOMPARE(failed.playbackPhase, delivered.playbackPhase);
    QCOMPARE(failed.requestedFrame, delivered.requestedFrame);
    QCOMPARE(failed.requestedPosition, delivered.requestedPosition);
    QCOMPARE(failed.displayedFrame, delivered.displayedFrame);
    QCOMPARE(failed.displayedPosition, delivered.displayedPosition);
    QCOMPARE(failed.requestRevision, delivered.requestRevision);
    QCOMPARE(failed.displayRevision, delivered.displayRevision);
    QCOMPARE(failed.displayedImageSize, delivered.displayedImageSize);
    QCOMPARE(failed.errorString, delivered.errorString);
    QCOMPARE(failed.warningString, delivered.warningString);
    QCOMPARE(failed.frameRequestCount, delivered.frameRequestCount);
    QCOMPARE(delivered.cancelRequestCount, 1);
    QCOMPARE(failed.cancelRequestCount, 0);
    QVERIFY(!delivered.diagnostic.valid);
    QVERIFY(failed.diagnostic.valid);
    QCOMPARE(failed.diagnostic.role, ImageViewport::PageRole::Primary);
    QCOMPARE(failed.diagnostic.operation, ProviderTransportOperationForTest::Cancel);
    QVERIFY(!failed.diagnostic.metadataTokenValid);
    QCOMPARE(failed.diagnostic.metadataTokenValue, 0U);
    QVERIFY(failed.diagnostic.frameTokenValid);
    QCOMPARE(failed.diagnostic.frameTokenValue, failed.cancelTokenValue);
    QVERIFY(!failed.diagnostic.queued);
    QVERIFY(!failed.diagnostic.pendingCleanup);
}

void ImageViewportProviderLifecycleTest::
    providerCloseDeliveryFailureRecordsDiagnosticAndPreservesClearState()
{
    struct Snapshot
    {
        int requestStatus = -1;
        int requestReason = -1;
        int displayStatus = -1;
        int playbackPhase = -1;
        int requestedFrame = -1;
        int requestedPosition = -1;
        int displayedFrame = -1;
        int displayedPosition = -1;
        ImageViewportRevisionToken requestRevision;
        ImageViewportRevisionToken displayRevision;
        QSizeF displayedImageSize;
        QString errorString;
        QString warningString;
        int cancelRequestCount = 0;
        int closeCount = 0;
        quint64 metadataTokenValue = 0;
        ProviderTransportDiagnosticForTest diagnostic;
    };

    const auto runScenario = [](bool failCloseDelivery, Snapshot& snapshot) {
        ImageSequenceFactory factory;
        const auto sessionCount = std::make_shared<int>(0);
        const auto metadataRequestCount = std::make_shared<int>(0);
        const auto frameRequestCount = std::make_shared<int>(0);
        const auto lastRequestedFrame = std::make_shared<int>(-1);
        const auto cancelRequestCount = std::make_shared<int>(0);
        const auto closeCount = std::make_shared<int>(0);
        auto sessionFactory
            = std::make_shared<CountingProviderSessionFactory>(sessionCount, metadataRequestCount,
                frameRequestCount, lastRequestedFrame, closeCount, std::shared_ptr<int>(),
                std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount);
        CountingProviderAdapter adapter(sessionFactory);
        QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
        QVERIFY(result->sequence());

        ImageViewport item;
        useSynchronousProviderExecutorForTest(item);
        item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});
        QVERIFY(sessionFactory->lastSession());
        const ImageSequenceProviderRequestToken metadataToken
            = sessionFactory->lastSession()->lastMetadataToken();
        QVERIFY(metadataToken.isValid());
        if (failCloseDelivery) {
            failNextProviderCommandDeliveryForTest(item, ImageViewport::PageRole::Primary);
        }

        QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);

        snapshot.requestStatus = requestStatusValue(item);
        snapshot.requestReason = requestReasonValue(item);
        snapshot.displayStatus = displayStatusValue(item);
        snapshot.playbackPhase = playbackPhaseValue(item);
        snapshot.requestedFrame = primaryRequestedFrame(item);
        snapshot.requestedPosition = primaryRequestedPosition(item);
        snapshot.displayedFrame = primaryDisplayedFrame(item);
        snapshot.displayedPosition = primaryDisplayedPosition(item);
        snapshot.requestRevision = revisionTokenProperty(item, "requestRevision");
        snapshot.displayRevision = revisionTokenProperty(item, "displayRevision");
        snapshot.displayedImageSize = displayedImageSize(item);
        snapshot.errorString = viewportErrorString(item);
        snapshot.warningString = viewportWarningString(item);
        snapshot.cancelRequestCount = *cancelRequestCount;
        snapshot.closeCount = *closeCount;
        snapshot.metadataTokenValue = providerRequestTokenValueForTest(metadataToken);
        snapshot.diagnostic = lastProviderTransportDiagnosticForTest(item);
    };

    Snapshot delivered;
    Snapshot failed;
    runScenario(false, delivered);
    runScenario(true, failed);

    QCOMPARE(failed.requestStatus, delivered.requestStatus);
    QCOMPARE(failed.requestReason, delivered.requestReason);
    QCOMPARE(failed.displayStatus, delivered.displayStatus);
    QCOMPARE(failed.playbackPhase, delivered.playbackPhase);
    QCOMPARE(failed.requestedFrame, delivered.requestedFrame);
    QCOMPARE(failed.requestedPosition, delivered.requestedPosition);
    QCOMPARE(failed.displayedFrame, delivered.displayedFrame);
    QCOMPARE(failed.displayedPosition, delivered.displayedPosition);
    QCOMPARE(failed.requestRevision, delivered.requestRevision);
    QCOMPARE(failed.displayRevision, delivered.displayRevision);
    QCOMPARE(failed.displayedImageSize, delivered.displayedImageSize);
    QCOMPARE(failed.errorString, delivered.errorString);
    QCOMPARE(failed.warningString, delivered.warningString);
    QCOMPARE(delivered.cancelRequestCount, 1);
    QCOMPARE(failed.cancelRequestCount, 0);
    QCOMPARE(delivered.closeCount, 1);
    QCOMPARE(failed.closeCount, 0);
    QVERIFY(!delivered.diagnostic.valid);
    QVERIFY(failed.diagnostic.valid);
    QCOMPARE(failed.diagnostic.role, ImageViewport::PageRole::Primary);
    QCOMPARE(failed.diagnostic.operation, ProviderTransportOperationForTest::Close);
    QVERIFY(failed.diagnostic.metadataTokenValid);
    QCOMPARE(failed.diagnostic.metadataTokenValue, failed.metadataTokenValue);
    QVERIFY(!failed.diagnostic.frameTokenValid);
    QCOMPARE(failed.diagnostic.frameTokenValue, 0U);
    QVERIFY(!failed.diagnostic.queued);
    QVERIFY(failed.diagnostic.pendingCleanup);
}

void ImageViewportProviderLifecycleTest::
    providerCloseDeliveryFailureRetriesCleanupOnDestructionAndIgnoresLateCallbacks()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    {
        ImageViewport item;
        useSynchronousProviderExecutorForTest(item);
        useSynchronousProviderEventDeliveryForTest(item);
        item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});
        const QMetaObject* metaObject = item.metaObject();

        CountingProviderSession* session = sessionFactory->lastSession();
        QVERIFY(session);
        const ImageSequenceProviderRequestToken metadataToken = session->lastMetadataToken();
        QVERIFY(metadataToken.isValid());
        failNextProviderCommandDeliveryForTest(item, ImageViewport::PageRole::Primary);

        QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);
        const ImageViewportRevisionToken requestRevision
            = revisionTokenProperty(item, "requestRevision");
        const ImageViewportRevisionToken displayRevision
            = revisionTokenProperty(item, "displayRevision");
        const ImageViewportRevisionToken commandRevision
            = revisionTokenProperty(item, "commandRevision");
        QCOMPARE(*cancelRequestCount, 0);
        QCOMPARE(*closeCount, 0);
        QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
        QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
        QVERIFY(lastProviderTransportDiagnosticForTest(item).valid);
        QVERIFY(lastProviderTransportDiagnosticForTest(item).pendingCleanup);

        emitProviderWaiting(session, metadataToken);
        emitProviderMetadataReady(
            session, metadataToken, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
        emitProviderUnsupported(session, metadataToken,
            ImageSequenceProviderSession::UnsupportedCause::PayloadRejection,
            QStringLiteral("late unsupported after failed close"));
        emitProviderFailed(
            session, metadataToken, QStringLiteral("late failure after failed close"));

        QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
        QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
        QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
        QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
        QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
        QCOMPARE(viewportErrorString(item), QString());
    }

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(sessionFactory->lastSession(), nullptr);
}

void ImageViewportProviderLifecycleTest::providerNullSequenceCancelsActiveFrameRequestBeforeClose()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto sessionFactory
        = std::make_shared<CountingProviderSessionFactory>(sessionCount, metadataRequestCount,
            frameRequestCount, lastRequestedFrame, closeCount, std::shared_ptr<int>(),
            std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount, lastCancelledToken);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 1);

    item.setPresentationTarget(
        ImageViewportPresentationTarget::clear(), PresentationTargetTransitionPolicy {});

    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledToken, frameToken);
    QCOMPARE(*closeCount, 1);

    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(viewportWarningString(item), QString());
}

void ImageViewportProviderLifecycleTest::
    providerReplacementIgnoresCancelledMetadataAcknowledgement()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CancellingAcknowledgementProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, cancelRequestCount, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QImage replacementImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::transparent);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));

    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const ImageViewportRevisionToken replacementRequestRevision
        = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(viewportErrorString(item), QString());

    drainQueuedProviderResults();

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), replacementRequestRevision);
    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderLifecycleTest::providerClearIgnoresCancelledMetadataAcknowledgement()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CancellingAcknowledgementProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, cancelRequestCount, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));

    QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageViewportRevisionToken clearedRequestRevision
        = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(viewportErrorString(item), QString());

    drainQueuedProviderResults();

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), clearedRequestRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderLifecycleTest::providerClosedGenerationTokenCollisionIsIgnoredAfterClear()
{
    ImageSequenceFactory factory;
    const auto staleSessionCount = std::make_shared<int>(0);
    const auto staleMetadataRequestCount = std::make_shared<int>(0);
    const auto staleFrameRequestCount = std::make_shared<int>(0);
    const auto staleCancelRequestCount = std::make_shared<int>(0);
    const auto staleCloseCount = std::make_shared<int>(0);
    auto staleSessionFactory = std::make_shared<CancellingAcknowledgementProviderSessionFactory>(
        staleSessionCount, staleMetadataRequestCount, staleFrameRequestCount,
        staleCancelRequestCount, staleCloseCount);
    CountingProviderAdapter staleAdapter(staleSessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> staleResult(factory.fromProvider(&staleAdapter));
    QVERIFY(staleResult->sequence());

    const auto replacementSessionCount = std::make_shared<int>(0);
    const auto replacementMetadataRequestCount = std::make_shared<int>(0);
    const auto replacementFrameRequestCount = std::make_shared<int>(0);
    const auto replacementLastRequestedFrame = std::make_shared<int>(-1);
    const auto replacementCloseCount = std::make_shared<int>(0);
    auto replacementSessionFactory = std::make_shared<CountingProviderSessionFactory>(
        replacementSessionCount, replacementMetadataRequestCount, replacementFrameRequestCount,
        replacementLastRequestedFrame, replacementCloseCount);
    CountingProviderAdapter replacementAdapter(replacementSessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromProvider(&replacementAdapter));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setPresentationTarget(ImageViewportPresentationTarget(staleResult->sequence()),
        PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(staleSessionFactory->lastSession());
    const ImageSequenceProviderRequestToken staleToken
        = staleSessionFactory->lastSession()->lastMetadataToken();
    QVERIFY(staleToken.isValid());
    QCOMPARE(*staleMetadataRequestCount, 1);

    QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);

    setNextProviderRequestTokenForTest(item, providerRequestTokenValueForTest(staleToken) - 1);
    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
        PresentationTargetTransitionPolicy {});

    QVERIFY(replacementSessionFactory->lastSession());
    QCOMPARE(replacementSessionFactory->lastSession()->lastMetadataToken(), staleToken);
    const ImageViewportRevisionToken replacementRequestRevision
        = revisionTokenProperty(item, "requestRevision");

    drainQueuedProviderResults();

    QCOMPARE(*staleCancelRequestCount, 1);
    QCOMPARE(*staleCloseCount, 1);
    QCOMPARE(*replacementSessionCount, 1);
    QCOMPARE(*replacementMetadataRequestCount, 1);
    QCOMPARE(*replacementFrameRequestCount, 0);
    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), replacementRequestRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderLifecycleTest::providerClearIgnoresCancelledFrameAcknowledgement()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CancellingAcknowledgementProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, cancelRequestCount, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);

    QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageViewportRevisionToken clearedRequestRevision
        = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(viewportErrorString(item), QString());

    drainQueuedProviderResults();

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), clearedRequestRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderLifecycleTest::providerResultsAreQueuedFromSessionEntryPoint()
{
    ImageSequenceFactory factory;
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<SynchronousMetadataProviderSessionFactory>(
        metadataRequestCount, frameRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);

    QCoreApplication::processEvents();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
}

void ImageViewportProviderLifecycleTest::
    synchronousProviderEventDeliveryBypassesEventLoopForProtocolTests()
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

    ImageViewport item;
    useSynchronousProviderEventDeliveryForTest(item);
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(primaryRequestedFrame(item), -1);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
}

void ImageViewportProviderLifecycleTest::
    providerQueuedMetadataFromClosedGenerationIsIgnoredAfterReplacement()
{
    ImageSequenceFactory factory;
    const auto staleMetadataRequestCount = std::make_shared<int>(0);
    const auto staleFrameRequestCount = std::make_shared<int>(0);
    auto staleSessionFactory = std::make_shared<SynchronousMetadataProviderSessionFactory>(
        staleMetadataRequestCount, staleFrameRequestCount);
    CountingProviderAdapter staleAdapter(staleSessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> staleResult(factory.fromProvider(&staleAdapter));
    QVERIFY(staleResult->sequence());

    const auto replacementSessionCount = std::make_shared<int>(0);
    const auto replacementMetadataRequestCount = std::make_shared<int>(0);
    const auto replacementFrameRequestCount = std::make_shared<int>(0);
    const auto replacementLastRequestedFrame = std::make_shared<int>(-1);
    const auto replacementCloseCount = std::make_shared<int>(0);
    auto replacementSessionFactory = std::make_shared<CountingProviderSessionFactory>(
        replacementSessionCount, replacementMetadataRequestCount, replacementFrameRequestCount,
        replacementLastRequestedFrame, replacementCloseCount);
    CountingProviderAdapter replacementAdapter(replacementSessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromProvider(&replacementAdapter));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setPresentationTarget(ImageViewportPresentationTarget(staleResult->sequence()),
        PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*staleMetadataRequestCount, 1);
    QCOMPARE(*staleFrameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(primaryRequestedFrame(item), -1);

    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
        PresentationTargetTransitionPolicy {});
    const ImageViewportRevisionToken replacementRequestRevision
        = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(*replacementSessionCount, 1);
    QCOMPARE(*replacementMetadataRequestCount, 1);
    QCOMPARE(*replacementFrameRequestCount, 0);
    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryFrameCount(item), -1);

    drainQueuedProviderResults();

    QCOMPARE(*staleFrameRequestCount, 0);
    QCOMPARE(*replacementFrameRequestCount, 0);
    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), replacementRequestRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryFrameCount(item), -1);

    QVERIFY(replacementSessionFactory->lastSession());
    emitProviderMetadataReady(replacementSessionFactory->lastSession(),
        replacementSessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(20.0, 10.0)));
    drainQueuedProviderResults();

    QCOMPARE(*replacementFrameRequestCount, 1);
    QCOMPARE(*replacementLastRequestedFrame, 0);
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));
}

void ImageViewportProviderLifecycleTest::providerFrameResultsAreQueuedFromSessionEntryPoint()
{
    ImageSequenceFactory factory;
    const auto frameRequestCount = std::make_shared<int>(0);
    auto sessionFactory
        = std::make_shared<SynchronousFrameProviderSessionFactory>(frameRequestCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));

    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));
}

void ImageViewportProviderLifecycleTest::providerTerminalResultsAreQueuedFromSessionEntryPoint()
{
    ImageSequenceFactory factory;
    const auto metadataRequestCount = std::make_shared<int>(0);
    auto sessionFactory
        = std::make_shared<SynchronousFailureProviderSessionFactory>(metadataRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(viewportErrorString(item), QString());

    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    verifyRequestStatusReasonPair(item);
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("metadata failed synchronously")));
}

void ImageViewportProviderLifecycleTest::providerUnsupportedResultsAreQueuedFromSessionEntryPoint()
{
    ImageSequenceFactory factory;
    const auto metadataRequestCount = std::make_shared<int>(0);
    auto sessionFactory
        = std::make_shared<SynchronousUnsupportedProviderSessionFactory>(metadataRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(viewportErrorString(item), QString());

    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(
        requestReasonValue(item), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    verifyRequestStatusReasonPair(item);
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QVERIFY(
        viewportErrorString(item).contains(QStringLiteral("metadata unsupported synchronously")));
}

QTEST_MAIN(ImageViewportProviderLifecycleTest)

#include "tst_imageviewport_provider_lifecycle.moc"
