#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

namespace {

void acknowledgePendingRenderCommit(ImageViewport& item)
{
    item.acknowledgeRenderCommitForTest(item.pendingRenderGenerationForTest(),
        item.activeRequestIdForTest(), item.pendingRenderPayloadIdForTest());
}

void acknowledgePendingRenderFailure(ImageViewport& item)
{
    item.acknowledgeRenderFailureForTest(item.pendingRenderGenerationForTest(),
        item.activeRequestIdForTest(), item.pendingRenderPayloadIdForTest());
}

}

class ImageViewportRenderCommitTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportRenderCommitTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void stillImagePaintFailureReportsRenderFailure();
    void timedFrameListPaintFailureRetainsPreviousDisplay();
    void timedFrameListPlaybackPaintFailureStopsPlayback();
    void timedFrameListPlayAfterPaintFailureRestartsDisplayRequest();
    void successfulPaintClearsRenderFailureInterest();
    void builtInSameFrameSeekCreatesFreshRequestIdentity();
    void providerTimedFramePaintFailureRetainsPreviousDisplay();
    void providerTimedPlaybackPaintFailureStopsPlayback();
    void providerTimedPlayAfterPaintFailureRestartsPlaybackRequest();
    void providerSupersededRenderWaitingClearsPendingRenderCommit();
    void providerSupersededRenderFailureIsIgnored();
    void providerRenderCommitAcknowledgementPromotesPendingFrameWithoutSceneGraph();
    void secondaryProviderSpreadRenderFailureRetainsPreviousDisplay();
    void twoPageSinglePayloadCommitAcknowledgementIsIncomplete();
    void secondaryRoleRenderFailureReportsFailureWithoutSceneGraph();
    void staleSecondaryRoleRenderFailureIsIgnoredWithoutSceneGraph();
    void staleRenderCommitAcknowledgementIsIgnoredWithoutSceneGraph();
    void staleRenderFailureAcknowledgementIsIgnoredWithoutSceneGraph();
    void activeRenderFailureAcknowledgementReportsFailureWithoutSceneGraph();
    void playbackWaitingRenderCommitAcknowledgementResumesWithoutSceneGraph();
    void geometryChangeRecoversRenderWaitingWithoutSceneGraph();
};

void ImageViewportRenderCommitTest::stillImagePaintFailureReportsRenderFailure()
{
    ImageSequenceFactory factory;
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));

    acknowledgePendingRenderFailure(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("render commit failed")));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportRenderCommitTest::timedFrameListPaintFailureRetainsPreviousDisplay()
{
    ImageSequenceFactory factory;
    QImage firstImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(QColor(255, 0, 0, 255));
    QImage secondImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(QColor(0, 255, 0, 255));
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));

    item.setSize(QSizeF(0.0, 20.0));
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    item.setSize(QSizeF(40.0, 20.0));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);

    acknowledgePendingRenderFailure(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("render commit failed")));
}

void ImageViewportRenderCommitTest::timedFrameListPlaybackPaintFailureStopsPlayback()
{
    ImageSequenceFactory factory;
    QImage firstImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(QColor(255, 0, 0, 255));
    QImage secondImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(QColor(0, 255, 0, 255));
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.setSize(QSizeF(0.0, 20.0));
    item.advancePlaybackForTest(100);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    item.setSize(QSizeF(40.0, 20.0));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);

    acknowledgePendingRenderFailure(item);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("render commit failed")));
}

void ImageViewportRenderCommitTest::timedFrameListPlayAfterPaintFailureRestartsDisplayRequest()
{
    ImageSequenceFactory factory;
    QImage firstImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(QColor(255, 0, 0, 255));
    QImage secondImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(QColor(0, 255, 0, 255));
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    item.setSize(QSizeF(0.0, 20.0));
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    item.setSize(QSizeF(40.0, 20.0));
    acknowledgePendingRenderFailure(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportRenderCommitTest::successfulPaintClearsRenderFailureInterest()
{
    ImageSequenceFactory factory;
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    const quint64 generation = item.pendingRenderGenerationForTest();
    const quint64 requestId = item.activeRequestIdForTest();
    const quint64 payloadId = item.pendingRenderPayloadIdForTest();
    acknowledgePendingRenderCommit(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    item.acknowledgeRenderFailureForTest(generation, requestId, payloadId);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportRenderCommitTest::builtInSameFrameSeekCreatesFreshRequestIdentity()
{
    ImageSequenceFactory factory;
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());

    const quint64 initialRequestId = item.activeRequestIdForTest();
    QVERIFY(initialRequestId > 0);
    QVERIFY(item.pendingRenderPayloadIdForTest() > 0);
    QCOMPARE(item.displayedRequestIdForTest(), 0U);

    acknowledgePendingRenderCommit(item);
    QCOMPARE(item.displayedRequestIdForTest(), initialRequestId);
    QCOMPARE(item.pendingRenderPayloadIdForTest(), 0U);

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    const quint64 seekRequestId = item.activeRequestIdForTest();
    QVERIFY(seekRequestId > initialRequestId);
    QVERIFY(item.pendingRenderPayloadIdForTest() > 0);
    QCOMPARE(item.displayedRequestIdForTest(), initialRequestId);

    acknowledgePendingRenderCommit(item);
    QCOMPARE(item.displayedRequestIdForTest(), seekRequestId);
    QCOMPARE(item.pendingRenderPayloadIdForTest(), 0U);
}

void ImageViewportRenderCommitTest::providerTimedFramePaintFailureRetainsPreviousDisplay()
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
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    acknowledgePendingRenderFailure(item);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("render commit failed")));
}

void ImageViewportRenderCommitTest::providerTimedPlaybackPaintFailureStopsPlayback()
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
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
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

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    acknowledgePendingRenderFailure(item);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("render commit failed")));
}

void ImageViewportRenderCommitTest::providerTimedPlayAfterPaintFailureRestartsPlaybackRequest()
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
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    acknowledgePendingRenderFailure(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("errorString").toString(), QString());

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportRenderCommitTest::providerSupersededRenderWaitingClearsPendingRenderCommit()
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
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(item.hasPendingRenderCommitForTest());
    acknowledgePendingRenderCommit(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(!item.hasPendingRenderCommitForTest());

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QVERIFY(item.hasPendingRenderCommitForTest());

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QVERIFY(!item.hasPendingRenderCommitForTest());
}

void ImageViewportRenderCommitTest::providerSupersededRenderFailureIsIgnored()
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
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QVERIFY(item.hasPendingRenderCommitForTest());

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(!item.hasPendingRenderCommitForTest());

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    acknowledgePendingRenderFailure(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(requestStateSpy.count(), 0);
    QCOMPARE(displayStateSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 0);
}

void ImageViewportRenderCommitTest::
    providerRenderCommitAcknowledgementPromotesPendingFrameWithoutSceneGraph()
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
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QVERIFY(item.hasPendingRenderCommitForTest());
    QCOMPARE(item.displayedRequestIdForTest(), 0U);

    const quint64 generation = item.pendingRenderGenerationForTest();
    const quint64 requestId = item.activeRequestIdForTest();
    const quint64 payloadId = item.pendingRenderPayloadIdForTest();
    QVERIFY(generation > 0);
    QVERIFY(requestId > 0);
    QVERIFY(payloadId > 0);

    item.acknowledgeRenderCommitForTest(generation, requestId, payloadId);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.displayedRequestIdForTest(), requestId);
    QVERIFY(!item.hasPendingRenderCommitForTest());
}

void ImageViewportRenderCommitTest::secondaryProviderSpreadRenderFailureRetainsPreviousDisplay()
{
    ImageSequenceFactory factory;
    QImage previousImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    previousImage.fill(QColor(255, 0, 0, 255));
    ImageFrame previousFrame(previousImage);
    QScopedPointer<ImageSequenceFactoryResult> previousResult(factory.fromFrame(&previousFrame));
    QVERIFY(previousResult->sequence());

    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(QColor(0, 255, 0, 255));
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
    item.setSize(QSizeF(88.0, 44.0));
    item.setSequence(previousResult->sequence());
    acknowledgePendingRenderCommit(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));

    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);

    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 0, 255, 255));
    ImageFrame secondaryFrame(secondaryImage);
    emit sessionFactory->lastSession()->imageFrameReady(
        sessionFactory->lastSession()->lastFrameToken(), &secondaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));
    QVERIFY(item.hasPendingRenderCommitForTest());

    acknowledgePendingRenderFailure(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("render commit failed")));
    QVERIFY(!item.hasPendingRenderCommitForTest());
}

void ImageViewportRenderCommitTest::twoPageSinglePayloadCommitAcknowledgementIsIncomplete()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(QColor(0, 255, 0, 255));
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
    item.setSize(QSizeF(88.0, 44.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 0, 255, 255));
    ImageFrame secondaryFrame(secondaryImage);
    emit sessionFactory->lastSession()->imageFrameReady(
        sessionFactory->lastSession()->lastFrameToken(), &secondaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(item.hasPendingRenderCommitForTest());

    const quint64 generation = item.pendingRenderGenerationForTest();
    const quint64 requestId = item.activeRequestIdForTest();
    const quint64 payloadId = item.pendingRenderPayloadIdForTest();
    const quint64 secondaryPayloadId = item.secondaryPendingRenderPayloadIdForTest();
    QVERIFY(secondaryPayloadId > 0);
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");

    item.acknowledgeRenderCommitForTest(generation, requestId, payloadId);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QVERIFY(item.hasPendingRenderCommitForTest());

    item.acknowledgeRenderCommitForTest(generation, requestId, payloadId, secondaryPayloadId);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.displayedRequestIdForTest(), requestId);
    QVERIFY(!item.hasPendingRenderCommitForTest());
}

void ImageViewportRenderCommitTest::secondaryRoleRenderFailureReportsFailureWithoutSceneGraph()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(QColor(0, 255, 0, 255));
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
    item.setSize(QSizeF(88.0, 44.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 0, 255, 255));
    ImageFrame secondaryFrame(secondaryImage);
    emit sessionFactory->lastSession()->imageFrameReady(
        sessionFactory->lastSession()->lastFrameToken(), &secondaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(item.hasPendingRenderCommitForTest());

    item.acknowledgeRenderFailureForTest(ImageViewport::PageRole::Secondary,
        item.pendingRenderGenerationForTest(), item.activeRequestIdForTest(),
        item.secondaryPendingRenderPayloadIdForTest());

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.displayedRequestIdForTest(), 0U);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("render commit failed")));
    QVERIFY(!item.hasPendingRenderCommitForTest());
}

void ImageViewportRenderCommitTest::staleSecondaryRoleRenderFailureIsIgnoredWithoutSceneGraph()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(QColor(0, 255, 0, 255));
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
    item.setSize(QSizeF(88.0, 44.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 0, 255, 255));
    ImageFrame secondaryFrame(secondaryImage);
    emit sessionFactory->lastSession()->imageFrameReady(
        sessionFactory->lastSession()->lastFrameToken(), &secondaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(item.hasPendingRenderCommitForTest());

    const quint64 generation = item.pendingRenderGenerationForTest();
    const quint64 requestId = item.activeRequestIdForTest();
    const quint64 payloadId = item.secondaryPendingRenderPayloadIdForTest();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    item.acknowledgeRenderFailureForTest(
        ImageViewport::PageRole::Secondary, generation, requestId, payloadId + 1);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(requestStateSpy.count(), 0);
    QCOMPARE(displayStateSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 0);
    QVERIFY(item.hasPendingRenderCommitForTest());
}

void ImageViewportRenderCommitTest::staleRenderCommitAcknowledgementIsIgnoredWithoutSceneGraph()
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
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(item.hasPendingRenderCommitForTest());

    const quint64 generation = item.pendingRenderGenerationForTest();
    const quint64 requestId = item.activeRequestIdForTest();
    const quint64 payloadId = item.pendingRenderPayloadIdForTest();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    item.acknowledgeRenderCommitForTest(generation, requestId, payloadId + 1);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(requestStateSpy.count(), 0);
    QCOMPARE(displayStateSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 0);
    QVERIFY(item.hasPendingRenderCommitForTest());
}

void ImageViewportRenderCommitTest::staleRenderFailureAcknowledgementIsIgnoredWithoutSceneGraph()
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
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    item.acknowledgeRenderCommitForTest(item.pendingRenderGenerationForTest(),
        item.activeRequestIdForTest(), item.pendingRenderPayloadIdForTest());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(item.hasPendingRenderCommitForTest());

    const quint64 generation = item.pendingRenderGenerationForTest();
    const quint64 requestId = item.activeRequestIdForTest();
    const quint64 payloadId = item.pendingRenderPayloadIdForTest();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    item.acknowledgeRenderFailureForTest(generation, requestId, payloadId + 1);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(requestStateSpy.count(), 0);
    QCOMPARE(displayStateSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 0);
    QVERIFY(item.hasPendingRenderCommitForTest());
}

void ImageViewportRenderCommitTest::
    activeRenderFailureAcknowledgementReportsFailureWithoutSceneGraph()
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
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    item.acknowledgeRenderCommitForTest(item.pendingRenderGenerationForTest(),
        item.activeRequestIdForTest(), item.pendingRenderPayloadIdForTest());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(item.hasPendingRenderCommitForTest());

    const quint64 generation = item.pendingRenderGenerationForTest();
    const quint64 requestId = item.activeRequestIdForTest();
    const quint64 payloadId = item.pendingRenderPayloadIdForTest();
    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    item.acknowledgeRenderFailureForTest(generation, requestId, payloadId);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("render commit failed")));
    QVERIFY(!item.hasPendingRenderCommitForTest());
    QCOMPARE(requestStateSpy.count(), 1);
    QCOMPARE(displayStateSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 1);
}

void ImageViewportRenderCommitTest::
    playbackWaitingRenderCommitAcknowledgementResumesWithoutSceneGraph()
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
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    item.acknowledgeRenderCommitForTest(item.pendingRenderGenerationForTest(),
        item.activeRequestIdForTest(), item.pendingRenderPayloadIdForTest());

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(item.hasPendingRenderCommitForTest());

    const quint64 generation = item.pendingRenderGenerationForTest();
    const quint64 requestId = item.activeRequestIdForTest();
    const quint64 payloadId = item.pendingRenderPayloadIdForTest();
    QSignalSpy playbackSpy(&item, &ImageViewport::playbackPhaseChanged);

    item.acknowledgeRenderCommitForTest(generation, requestId, payloadId);

    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.displayedRequestIdForTest(), requestId);
    QVERIFY(!item.hasPendingRenderCommitForTest());
    QCOMPARE(playbackSpy.count(), 1);
}

void ImageViewportRenderCommitTest::geometryChangeRecoversRenderWaitingWithoutSceneGraph()
{
    ImageSequenceFactory factory;
    QImage firstImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(QColor(255, 0, 0, 255));
    QImage secondImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(QColor(0, 255, 0, 255));
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();
    const quint64 initialRequestId = item.activeRequestIdForTest();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.setSize(QSizeF(0.0, 20.0));
    item.advancePlaybackForTest(100);

    const quint64 waitingRequestId = item.activeRequestIdForTest();
    QVERIFY(waitingRequestId > initialRequestId);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    QSignalSpy playbackSpy(&item, &ImageViewport::playbackPhaseChanged);
    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);

    item.setSize(QSizeF(40.0, 20.0));

    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.activeRequestIdForTest(), waitingRequestId);
    QVERIFY(item.hasPendingRenderCommitForTest());
    QCOMPARE(item.pendingRenderGenerationForTest(), 1U);
    QCOMPARE(item.pendingRenderPayloadIdForTest(), waitingRequestId);
    QCOMPARE(playbackSpy.count(), 1);
    QCOMPARE(requestStateSpy.count(), 1);
    QCOMPARE(displayStateSpy.count(), 1);
}

QTEST_MAIN(ImageViewportRenderCommitTest)

#include "tst_imageviewport_render_commit.moc"
