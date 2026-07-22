// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

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

private Q_SLOTS:
    void stillAssignmentWaitsForRenderCommitWithPositiveGeometry();
    void timedListAssignmentWaitsForRenderCommitWithPositiveGeometry();
    void builtInTwoPageSpreadWaitsForCompleteRenderCommit();
    void mixedBuiltInProviderSpreadWaitsForCompleteRenderCommit_data();
    void mixedBuiltInProviderSpreadWaitsForCompleteRenderCommit();
    void mixedSpreadSeekReusesUnchangedProviderRole();
    void staleBuiltInRenderAcknowledgementIsIgnored();
    void stillImagePaintFailureReportsRenderFailure();
    void timedFrameListPaintFailureRetainsPreviousDisplay();
    void timedFrameListPlaybackPaintFailureStopsPlayback();
    void timedFrameListPlayAfterPaintFailureRestartsDisplayRequest();
    void committedDisplayRebuildFailureUsesNormalFailurePath();
    void builtInSameFrameSeekCreatesFreshRequestIdentity();
    void providerTimedFramePaintFailureRetainsPreviousDisplay();
    void providerTimedPlaybackPaintFailureStopsPlayback();
    void providerTimedPlayAfterPaintFailureRestartsPlaybackRequest();
    void providerStaleRenderWaitingClearsPendingRenderCommit();
    void providerStaleRenderFailureIsIgnored();
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
    void renderingQualityFallbackOwnsWarningAndDisplayRevision();
    void renderingQualityFallbackFollowsRequestedPreferences();
    void renderingQualityFallbackSurvivesSameGenerationSeekUntilDisplayIsDiscarded();
    void replacedGenerationCannotRestoreRenderingQualityFallback();
};

void ImageViewportRenderCommitTest::renderingQualityFallbackOwnsWarningAndDisplayRevision()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Ready);

    const quint64 attempt = currentRenderAttemptForTest(item);
    QVERIFY(attempt > 0);
    const ImageViewportRevisionToken requestBefore = viewportRequestRevision(item);
    const ImageViewportRevisionToken displayBefore = viewportDisplayRevision(item);
    const ImageViewportRevisionToken snapshotBefore = item.state().revisions().snapshot();

    reportRenderQualityFallbackForTest(item, attempt, true, false);
    QVERIFY(!viewportWarningString(item).isEmpty());
    QCOMPARE(viewportRequestRevision(item), requestBefore);
    QVERIFY(viewportDisplayRevision(item) != displayBefore);
    QVERIFY(item.state().revisions().snapshot() != snapshotBefore);
    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Ready);

    const QString activeWarning = viewportWarningString(item);
    const ImageViewportRevisionToken activeDisplayRevision = viewportDisplayRevision(item);
    const ImageViewportRevisionToken activeSnapshotRevision = item.state().revisions().snapshot();
    reportRenderQualityFallbackForTest(item, currentRenderAttemptForTest(item) - 1, false, false);
    QCOMPARE(viewportWarningString(item), activeWarning);
    QCOMPARE(viewportDisplayRevision(item), activeDisplayRevision);
    QCOMPARE(item.state().revisions().snapshot(), activeSnapshotRevision);

    reportRenderQualityFallbackForTest(item, currentRenderAttemptForTest(item), false, false);
    QVERIFY(viewportWarningString(item).isEmpty());
    QCOMPARE(viewportRequestRevision(item), requestBefore);
    QVERIFY(viewportDisplayRevision(item) != activeDisplayRevision);
    QVERIFY(item.state().revisions().snapshot() != activeSnapshotRevision);
    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Ready);

    reportRenderQualityFallbackForTest(item, currentRenderAttemptForTest(item), true, true);
    QVERIFY(!viewportWarningString(item).isEmpty());
    item.clear();
    QVERIFY(viewportWarningString(item).isEmpty());
}

void ImageViewportRenderCommitTest::renderingQualityFallbackFollowsRequestedPreferences()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingPrimaryRenderCommitForTest(item);

    ImageViewportPresentationCommand enableMipmap;
    enableMipmap.setMipmap(true);
    QCOMPARE(item.setPresentation(enableMipmap).outcome(), ImageViewportCommandOutcome::Accepted);
    const ImageViewportRevisionToken requestRevision = viewportRequestRevision(item);

    reportRenderQualityFallbackForTest(item, currentRenderAttemptForTest(item), true, true);
    QVERIFY(!viewportWarningString(item).isEmpty());
    QCOMPARE(viewportRequestRevision(item), requestRevision);

    ImageViewportPresentationCommand disableMipmap;
    disableMipmap.setMipmap(false);
    QCOMPARE(item.setPresentation(disableMipmap).outcome(), ImageViewportCommandOutcome::Accepted);
    QVERIFY(!viewportWarningString(item).isEmpty());

    ImageViewportPresentationCommand disableSmoothing;
    disableSmoothing.setSmoothing(false);
    QCOMPARE(
        item.setPresentation(disableSmoothing).outcome(), ImageViewportCommandOutcome::Accepted);
    QVERIFY(viewportWarningString(item).isEmpty());
    QCOMPARE(viewportRequestRevision(item), requestRevision);

    reportRenderQualityFallbackForTest(item, currentRenderAttemptForTest(item), true, true);
    QVERIFY(viewportWarningString(item).isEmpty());

    ImageViewportPresentationCommand reenablePreferences;
    reenablePreferences.setSmoothing(true);
    reenablePreferences.setMipmap(true);
    QCOMPARE(
        item.setPresentation(reenablePreferences).outcome(), ImageViewportCommandOutcome::Accepted);
    QVERIFY(viewportWarningString(item).isEmpty());

    reportRenderQualityFallbackForTest(item, currentRenderAttemptForTest(item), true, true);
    QVERIFY(!viewportWarningString(item).isEmpty());
    QCOMPARE(viewportRequestRevision(item), requestRevision);
}

void ImageViewportRenderCommitTest::
    renderingQualityFallbackSurvivesSameGenerationSeekUntilDisplayIsDiscarded()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::red);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::green);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList frames;
    QVERIFY(frames.appendFrame(&firstFrame, 100));
    QVERIFY(frames.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&frames));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingPrimaryRenderCommitForTest(item);
    reportRenderQualityFallbackForTest(item, currentRenderAttemptForTest(item), true, false);
    QVERIFY(!viewportWarningString(item).isEmpty());

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().display().status(), ImageViewportDisplayStatus::Retained);
    QVERIFY(!viewportWarningString(item).isEmpty());

    const ImageViewportRevisionToken requestRevision = viewportRequestRevision(item);
    const ImageViewportRevisionToken displayRevision = viewportDisplayRevision(item);
    const ImageViewportRevisionToken snapshotRevision = item.state().revisions().snapshot();
    discardRetainedDisplayForResourcePressureForTest(item);

    QVERIFY(viewportWarningString(item).isEmpty());
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QVERIFY(viewportDisplayRevision(item) != displayRevision);
    QVERIFY(item.state().revisions().snapshot() != snapshotRevision);
}

void ImageViewportRenderCommitTest::replacedGenerationCannotRestoreRenderingQualityFallback()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> replacement(factory.fromProvider(&adapter));
    QVERIFY(replacement->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const quint64 replacedGeneration = pendingRenderGenerationForTest(item);
    acknowledgePendingPrimaryRenderCommitForTest(item);
    reportRenderQualityFallbackForTest(item, currentRenderAttemptForTest(item), true, false);
    QVERIFY(!viewportWarningString(item).isEmpty());

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(replacement->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().display().status(), ImageViewportDisplayStatus::Retained);
    QVERIFY(viewportWarningString(item).isEmpty());

    reportRenderQualityFallbackForTest(item, currentRenderAttemptForTest(item), true, false);
    QVERIFY(viewportWarningString(item).isEmpty());

    const auto observations = internalObservationsForTest(item);
    QVERIFY(!observations.isEmpty());
    const InternalObservationForTest stale = observations.constLast();
    QCOMPARE(stale.subsystem, InternalObservationSubsystemForTest::Engine);
    QCOMPARE(stale.category, InternalObservationCategoryForTest::StaleDrop);
    QCOMPARE(stale.cause, InternalObservationCauseForTest::StaleRenderQualityFallback);
    QCOMPARE(stale.identity.generation, replacedGeneration);
    QVERIFY(stale.identity.renderAttempt > 0);
}

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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(hasPendingRenderCommitForTest(item));
    QVERIFY(pendingRenderPayloadIdForTest(item) > 0);

    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    const quint64 secondaryPayloadId = secondaryPendingRenderPayloadIdForTest(item);
    QVERIFY(payloadId > 0);
    QVERIFY(secondaryPayloadId > 0);
    QVERIFY(secondaryPayloadId != payloadId);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId, secondaryPayloadId);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(10.0, 20.0));
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
        item.setPresentationTarget(
                ImageViewportPresentationTarget(
                    providerIsPrimary ? providerResult->sequence() : builtInResult->sequence(),
                    providerIsPrimary ? builtInResult->sequence() : providerResult->sequence()),
                PresentationTargetTransitionPolicy {})
            .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QImage providerImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    providerImage.fill(QColor(0, 0, 255, 255));
    ImageFrame providerFrame(providerImage);
    emitProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastFrameToken(), &providerFrame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 primaryPayloadId = pendingRenderPayloadIdForTest(item);

    acknowledgeRenderCommitForTest(item, generation, requestId, primaryPayloadId);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QVERIFY(!hasPendingRenderCommitForTest(item));
}

void ImageViewportRenderCommitTest::mixedSpreadSeekReusesUnchangedProviderRole()
{
    ImageSequenceFactory factory;
    QImage firstBuiltInImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    firstBuiltInImage.fill(QColor(0, 255, 0, 255));
    QImage secondBuiltInImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    secondBuiltInImage.fill(QColor(255, 255, 0, 255));
    ImageFrame firstBuiltInFrame(firstBuiltInImage);
    ImageFrame secondBuiltInFrame(secondBuiltInImage);
    TimedImageFrameList builtInFrames;
    QVERIFY(builtInFrames.appendFrame(&firstBuiltInFrame, 100));
    QVERIFY(builtInFrames.appendFrame(&secondBuiltInFrame, 100));
    QScopedPointer<ImageSequenceFactoryResult> builtInResult(
        factory.fromTimedFrameList(&builtInFrames));
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            builtInResult->sequence(), providerResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QImage providerImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    providerImage.fill(QColor(0, 0, 255, 255));
    ImageFrame providerFrame(providerImage);
    emitProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastFrameToken(), &providerFrame);
    drainQueuedProviderResults();

    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Ready);
    QCOMPARE(item.state().display().displayedRoleSet(), ImageViewportRoleSet(true, true));
    QCOMPARE(*frameRequestCount, 1);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Loading);
    QVERIFY(hasPendingRenderCommitForTest(item));
    QVERIFY(pendingRenderPayloadIdForTest(item) > 0);
    QVERIFY(secondaryPendingRenderPayloadIdForTest(item) > 0);
    QCOMPARE(*frameRequestCount, 1);

    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Ready);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(secondaryDisplayedFrame(item), 0);
    QCOMPARE(item.state().display().displayedRoleSet(), ImageViewportRoleSet(true, true));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId + 1);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    const RenderFailureDiagnosticForTest diagnostic
        = lastAcceptedRenderFailureDiagnosticForTest(item);
    QVERIFY(!diagnostic.valid);
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));

    acknowledgePendingPrimaryRenderFailureForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("render commit failed")));

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(viewportErrorString(item), QString());
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(4.0, 2.0));

    item.setSize(QSizeF(0.0, 20.0));
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);

    item.setSize(QSizeF(40.0, 20.0));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryDisplayedFrame(item), 0);

    acknowledgePendingPrimaryRenderFailureForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(4.0, 2.0));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("render commit failed")));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    item.setSize(QSizeF(0.0, 20.0));
    advancePlaybackForTest(item, 100);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    item.setSize(QSizeF(40.0, 20.0));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);

    acknowledgePendingPrimaryRenderFailureForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("render commit failed")));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();
    acknowledgePendingPrimaryRenderCommitForTest(item);

    item.setSize(QSizeF(0.0, 20.0));
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    item.setSize(QSizeF(40.0, 20.0));
    acknowledgePendingPrimaryRenderFailureForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportRenderCommitTest::committedDisplayRebuildFailureUsesNormalFailurePath()
{
    ImageSequenceFactory factory;
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(40.0, 20.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));

    acknowledgeRenderFailureForTest(item, generation, requestId, payloadId);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("render commit failed")));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    const quint64 initialRequestId = activeRequestIdForTest(item);
    QVERIFY(initialRequestId > 0);
    QVERIFY(pendingRenderPayloadIdForTest(item) > 0);
    QCOMPARE(displayedRequestIdForTest(item), 0U);

    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(displayedRequestIdForTest(item), initialRequestId);
    QCOMPARE(pendingRenderPayloadIdForTest(item), 0U);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Accepted);

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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    acknowledgePendingPrimaryRenderFailureForTest(item);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(4.0, 2.0));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("render commit failed")));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    advancePlaybackForTest(item, 100);

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    acknowledgePendingPrimaryRenderFailureForTest(item);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(4.0, 2.0));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("render commit failed")));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    acknowledgePendingPrimaryRenderFailureForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(viewportErrorString(item), QString());

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportRenderCommitTest::providerStaleRenderWaitingClearsPendingRenderCommit()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(hasPendingRenderCommitForTest(item));
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QVERIFY(!hasPendingRenderCommitForTest(item));

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QVERIFY(hasPendingRenderCommitForTest(item));

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QVERIFY(!hasPendingRenderCommitForTest(item));
}

void ImageViewportRenderCommitTest::providerStaleRenderFailureIsIgnored()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QVERIFY(!hasPendingRenderCommitForTest(item));

    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    acknowledgePendingPrimaryRenderFailureForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QVERIFY(hasPendingRenderCommitForTest(item));
    QCOMPARE(displayedRequestIdForTest(item), 0U);

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    QVERIFY(generation > 0);
    QVERIFY(requestId > 0);
    QVERIFY(payloadId > 0);

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
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
    item.setPresentationTarget(ImageViewportPresentationTarget(previousResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingPrimaryRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(displayedImageSize(item), QSizeF(4.0, 2.0));

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(displayedImageSize(item), QSizeF(4.0, 2.0));

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);

    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 0, 255, 255));
    ImageFrame secondaryFrame(secondaryImage);
    emitProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastFrameToken(), &secondaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(displayedImageSize(item), QSizeF(4.0, 2.0));
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgePendingPrimaryRenderFailureForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(displayedImageSize(item), QSizeF(4.0, 2.0));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("render commit failed")));
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 0, 255, 255));
    ImageFrame secondaryFrame(secondaryImage);
    emitProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastFrameToken(), &secondaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    const quint64 secondaryPayloadId = secondaryPendingRenderPayloadIdForTest(item);
    QVERIFY(secondaryPayloadId > 0);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId, secondaryPayloadId);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 0, 255, 255));
    ImageFrame secondaryFrame(secondaryImage);
    emitProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastFrameToken(), &secondaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgeRenderFailureForTest(item, ImageViewportPageRole::Secondary,
        pendingRenderGenerationForTest(item), activeRequestIdForTest(item),
        secondaryPendingRenderPayloadIdForTest(item));

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(displayedRequestIdForTest(item), 0U);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("render commit failed")));
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 0, 255, 255));
    ImageFrame secondaryFrame(secondaryImage);
    emitProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastFrameToken(), &secondaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = secondaryPendingRenderPayloadIdForTest(item);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    acknowledgeRenderFailureForTest(
        item, ImageViewportPageRole::Secondary, generation, requestId, payloadId + 1);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId + 1);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgeRenderCommitForTest(item, pendingRenderGenerationForTest(item),
        activeRequestIdForTest(item), pendingRenderPayloadIdForTest(item));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    acknowledgeRenderFailureForTest(item, generation, requestId, payloadId + 1);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgeRenderCommitForTest(item, pendingRenderGenerationForTest(item),
        activeRequestIdForTest(item), pendingRenderPayloadIdForTest(item));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    acknowledgeRenderFailureForTest(item, generation, requestId, payloadId);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(4.0, 2.0));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("render commit failed")));
    QVERIFY(!hasPendingRenderCommitForTest(item));
}

void ImageViewportRenderCommitTest::activeRenderFailureDiagnosticsPreserveCause_data()
{
    QTest::addColumn<RenderFailureCause>("cause");

    QTest::newRow("texture-creation") << RenderFailureCause::TextureCreationFailure;
    QTest::newRow("image-node-creation") << RenderFailureCause::ImageNodeCreationFailure;
    QTest::newRow("invalid-role-payload") << RenderFailureCause::InvalidRolePayload;
    QTest::newRow("invalid-render-geometry") << RenderFailureCause::InvalidRenderGeometry;
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    QVERIFY(generation > 0);
    QVERIFY(requestId > 0);
    QVERIFY(payloadId > 0);

    acknowledgeRenderFailureForTest(
        item, ImageViewportPageRole::Primary, generation, requestId, payloadId, cause);

    const RenderFailureDiagnosticForTest diagnostic
        = lastAcceptedRenderFailureDiagnosticForTest(item);
    QVERIFY(diagnostic.valid);
    QCOMPARE(diagnostic.role, ImageViewportPageRole::Primary);
    QCOMPARE(diagnostic.generation, generation);
    QCOMPARE(diagnostic.requestId, requestId);
    QCOMPARE(diagnostic.preparedPayloadId, payloadId);
    QCOMPARE(diagnostic.cause, cause);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("render commit failed")));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);
    acknowledgeRenderFailureForTest(item, ImageViewportPageRole::Primary, generation, requestId,
        payloadId, RenderFailureCause::TextureCreationFailure);

    const RenderFailureDiagnosticForTest activeDiagnostic
        = lastAcceptedRenderFailureDiagnosticForTest(item);
    QVERIFY(activeDiagnostic.valid);
    QCOMPARE(activeDiagnostic.cause, RenderFailureCause::TextureCreationFailure);

    acknowledgeRenderFailureForTest(item, ImageViewportPageRole::Primary, generation, requestId,
        payloadId + 1, RenderFailureCause::ImageNodeCreationFailure);

    const RenderFailureDiagnosticForTest staleDiagnostic
        = lastAcceptedRenderFailureDiagnosticForTest(item);
    QVERIFY(staleDiagnostic.valid);
    QCOMPARE(staleDiagnostic.role, activeDiagnostic.role);
    QCOMPARE(staleDiagnostic.generation, activeDiagnostic.generation);
    QCOMPARE(staleDiagnostic.requestId, activeDiagnostic.requestId);
    QCOMPARE(staleDiagnostic.preparedPayloadId, activeDiagnostic.preparedPayloadId);
    QCOMPARE(staleDiagnostic.cause, activeDiagnostic.cause);

    const auto observations = internalObservationsForTest(item);
    QVERIFY(!observations.isEmpty());
    const InternalObservationForTest stale = observations.constLast();
    QCOMPARE(stale.subsystem, InternalObservationSubsystemForTest::Engine);
    QCOMPARE(stale.category, InternalObservationCategoryForTest::StaleDrop);
    QCOMPARE(stale.cause, InternalObservationCauseForTest::StaleRenderAcknowledgement);
    QVERIFY(stale.identity.roleValid);
    QCOMPARE(stale.identity.role, ImageViewportPageRole::Primary);
    QCOMPARE(stale.identity.generation, generation);
    QCOMPARE(stale.identity.requestId, requestId);
    QCOMPARE(stale.identity.payloadId, payloadId + 1);
    QVERIFY(stale.identity.renderAttempt > 0);
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
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
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    advancePlaybackForTest(item, 100);
    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QVERIFY(hasPendingRenderCommitForTest(item));

    const quint64 generation = pendingRenderGenerationForTest(item);
    const quint64 requestId = activeRequestIdForTest(item);
    const quint64 payloadId = pendingRenderPayloadIdForTest(item);

    acknowledgeRenderCommitForTest(item, generation, requestId, payloadId);

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);
    QCOMPARE(displayedRequestIdForTest(item), requestId);
    QVERIFY(!hasPendingRenderCommitForTest(item));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();
    acknowledgePendingPrimaryRenderCommitForTest(item);
    const quint64 initialRequestId = activeRequestIdForTest(item);

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    item.setSize(QSizeF(0.0, 20.0));
    advancePlaybackForTest(item, 100);

    const quint64 waitingRequestId = activeRequestIdForTest(item);
    QVERIFY(waitingRequestId > initialRequestId);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);

    item.setSize(QSizeF(40.0, 20.0));

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(activeRequestIdForTest(item), waitingRequestId);
    QVERIFY(hasPendingRenderCommitForTest(item));
    QCOMPARE(pendingRenderGenerationForTest(item), 1U);
    QVERIFY(pendingRenderPayloadIdForTest(item) > 0);

    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);
}

QTEST_MAIN(ImageViewportRenderCommitTest)

#include "tst_imageviewport_render_commit.moc"
