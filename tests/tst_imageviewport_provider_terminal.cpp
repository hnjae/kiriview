#include "imageviewport_provider_test_support.h"

class ImageViewportProviderTerminalTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderTerminalTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void providerFrameUnsupportedOperationReportsUnsupportedRequest();
    void providerPlaybackUnsupportedPayloadReportsPayloadRejection();
    void providerMetadataFailureReportsProviderFailure();
    void providerMetadataUnsupportedReportsUnsupportedRequest();
    void providerMetadataCancellationReportsProviderFailure();
    void providerMetadataEndOfSequenceReportsProtocolViolation();
    void providerFrameEndOfSequenceReportsProtocolViolation();
    void providerTotalDurationSeekEndOfSequenceReportsProtocolViolation();
};

void ImageViewportProviderTerminalTest::providerFrameUnsupportedOperationReportsUnsupportedRequest()
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
    item.setPageSet(ImageViewportPageSet(result->sequence()), PageSetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();

    emitProviderUnsupported(sessionFactory->lastSession(), frameToken,
        ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest,
        QStringLiteral("frame operation unsupported"));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QVERIFY(viewportErrorString(item)
            .contains(QStringLiteral("frame operation unsupported")));
}

void ImageViewportProviderTerminalTest::providerPlaybackUnsupportedPayloadReportsPayloadRejection()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        playbackRequestCount, lastPlaybackFrame, lastPlaybackPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPageSet(ImageViewportPageSet(result->sequence()), PageSetTransitionPolicy {});
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
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 100);
    QCOMPARE(*playbackRequestCount, 1);
    const ImageSequenceProviderRequestToken playbackToken
        = sessionFactory->lastSession()->lastFrameToken();

    emitProviderUnsupported(sessionFactory->lastSession(), playbackToken,
        ImageSequenceProviderSession::UnsupportedCause::PayloadRejection,
        QStringLiteral("playback payload unsupported"));
    drainQueuedProviderResults();

    QCOMPARE(
        playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QVERIFY(viewportErrorString(item)
            .contains(QStringLiteral("playback payload unsupported")));
}

void ImageViewportProviderTerminalTest::providerMetadataFailureReportsProviderFailure()
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
    item.setPageSet(ImageViewportPageSet(result->sequence()), PageSetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderFailed(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata service unavailable"));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QVERIFY(viewportErrorString(item)
            .contains(QStringLiteral("metadata service unavailable")));
}

void ImageViewportProviderTerminalTest::providerMetadataUnsupportedReportsUnsupportedRequest()
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
    item.setPageSet(ImageViewportPageSet(result->sequence()), PageSetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderUnsupported(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderUnsupportedCause::UnsupportedRequest,
        QStringLiteral("unsupported codec"));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("unsupported codec")));

    const ImageViewportRevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(requestStatusValue(item),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(requestStatusValue(item),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(requestStatusValue(item),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportProviderTerminalTest::providerMetadataCancellationReportsProviderFailure()
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
    item.setPageSet(ImageViewportPageSet(result->sequence()), PageSetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderCancelled(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QVERIFY(viewportErrorString(item)
            .contains(QStringLiteral("metadata cancelled by provider")));
}

void ImageViewportProviderTerminalTest::providerMetadataEndOfSequenceReportsProtocolViolation()
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
    item.setPageSet(ImageViewportPageSet(result->sequence()), PageSetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderEndOfSequence(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastMetadataToken());
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QVERIFY(viewportErrorString(item)
            .contains(QStringLiteral("provider protocol violation")));

    const ImageViewportRevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
}

void ImageViewportProviderTerminalTest::providerFrameEndOfSequenceReportsProtocolViolation()
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
    item.setPageSet(ImageViewportPageSet(result->sequence()), PageSetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);

    emitProviderEndOfSequence(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken());
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 1);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryFrameCount(item), 1);
    QVERIFY(viewportErrorString(item)
            .contains(QStringLiteral("provider protocol violation")));

    const ImageViewportRevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
}

void ImageViewportProviderTerminalTest::
    providerTotalDurationSeekEndOfSequenceReportsProtocolViolation()
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
    item.setPageSet(ImageViewportPageSet(result->sequence()), PageSetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(item.seekToPosition(350), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "RequestQueued"));
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken totalDurationToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 350);

    emitProviderEndOfSequence(sessionFactory->lastSession(), totalDurationToken);
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 1);
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 350);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QVERIFY(viewportErrorString(item)
            .contains(QStringLiteral("provider protocol violation")));

    const ImageViewportRevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seekToPosition(350), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
}
QTEST_MAIN(ImageViewportProviderTerminalTest)

#include "tst_imageviewport_provider_terminal.moc"
