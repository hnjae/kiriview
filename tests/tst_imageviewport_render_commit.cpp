#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

class ImageViewportRenderCommitTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportRenderCommitTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void stillAssignmentWaitsForRenderCommitWithPositiveGeometry();
    void timedListAssignmentWaitsForRenderCommitWithPositiveGeometry();
    void builtInTwoPageSpreadWaitsForCompleteRenderCommit();
    void mixedBuiltInProviderSpreadWaitsForCompleteRenderCommit_data();
    void mixedBuiltInProviderSpreadWaitsForCompleteRenderCommit();
    void staleBuiltInRenderAcknowledgementIsIgnored();
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
    void activeRenderFailureDiagnosticsPreserveCause_data();
    void activeRenderFailureDiagnosticsPreserveCause();
    void staleRenderFailureDoesNotOverwriteActiveDiagnostics();
    void playbackWaitingRenderCommitAcknowledgementResumesWithoutSceneGraph();
    void geometryChangeRecoversRenderWaitingWithoutSceneGraph();
};

void ImageViewportRenderCommitTest::stillAssignmentWaitsForRenderCommitWithPositiveGeometry()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(hasPendingRenderCommitForTest(item));
    QVERIFY(pendingRenderPayloadIdForTest(item) > 0);

    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QVERIFY(!hasPendingRenderCommitForTest(item));
}

void ImageViewportRenderCommitTest::timedListAssignmentWaitsForRenderCommitWithPositiveGeometry()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(QColor(255, 0, 0, 255));
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(QColor(0, 255, 0, 255));
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QVERIFY(!hasPendingRenderCommitForTest(item));
}

void ImageViewportRenderCommitTest::builtInTwoPageSpreadWaitsForCompleteRenderCommit()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(QColor(0, 255, 0, 255));
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 0, 255, 255));
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(120.0, 40.0));
    QCOMPARE(item.setPageSet(primaryResult->sequence(), secondaryResult->sequence()),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    const quint64 secondaryPayloadId = secondaryPendingRenderPayloadIdForTest(item);
    QVERIFY(payloadId > 0);
    QVERIFY(secondaryPayloadId > 0);
    QVERIFY(secondaryPayloadId != payloadId);
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId, secondaryPayloadId);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(10.0, 20.0));
    QVERIFY(!hasPendingRenderCommitForTest(item));
}

void ImageViewportRenderCommitTest::mixedBuiltInProviderSpreadWaitsForCompleteRenderCommit_data()
{
    QTest::addColumn<bool>("providerIsPrimary");

    QTest::newRow("built-in-primary-provider-secondary") << false;
    QTest::newRow("provider-primary-built-in-secondary") << true;
}

void ImageViewportRenderCommitTest::mixedBuiltInProviderSpreadWaitsForCompleteRenderCommit()
{
    QFETCH(bool, providerIsPrimary);

    ImageSequenceFactory factory;
    QImage builtInImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    builtInImage.fill(QColor(0, 255, 0, 255));
    ImageFrame builtInFrame(builtInImage);
    QScopedPointer<ImageSequenceFactoryResult> builtInResult(factory.fromFrame(&builtInFrame));
    QVERIFY(builtInResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> providerResult(factory.fromProvider(&adapter));
    QVERIFY(providerResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(120.0, 40.0));
    QCOMPARE(
        item.setPageSet(providerIsPrimary ? providerResult->sequence() : builtInResult->sequence(),
            providerIsPrimary ? builtInResult->sequence() : providerResult->sequence()),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QImage providerImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    providerImage.fill(QColor(0, 0, 255, 255));
    ImageFrame providerFrame(providerImage);
    emit sessionFactory->lastSession()->imageFrameReady(
        sessionFactory->lastSession()->lastFrameToken(), &providerFrame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 primaryPayloadId = pendingRenderPayloadIdForTest(item);

    acknowledgeRenderCommitForTest(item, generation, requestId, primaryPayloadId);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(!hasPendingRenderCommitForTest(item));
}

void ImageViewportRenderCommitTest::staleBuiltInRenderAcknowledgementIsIgnored()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId + 1);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(requestStateSpy.count(), 0);
    QCOMPARE(displayStateSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 0);
    QVERIFY(hasPendingRenderCommitForTest(item));
}

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
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));

    acknowledgePendingPrimaryRenderFailureForTest(item);
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
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
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
    acknowledgePendingPrimaryRenderCommitForTest(item);
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
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    acknowledgePendingPrimaryRenderFailureForTest(item);
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
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.setSize(QSizeF(0.0, 20.0));
    advancePlaybackForTest(item, 100);
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
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    acknowledgePendingPrimaryRenderFailureForTest(item);
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
    acknowledgePendingPrimaryRenderCommitForTest(item);

    item.setSize(QSizeF(0.0, 20.0));
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    item.setSize(QSizeF(40.0, 20.0));
    acknowledgePendingPrimaryRenderFailureForTest(item);

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
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
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

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    acknowledgeRenderFailureForTest(item, generation, requestId, payloadId);
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

    const quint64 initialRequestId = activeRequestIdForTest(item);
    QVERIFY(initialRequestId > 0);
    QVERIFY(pendingRenderPayloadIdForTest(item) > 0);
    QCOMPARE(displayedRequestIdForTest(item), 0U);

    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(displayedRequestIdForTest(item), initialRequestId);
    QCOMPARE(pendingRenderPayloadIdForTest(item), 0U);

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    const quint64 seekRequestId = activeRequestIdForTest(item);
    QVERIFY(seekRequestId > initialRequestId);
    QVERIFY(pendingRenderPayloadIdForTest(item) > 0);
    QCOMPARE(displayedRequestIdForTest(item), initialRequestId);

    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(displayedRequestIdForTest(item), seekRequestId);
    QCOMPARE(pendingRenderPayloadIdForTest(item), 0U);
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
    acknowledgePendingPrimaryRenderCommitForTest(item);

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

    acknowledgePendingPrimaryRenderFailureForTest(item);
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
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 100);

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

    acknowledgePendingPrimaryRenderFailureForTest(item);
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
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    acknowledgePendingPrimaryRenderFailureForTest(item);

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
    acknowledgePendingPrimaryRenderCommitForTest(item);

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
    QVERIFY(hasPendingRenderCommitForTest(item));
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(!hasPendingRenderCommitForTest(item));

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QVERIFY(hasPendingRenderCommitForTest(item));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QVERIFY(!hasPendingRenderCommitForTest(item));
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
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(!hasPendingRenderCommitForTest(item));

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    acknowledgePendingPrimaryRenderFailureForTest(item);
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
    QVERIFY(hasPendingRenderCommitForTest(item));
    QCOMPARE(displayedRequestIdForTest(item), 0U);

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    QVERIFY(generation > 0);
    QVERIFY(requestId > 0);
    QVERIFY(payloadId > 0);

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(displayedRequestIdForTest(item), requestId);
    QVERIFY(!hasPendingRenderCommitForTest(item));
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
    acknowledgePendingPrimaryRenderCommitForTest(item);
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
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgePendingPrimaryRenderFailureForTest(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("render commit failed")));
    QVERIFY(!hasPendingRenderCommitForTest(item));
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
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    const quint64 secondaryPayloadId = secondaryPendingRenderPayloadIdForTest(item);
    QVERIFY(secondaryPayloadId > 0);
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId, secondaryPayloadId);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(displayedRequestIdForTest(item), requestId);
    QVERIFY(!hasPendingRenderCommitForTest(item));
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
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgeRenderFailureForTest(item, ImageViewport::PageRole::Secondary,
        pendingRenderGenerationForTest(item), activeRequestIdForTest(item),
        secondaryPendingRenderPayloadIdForTest(item));

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(displayedRequestIdForTest(item), 0U);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("render commit failed")));
    QVERIFY(!hasPendingRenderCommitForTest(item));
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
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = secondaryPendingRenderPayloadIdForTest(item);
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    acknowledgeRenderFailureForTest(
        item, ImageViewport::PageRole::Secondary, generation, requestId, payloadId + 1);

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
    QVERIFY(hasPendingRenderCommitForTest(item));
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
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId + 1);

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
    QVERIFY(hasPendingRenderCommitForTest(item));
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
    acknowledgeRenderCommitForTest(item, pendingRenderGenerationForTest(item),
        activeRequestIdForTest(item), pendingRenderPayloadIdForTest(item));
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
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    acknowledgeRenderFailureForTest(item, generation, requestId, payloadId + 1);

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
    QVERIFY(hasPendingRenderCommitForTest(item));
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
    acknowledgeRenderCommitForTest(item, pendingRenderGenerationForTest(item),
        activeRequestIdForTest(item), pendingRenderPayloadIdForTest(item));
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
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    QSignalSpy requestStateSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    acknowledgeRenderFailureForTest(item, generation, requestId, payloadId);

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
    QVERIFY(!hasPendingRenderCommitForTest(item));
    QCOMPARE(requestStateSpy.count(), 1);
    QCOMPARE(displayStateSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 1);
}

void ImageViewportRenderCommitTest::activeRenderFailureDiagnosticsPreserveCause_data()
{
    QTest::addColumn<RenderFailureCause>("cause");

    QTest::newRow("missing-window") << RenderFailureCause::MissingWindow;
    QTest::newRow("texture-creation") << RenderFailureCause::TextureCreationFailure;
    QTest::newRow("image-node-creation") << RenderFailureCause::ImageNodeCreationFailure;
    QTest::newRow("invalid-role-payload") << RenderFailureCause::InvalidRolePayload;
    QTest::newRow("unknown-backend") << RenderFailureCause::UnknownBackendFailure;
}

void ImageViewportRenderCommitTest::activeRenderFailureDiagnosticsPreserveCause()
{
    QFETCH(RenderFailureCause, cause);

    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    QVERIFY(generation > 0);
    QVERIFY(requestId > 0);
    QVERIFY(payloadId > 0);

    acknowledgeRenderFailureForTest(
        item, ImageViewport::PageRole::Primary, generation, requestId, payloadId, cause);

    const RenderFailureDiagnosticForTest diagnostic
        = lastAcceptedRenderFailureDiagnosticForTest(item);
    QVERIFY(diagnostic.valid);
    QCOMPARE(diagnostic.role, ImageViewport::PageRole::Primary);
    QCOMPARE(diagnostic.generation, generation);
    QCOMPARE(diagnostic.requestId, requestId);
    QCOMPARE(diagnostic.preparedPayloadId, payloadId);
    QCOMPARE(diagnostic.cause, cause);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("render commit failed")));
}

void ImageViewportRenderCommitTest::staleRenderFailureDoesNotOverwriteActiveDiagnostics()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    acknowledgeRenderFailureForTest(item, ImageViewport::PageRole::Primary, generation, requestId,
        payloadId, RenderFailureCause::TextureCreationFailure);

    const RenderFailureDiagnosticForTest activeDiagnostic
        = lastAcceptedRenderFailureDiagnosticForTest(item);
    QVERIFY(activeDiagnostic.valid);
    QCOMPARE(activeDiagnostic.cause, RenderFailureCause::TextureCreationFailure);

    acknowledgeRenderFailureForTest(item, ImageViewport::PageRole::Primary, generation, requestId,
        payloadId + 1, RenderFailureCause::ImageNodeCreationFailure);

    const RenderFailureDiagnosticForTest staleDiagnostic
        = lastAcceptedRenderFailureDiagnosticForTest(item);
    QVERIFY(staleDiagnostic.valid);
    QCOMPARE(staleDiagnostic.role, activeDiagnostic.role);
    QCOMPARE(staleDiagnostic.generation, activeDiagnostic.generation);
    QCOMPARE(staleDiagnostic.requestId, activeDiagnostic.requestId);
    QCOMPARE(staleDiagnostic.preparedPayloadId, activeDiagnostic.preparedPayloadId);
    QCOMPARE(staleDiagnostic.cause, activeDiagnostic.cause);
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
    acknowledgeRenderCommitForTest(item, pendingRenderGenerationForTest(item),
        activeRequestIdForTest(item), pendingRenderPayloadIdForTest(item));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 100);
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
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    QSignalSpy playbackSpy(&item, &ImageViewport::playbackPhaseChanged);

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId);

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
    QCOMPARE(displayedRequestIdForTest(item), requestId);
    QVERIFY(!hasPendingRenderCommitForTest(item));
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
    acknowledgePendingPrimaryRenderCommitForTest(item);
    const quint64 initialRequestId = activeRequestIdForTest(item);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.setSize(QSizeF(0.0, 20.0));
    advancePlaybackForTest(item, 100);

    const quint64 waitingRequestId = activeRequestIdForTest(item);
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
    QCOMPARE(activeRequestIdForTest(item), waitingRequestId);
    QVERIFY(hasPendingRenderCommitForTest(item));
    QCOMPARE(pendingRenderGenerationForTest(item), 1U);
    QVERIFY(pendingRenderPayloadIdForTest(item) > 0);
    QCOMPARE(playbackSpy.count(), 0);
    QCOMPARE(requestStateSpy.count(), 0);
    QCOMPARE(displayStateSpy.count(), 0);

    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(playbackSpy.count(), 1);
    QCOMPARE(requestStateSpy.count(), 1);
    QCOMPARE(displayStateSpy.count(), 1);
}

QTEST_MAIN(ImageViewportRenderCommitTest)

#include "tst_imageviewport_render_commit.moc"
