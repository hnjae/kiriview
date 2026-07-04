#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

class ImageViewportProviderRequestsTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderRequestsTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void providerRequestTokensAreUniqueWithinSession();
    void providerFrameSeekUsesFrameRequest();
    void providerFrameSeekBeforeMetadataResolvesAfterMetadata();
    void providerSupersededPreMetadataSeekIgnoresStaleRejection();
    void providerStillMetadataRevisesAcceptedSeekObservations();
    void providerInvalidPreMetadataSeekCanStartPlaybackAfterMetadata();
    void providerPositionSeekBeforeMetadataResolvesAfterMetadata();
    void providerPositionSeekUsesPositionRequest();
    void providerTotalDurationPositionSeekUsesPositionRequest();
    void providerPreMetadataPositionSeekResolvesToPositionRequest();
    void providerTotalDurationSeekBeforeMetadataResolvesFinalFrame();
    void providerPositionSeekBeforeStillMetadataKeepsGenerationSeekable();
    void providerPlaybackBeforeStillMetadataKeepsGenerationSeekable();
    void providerFrameSeekQueuesBehindActiveFrameRequest();
    void waitProjectionRevisionChangesOnlyWhenPublicReasonChanges();
    void providerTimedSameFrameSeekSupersedesActiveRequest();
    void providerTimedFrameSeekRequestsSelectedFrame();
    void providerTimedFrameSeekWithoutDiagnosticsDoesNotNotify();
    void providerTimedFrameCommitWithUnchangedGeometryDoesNotNotifyGeometryState();
    void providerTimedFrameSeekCancelsSupersededRequest();
    void providerTimedPositionSeekRequestsResolvedFrame();
    void secondaryProviderFrameSeekBeforeMetadataResolvesAfterMetadata();
    void secondaryProviderPositionSeekBeforeMetadataResolvesAfterMetadata();
    void secondaryProviderFrameSeekUsesFrameRequest();
    void secondaryProviderPositionSeekRequestsResolvedFrame();
    void secondaryProviderInvalidAndUnsupportedSeekCommandsPreserveRequest();
    void secondaryProviderFrameSeekIgnoresSupersededFrameResult();
};

void ImageViewportProviderRequestsTest::providerRequestTokensAreUniqueWithinSession()
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

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken
        = sessionFactory->lastSession()->lastMetadataToken();
    QVERIFY(metadataToken.isValid());
    QCOMPARE(*metadataRequestCount, 1);

    emit sessionFactory->lastSession()->metadataReady(
        metadataToken, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    const ImageSequenceProviderRequestToken initialFrameToken
        = sessionFactory->lastSession()->lastFrameToken();
    QVERIFY(initialFrameToken.isValid());
    QVERIFY(initialFrameToken != metadataToken);
    QCOMPARE(*frameRequestCount, 1);

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken seekFrameToken
        = sessionFactory->lastSession()->lastFrameToken();

    QVERIFY(seekFrameToken.isValid());
    QVERIFY(seekFrameToken != metadataToken);
    QVERIFY(seekFrameToken != initialFrameToken);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*cancelRequestCount, 1);
}

void ImageViewportProviderRequestsTest::providerFrameSeekUsesFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<quint64>(), positionRequestCount, lastPositionFrame,
        lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*positionRequestCount, 0);
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(*positionRequestCount, 0);
    QCOMPARE(*lastPositionFrame, -1);
    QCOMPARE(*lastRequestedPosition, -1);
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
}

void ImageViewportProviderRequestsTest::providerFrameSeekBeforeMetadataResolvesAfterMetadata()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    const quint64 preMetadataRequestId = activeRequestIdForTest(item);
    QVERIFY(preMetadataRequestId > 0);
    const RevisionToken preMetadataRequestRevision = revisionTokenProperty(item, "requestRevision");
    QSignalSpy requestRevisionSpy(&item, &ImageViewport::requestRevisionChanged);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QVERIFY(activeRequestIdForTest(item) > preMetadataRequestId);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    verifyRevisionChanged(item, "requestRevision", preMetadataRequestRevision);
    QCOMPARE(requestRevisionSpy.count(), 1);
}

void ImageViewportProviderRequestsTest::providerSupersededPreMetadataSeekIgnoresStaleRejection()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(3), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderRequestsTest::providerStillMetadataRevisesAcceptedSeekObservations()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("frameSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("timedPlaybackSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    const RevisionToken preMetadataRequestRevision = revisionTokenProperty(item, "requestRevision");
    QSignalSpy requestRevisionSpy(&item, &ImageViewport::requestRevisionChanged);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), 0);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").minimum(), -1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), -1);
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    verifyRevisionChanged(item, "requestRevision", preMetadataRequestRevision);
    QCOMPARE(requestRevisionSpy.count(), 1);
}

void ImageViewportProviderRequestsTest::
    providerInvalidPreMetadataSeekCanStartPlaybackAfterMetadata()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(3), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestedFrame").toInt(), 3);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "InvalidRequest"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(*frameRequestCount, 0);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 0);
    QCOMPARE(*lastPlaybackPosition, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
}

void ImageViewportProviderRequestsTest::providerPositionSeekBeforeMetadataResolvesAfterMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<quint64>(), positionRequestCount, lastPositionFrame,
        lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seekToPosition(349), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), 349);
    const RevisionToken preMetadataRequestRevision = revisionTokenProperty(item, "requestRevision");
    QSignalSpy requestRevisionSpy(&item, &ImageViewport::requestRevisionChanged);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 349);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 349);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    verifyRevisionChanged(item, "requestRevision", preMetadataRequestRevision);
    QCOMPARE(requestRevisionSpy.count(), 1);
}

void ImageViewportProviderRequestsTest::providerPositionSeekUsesPositionRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<quint64>(), positionRequestCount, lastPositionFrame,
        lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*positionRequestCount, 0);
    QCOMPARE(item.seekToPosition(125), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 125);
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 125);
}

void ImageViewportProviderRequestsTest::providerTotalDurationPositionSeekUsesPositionRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<quint64>(), positionRequestCount, lastPositionFrame,
        lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());

    QCOMPARE(item.seekToPosition(350), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 350);
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);
}

void ImageViewportProviderRequestsTest::providerPreMetadataPositionSeekResolvesToPositionRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<quint64>(), positionRequestCount, lastPositionFrame,
        lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());

    QCOMPARE(item.seekToPosition(125), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*positionRequestCount, 0);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 125);
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 125);
}

void ImageViewportProviderRequestsTest::providerTotalDurationSeekBeforeMetadataResolvesFinalFrame()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<quint64>(), positionRequestCount, lastPositionFrame,
        lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seekToPosition(350), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 350);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastPositionToken(), &frame, 1, 100);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderRequestsTest::
    providerPositionSeekBeforeStillMetadataKeepsGenerationSeekable()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seekToPosition(10), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), 10);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), 10);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
}

void ImageViewportProviderRequestsTest::providerPlaybackBeforeStillMetadataKeepsGenerationSeekable()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(*frameRequestCount, 0);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->imageFrameReady(
        sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
}

void ImageViewportProviderRequestsTest::providerFrameSeekQueuesBehindActiveFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledTokenId = std::make_shared<quint64>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount,
        lastCancelledTokenId);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    CountingProviderSession* session = sessionFactory->lastSession();
    const ImageSequenceProviderRequestToken initialFrameToken = session->lastFrameToken();
    QVERIFY(initialFrameToken.isValid());
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);

    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy requestRevisionSpy(&item, &ImageViewport::requestRevisionChanged);
    const RevisionToken providerWaitingRevision = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledTokenId, initialFrameToken.id());
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RequestQueued"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    verifyRevisionChanged(item, "requestRevision", providerWaitingRevision);
    QCOMPARE(requestStateSpy.count(), 1);
    QCOMPARE(requestRevisionSpy.count(), 1);

    const RevisionToken queuedRevision = revisionTokenProperty(item, "requestRevision");
    drainQueuedProviderResults();

    const ImageSequenceProviderRequestToken seekFrameToken = session->lastFrameToken();
    QVERIFY(seekFrameToken.isValid());
    QVERIFY(seekFrameToken != initialFrameToken);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    verifyRevisionChanged(item, "requestRevision", queuedRevision);
    QCOMPARE(requestStateSpy.count(), 2);
    QCOMPARE(requestRevisionSpy.count(), 2);

    QImage staleImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    staleImage.fill(Qt::transparent);
    ImageFrame staleFrame(staleImage);
    const RevisionToken activeRevision = revisionTokenProperty(item, "requestRevision");
    emitTimedProviderFrameReady(session, initialFrameToken, &staleFrame, 0, 0);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), activeRevision);
    QCOMPARE(requestStateSpy.count(), 2);
    QCOMPARE(requestRevisionSpy.count(), 2);
}

void ImageViewportProviderRequestsTest::waitProjectionRevisionChangesOnlyWhenPublicReasonChanges()
{
    ImageSequenceFactory factory;
    const auto primarySessionCount = std::make_shared<int>(0);
    const auto primaryMetadataRequestCount = std::make_shared<int>(0);
    const auto primaryFrameRequestCount = std::make_shared<int>(0);
    const auto primaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto primaryCloseCount = std::make_shared<int>(0);
    auto primarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        primarySessionCount, primaryMetadataRequestCount, primaryFrameRequestCount,
        primaryLastRequestedFrame, primaryCloseCount);
    CountingProviderAdapter primaryAdapter(
        primarySessionFactory, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
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
    QCOMPARE(item.setPageSet(primaryResult->sequence(), secondaryResult->sequence()),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QVERIFY(primarySessionFactory->lastSession());
    QVERIFY(secondarySessionFactory->lastSession());
    const ImageSequenceProviderRequestToken primaryFrameToken
        = primarySessionFactory->lastSession()->lastFrameToken();
    const ImageSequenceProviderRequestToken secondaryMetadataToken
        = secondarySessionFactory->lastSession()->lastMetadataToken();
    QVERIFY(primaryFrameToken.isValid());
    QVERIFY(secondaryMetadataToken.isValid());

    QSignalSpy requestRevisionSpy(&item, &ImageViewport::requestRevisionChanged);
    const RevisionToken providerWaitingRevision = revisionTokenProperty(item, "requestRevision");

    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    emit primarySessionFactory->lastSession()->imageFrameReady(primaryFrameToken, &primaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), providerWaitingRevision);
    QCOMPARE(requestRevisionSpy.count(), 0);

    emit secondarySessionFactory->lastSession()->metadataReady(
        secondaryMetadataToken, ImageSequenceProviderMetadata::still(QSizeF(8.0, 16.0)));
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken secondaryFrameToken
        = secondarySessionFactory->lastSession()->lastFrameToken();
    QVERIFY(secondaryFrameToken.isValid());

    const RevisionToken secondaryProviderWaitingRevision
        = revisionTokenProperty(item, "requestRevision");
    QImage secondaryImage(8, 16, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame secondaryFrame(secondaryImage);
    emit secondarySessionFactory->lastSession()->imageFrameReady(
        secondaryFrameToken, &secondaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    verifyRevisionChanged(item, "requestRevision", secondaryProviderWaitingRevision);
}

void ImageViewportProviderRequestsTest::providerTimedSameFrameSeekSupersedesActiveRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledTokenId = std::make_shared<quint64>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount,
        lastCancelledTokenId);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    CountingProviderSession* session = sessionFactory->lastSession();
    const ImageSequenceProviderRequestToken initialFrameToken = session->lastFrameToken();
    QVERIFY(initialFrameToken.isValid());
    const quint64 initialRequestId = activeRequestIdForTest(item);
    QVERIFY(initialRequestId > 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);

    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy requestRevisionSpy(&item, &ImageViewport::requestRevisionChanged);
    const RevisionToken providerWaitingRevision = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledTokenId, initialFrameToken.id());
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RequestQueued"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    verifyRevisionChanged(item, "requestRevision", providerWaitingRevision);
    QCOMPARE(requestStateSpy.count(), 1);
    QCOMPARE(requestRevisionSpy.count(), 1);

    const RevisionToken queuedRevision = revisionTokenProperty(item, "requestRevision");
    drainQueuedProviderResults();

    const ImageSequenceProviderRequestToken activeFrameToken = session->lastFrameToken();
    QVERIFY(activeFrameToken.isValid());
    QVERIFY(activeFrameToken != initialFrameToken);
    const quint64 activeRequestId = activeRequestIdForTest(item);
    QVERIFY(activeRequestId > initialRequestId);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    verifyRevisionChanged(item, "requestRevision", queuedRevision);
    QCOMPARE(requestStateSpy.count(), 2);
    QCOMPARE(requestRevisionSpy.count(), 2);

    QImage staleImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    staleImage.fill(Qt::transparent);
    ImageFrame staleFrame(staleImage);
    const RevisionToken activeRevision = revisionTokenProperty(item, "requestRevision");
    emitTimedProviderFrameReady(session, initialFrameToken, &staleFrame, 0, 0);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(activeRequestIdForTest(item), activeRequestId);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), activeRevision);
    QCOMPARE(requestStateSpy.count(), 2);
    QCOMPARE(requestRevisionSpy.count(), 2);

    QImage activeImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    activeImage.fill(Qt::black);
    ImageFrame activeFrame(activeImage);
    emitTimedProviderFrameReady(session, activeFrameToken, &activeFrame, 0, 0);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderRequestsTest::providerTimedFrameSeekRequestsSelectedFrame()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    const RevisionToken readyDisplayRevision = revisionTokenProperty(item, "displayRevision");

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
}

void ImageViewportProviderRequestsTest::providerTimedFrameSeekWithoutDiagnosticsDoesNotNotify()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());

    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(diagnosticsSpy.count(), 0);
}

void ImageViewportProviderRequestsTest::
    providerTimedFrameCommitWithUnchangedGeometryDoesNotNotifyGeometryState()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &firstFrame, 0, 0);
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));

    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(geometrySpy.count(), 0);

    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame secondFrame(secondImage);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &secondFrame, 1, 100);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(geometrySpy.count(), 0);
}

void ImageViewportProviderRequestsTest::providerTimedFrameSeekCancelsSupersededRequest()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken firstSeekToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*cancelRequestCount, 1);
    QVERIFY(sessionFactory->lastSession()->lastCancelledToken() == firstSeekToken);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), firstSeekToken, &frame, 1, 100);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    emit sessionFactory->lastSession()->providerUnsupported(
        firstSeekToken, QStringLiteral("superseded request unsupported late"));
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());

    emit sessionFactory->lastSession()->providerCancelled(
        firstSeekToken, QStringLiteral("superseded request cleanup complete"));
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());

    emit sessionFactory->lastSession()->providerFailed(
        firstSeekToken, QStringLiteral("superseded request failed late"));
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderRequestsTest::providerTimedPositionSeekRequestsResolvedFrame()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<quint64>(), positionRequestCount, lastPositionFrame,
        lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    QCOMPARE(item.seekToPosition(349), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 349);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 349);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));

    emitTimedProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastPositionToken(), &frame, 1, 100);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(item.seekToPosition(350), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*positionRequestCount, 2);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 350);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);

    emitTimedProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastPositionToken(), &frame, 1, 100);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);
}

void ImageViewportProviderRequestsTest::
    secondaryProviderFrameSeekBeforeMetadataResolvesAfterMetadata()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(
        item.seek(ImageViewport::PageRole::Secondary, 1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), 1);
    QCOMPARE(item.property("secondaryRequestedPosition").toInt(), -1);

    QVERIFY(secondarySessionFactory->lastSession());
    emit secondarySessionFactory->lastSession()->metadataReady(
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 1);
    QCOMPARE(*secondaryLastRequestedFrame, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), 1);
    QCOMPARE(item.property("secondaryRequestedPosition").toInt(), 100);
    QCOMPARE(item.property("secondaryFrameCount").toInt(), 2);
    QCOMPARE(item.property("secondaryTotalDuration").toInt(), 350);
}

void ImageViewportProviderRequestsTest::
    secondaryProviderPositionSeekBeforeMetadataResolvesAfterMetadata()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    const auto secondaryPositionRequestCount = std::make_shared<int>(0);
    const auto secondaryLastPositionFrame = std::make_shared<int>(-1);
    const auto secondaryLastRequestedPosition = std::make_shared<int>(-1);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount, std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<quint64>(), secondaryPositionRequestCount, secondaryLastPositionFrame,
        secondaryLastRequestedPosition);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Secondary, 349),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(*secondaryPositionRequestCount, 0);
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), -1);
    QCOMPARE(item.property("secondaryRequestedPosition").toInt(), 349);

    QVERIFY(secondarySessionFactory->lastSession());
    emit secondarySessionFactory->lastSession()->metadataReady(
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(*secondaryPositionRequestCount, 1);
    QCOMPARE(*secondaryLastPositionFrame, 1);
    QCOMPARE(*secondaryLastRequestedPosition, 349);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), 1);
    QCOMPARE(item.property("secondaryRequestedPosition").toInt(), 349);
    QCOMPARE(item.property("secondaryFrameCount").toInt(), 2);
    QCOMPARE(item.property("secondaryTotalDuration").toInt(), 350);
}

void ImageViewportProviderRequestsTest::secondaryProviderFrameSeekUsesFrameRequest()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    const auto secondaryPositionRequestCount = std::make_shared<int>(0);
    const auto secondaryLastPositionFrame = std::make_shared<int>(-1);
    const auto secondaryLastRequestedPosition = std::make_shared<int>(-1);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount, std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<quint64>(), secondaryPositionRequestCount, secondaryLastPositionFrame,
        secondaryLastRequestedPosition);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);

    QVERIFY(secondarySessionFactory->lastSession());
    emit secondarySessionFactory->lastSession()->metadataReady(
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    QCOMPARE(*secondaryFrameRequestCount, 1);
    QCOMPARE(*secondaryLastRequestedFrame, 0);

    QCOMPARE(
        item.seek(ImageViewport::PageRole::Secondary, 1), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 2);
    QCOMPARE(*secondaryLastRequestedFrame, 1);
    QCOMPARE(*secondaryPositionRequestCount, 0);
    QCOMPARE(*secondaryLastPositionFrame, -1);
    QCOMPARE(*secondaryLastRequestedPosition, -1);
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), 1);
    QCOMPARE(item.property("secondaryRequestedPosition").toInt(), 100);
}

void ImageViewportProviderRequestsTest::secondaryProviderPositionSeekRequestsResolvedFrame()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    const auto secondaryPositionRequestCount = std::make_shared<int>(0);
    const auto secondaryLastPositionFrame = std::make_shared<int>(-1);
    const auto secondaryLastRequestedPosition = std::make_shared<int>(-1);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount, std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<quint64>(), secondaryPositionRequestCount, secondaryLastPositionFrame,
        secondaryLastRequestedPosition);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);

    QVERIFY(secondarySessionFactory->lastSession());
    emit secondarySessionFactory->lastSession()->metadataReady(
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Secondary, 350),
        ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 1);
    QCOMPARE(*secondaryLastRequestedFrame, 0);
    QCOMPARE(*secondaryPositionRequestCount, 1);
    QCOMPARE(*secondaryLastPositionFrame, 1);
    QCOMPARE(*secondaryLastRequestedPosition, 350);
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), 1);
    QCOMPARE(item.property("secondaryRequestedPosition").toInt(), 350);
}

void ImageViewportProviderRequestsTest::
    secondaryProviderInvalidAndUnsupportedSeekCommandsPreserveRequest()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    const auto secondaryPositionRequestCount = std::make_shared<int>(0);
    const auto secondaryLastPositionFrame = std::make_shared<int>(-1);
    const auto secondaryLastRequestedPosition = std::make_shared<int>(-1);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount, std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<quint64>(), secondaryPositionRequestCount, secondaryLastPositionFrame,
        secondaryLastRequestedPosition);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(secondarySessionFactory->lastSession());
    emit secondarySessionFactory->lastSession()->metadataReady(
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(
        item.seek(ImageViewport::PageRole::Secondary, 1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*secondaryFrameRequestCount, 1);

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Secondary, 0),
        ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*secondaryPositionRequestCount, 0);
    QCOMPARE(*secondaryLastPositionFrame, -1);
    QCOMPARE(*secondaryLastRequestedPosition, -1);
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), 0);
    QCOMPARE(item.property("secondaryRequestedPosition").toInt(), -1);
}

void ImageViewportProviderRequestsTest::secondaryProviderFrameSeekIgnoresSupersededFrameResult()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
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
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(secondarySessionFactory->lastSession());
    emit secondarySessionFactory->lastSession()->metadataReady(
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    CountingProviderSession* session = secondarySessionFactory->lastSession();
    const ImageSequenceProviderRequestToken initialFrameToken = session->lastFrameToken();
    QVERIFY(initialFrameToken.isValid());

    QCOMPARE(
        item.seek(ImageViewport::PageRole::Secondary, 1), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();
    QCOMPARE(*secondaryFrameRequestCount, 2);
    QCOMPARE(*secondaryLastRequestedFrame, 1);

    QImage staleImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    staleImage.fill(Qt::transparent);
    ImageFrame staleFrame(staleImage);
    emitTimedProviderFrameReady(session, initialFrameToken, &staleFrame, 0, 0);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), 1);
    QCOMPARE(item.property("secondaryRequestedPosition").toInt(), 100);
    QCOMPARE(item.property("secondaryDisplayedFrame").toInt(), -1);
    QVERIFY(item.property("errorString").toString().isEmpty());
}

QTEST_MAIN(ImageViewportProviderRequestsTest)

#include "tst_imageviewport_provider_requests.moc"
