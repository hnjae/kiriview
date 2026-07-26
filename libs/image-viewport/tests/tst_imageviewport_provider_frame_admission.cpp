// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_paint_test_support.h"
#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

namespace {
std::unique_ptr<ImageFrame> providerDetailFrame(
    QSize rasterSize, ImageViewportPayloadQuality quality, QColor color = Qt::transparent)
{
    const QSizeF logicalSize(16.0, 8.0);
    QImage image(rasterSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(color);
    return std::make_unique<ImageFrame>(image, logicalSize, QSizeF(rasterSize),
        QSizeF(
            rasterSize.width() / logicalSize.width(), rasterSize.height() / logicalSize.height()),
        image.sizeInBytes(), quality,
        quality == ImageViewportPayloadQuality::Exact
            ? ImageViewportPayloadExactness::ExactForSource
            : ImageViewportPayloadExactness::NotExact,
        image.hasAlphaChannel(), ImageFrame::OrientationPolicy::Identity, QString {});
}

struct PendingProvisionalReplacementFixture
{
    PendingProvisionalReplacementFixture()
        : sessionFactory(std::make_shared<CountingProviderSessionFactory>(sessionCount,
              metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount))
        , adapter(sessionFactory)
        , result(factory.fromProvider(&adapter))
    {
        if (!result || !result->sequence()) {
            return;
        }
        useSynchronousProviderEventDeliveryForTest(item);
        item.setSize(QSizeF(100.0, 100.0));
        item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});

        CountingProviderSession* session = sessionFactory->lastSession();
        if (!session) {
            return;
        }
        emitProviderMetadataReady(session, session->lastMetadataToken(),
            ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
        token = session->lastFrameToken();
        preview = providerDetailFrame(QSize(8, 4), ImageViewportPayloadQuality::Preview, Qt::red);
        emitProviderProvisionalFrameReady(session, token, preview.get());
        generation = pendingRenderGenerationForTest(item);
        requestId = activeRequestIdForTest(item);
        provisionalPayloadId = pendingRenderPayloadIdForTest(item);

        authoritative
            = providerDetailFrame(QSize(16, 8), ImageViewportPayloadQuality::Exact, Qt::blue);
        emitProviderFrameReady(session, token, authoritative.get());
        authoritativePayloadId = pendingRenderPayloadIdForTest(item);
    }

    [[nodiscard]] bool initialized() const
    {
        return result && result->sequence() && sessionFactory->lastSession()
            && *frameRequestCount == 1 && token.isValid() && generation != 0 && requestId != 0
            && provisionalPayloadId != 0 && authoritativePayloadId != 0;
    }

    std::shared_ptr<int> sessionCount = std::make_shared<int>(0);
    std::shared_ptr<int> metadataRequestCount = std::make_shared<int>(0);
    std::shared_ptr<int> frameRequestCount = std::make_shared<int>(0);
    std::shared_ptr<int> lastRequestedFrame = std::make_shared<int>(-1);
    std::shared_ptr<int> closeCount = std::make_shared<int>(0);
    std::shared_ptr<CountingProviderSessionFactory> sessionFactory;
    CountingProviderAdapter adapter;
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result;
    std::unique_ptr<ImageFrame> preview;
    std::unique_ptr<ImageFrame> authoritative;
    ImageSequenceProviderRequestToken token;
    quint64 generation = 0;
    quint64 requestId = 0;
    quint64 provisionalPayloadId = 0;
    quint64 authoritativePayloadId = 0;
    ImageViewport item;
};
}

class ImageViewportProviderFrameAdmissionTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderFrameAdmissionTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:
    void providerStillFrameReadyCommitsDisplay();
    void providerProvisionalFrameRemainsLoadingUntilAuthoritativeCommit();
    void providerAuthoritativeFrameRejectsDelayedProvisionalCommit();
    void providerAuthoritativeFrameRejectsDelayedProvisionalFailure();
    void providerTerminalFailureClearsProvisionalDisplay();
    void providerSpreadDoesNotExposePartialProvisionalRoles();
    void providerStillFrameUsesDeviceIndependentPayloadSize();
    void providerOrientedFramePayloadCommitsNormalizedLogicalSize();
    void providerTimedFrameReadyCommitsTimedDisplay();
    void providerFrameRejectsLogicalSizeMismatch();
    void providerFrameWithoutDemandRevisionRejectsPayload();
    void providerTimedFrameEnvelopeMismatchRejectsPayload();
    void providerTotalDurationSeekRejectsPublicPositionEnvelope();
    void providerFrameEnvelopeMismatchKeepsGenerationPositionSeekable();
    void providerStillFrameEnvelopeMismatchRejectsPayload();
    void providerTimedFrameRejectsStillEnvelope();
    void providerTimedFrameDurationMismatchRejectsPayload();
    void providerTimedFramePayloadLimitReportsUnsupportedPayload();
    void providerPayloadLimitKeepsGenerationFrameSeekable();
    void providerFrameRejectsInvalidPayloadByteSize();
    void providerRejectedOwnedFramePayloadReleasesOnce();
    void providerStaleOwnedFramePayloadReleasesOnce();
    void providerClosedGenerationOwnedFramePayloadReleasesOnce();
    void providerAcceptedOwnedFramePayloadReleasesOnce();
    void secondaryProviderAcceptedOwnedFramePayloadCompletesSpreadAndReleasesOnce();
    void providerRetainedOwnedFramePayloadOutlivesClosingSessionUntilReplacementCommit();
    void providerResourcePressureDiscardsOnlyRetainedOwnedFramePayload();
    void providerFrameReadyWithoutWindowPublishesRenderWaiting();
    void providerFrameReadyWithZeroGeometryKeepsRenderWaiting();
};

void ImageViewportProviderFrameAdmissionTest::providerStillFrameReadyCommitsDisplay()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QCOMPARE(*frameRequestCount, 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken(), &frame);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
}

void ImageViewportProviderFrameAdmissionTest::
    providerProvisionalFrameRemainsLoadingUntilAuthoritativeCommit()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QCOMPARE(*frameRequestCount, 1);
    const ImageSequenceProviderRequestToken token = sessionFactory->lastSession()->lastFrameToken();

    const std::unique_ptr<ImageFrame> preview
        = providerDetailFrame(QSize(8, 4), ImageViewportPayloadQuality::Preview, Qt::red);
    QVERIFY(preview->isValid());
    emitProviderProvisionalFrameReady(sessionFactory->lastSession(), token, preview.get());

    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Loading);
    QCOMPARE(requestReason(item), ImageViewportRequestReason::RenderWaiting);
    QCOMPARE(displayStatus(item), ImageViewportDisplayStatus::Empty);
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Loading);
    QCOMPARE(requestReason(item), ImageViewportRequestReason::ProviderWaiting);
    QCOMPARE(displayStatus(item), ImageViewportDisplayStatus::Ready);
    QCOMPARE(item.state().primary().display().quality(), ImageViewportPayloadQuality::Preview);
    QCOMPARE(item.state().primary().display().exactness(), ImageViewportPayloadExactness::NotExact);
    QCOMPARE(item.state().primary().display().payloadRasterSize(), QSizeF(8.0, 4.0));
    QCOMPARE(item.state().primary().display().currentForDemand(), false);
    QCOMPARE(*frameRequestCount, 1);

    const std::unique_ptr<ImageFrame> authoritative
        = providerDetailFrame(QSize(16, 8), ImageViewportPayloadQuality::Exact, Qt::blue);
    QVERIFY(authoritative->isValid());
    emitProviderFrameReady(sessionFactory->lastSession(), token, authoritative.get());

    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Loading);
    QCOMPARE(requestReason(item), ImageViewportRequestReason::RenderWaiting);
    QCOMPARE(displayStatus(item), ImageViewportDisplayStatus::Ready);
    QCOMPARE(item.state().primary().display().quality(), ImageViewportPayloadQuality::Preview);
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Ready);
    QCOMPARE(requestReason(item), ImageViewportRequestReason::Ready);
    QCOMPARE(displayStatus(item), ImageViewportDisplayStatus::Ready);
    QCOMPARE(item.state().primary().display().quality(), ImageViewportPayloadQuality::Exact);
    QCOMPARE(item.state().primary().display().exactness(),
        ImageViewportPayloadExactness::ExactForSource);
    QCOMPARE(item.state().primary().display().payloadRasterSize(), QSizeF(16.0, 8.0));
    QCOMPARE(item.state().primary().display().currentForDemand(), true);
    QCOMPARE(*frameRequestCount, 1);
}

void ImageViewportProviderFrameAdmissionTest::
    providerAuthoritativeFrameRejectsDelayedProvisionalCommit()
{
    PendingProvisionalReplacementFixture fixture;
    QVERIFY(fixture.initialized());
    QVERIFY(fixture.authoritativePayloadId != fixture.provisionalPayloadId);

    acknowledgeRenderCommitForTest(
        fixture.item, fixture.generation, fixture.requestId, fixture.provisionalPayloadId);

    QCOMPARE(requestStatus(fixture.item), ImageViewportRequestStatus::Loading);
    QCOMPARE(requestReason(fixture.item), ImageViewportRequestReason::RenderWaiting);
    QCOMPARE(displayStatus(fixture.item), ImageViewportDisplayStatus::Empty);
    QVERIFY(hasPendingRenderCommitForTest(fixture.item));
    QCOMPARE(pendingRenderPayloadIdForTest(fixture.item), fixture.authoritativePayloadId);

    acknowledgeRenderCommitForTest(
        fixture.item, fixture.generation, fixture.requestId, fixture.authoritativePayloadId);

    QCOMPARE(requestStatus(fixture.item), ImageViewportRequestStatus::Ready);
    QCOMPARE(requestReason(fixture.item), ImageViewportRequestReason::Ready);
    QCOMPARE(displayStatus(fixture.item), ImageViewportDisplayStatus::Ready);
    QCOMPARE(
        fixture.item.state().primary().display().quality(), ImageViewportPayloadQuality::Exact);
    QCOMPARE(fixture.item.state().primary().display().payloadRasterSize(), QSizeF(16.0, 8.0));
}

void ImageViewportProviderFrameAdmissionTest::
    providerAuthoritativeFrameRejectsDelayedProvisionalFailure()
{
    PendingProvisionalReplacementFixture fixture;
    QVERIFY(fixture.initialized());
    QVERIFY(fixture.authoritativePayloadId != fixture.provisionalPayloadId);

    acknowledgeRenderFailureForTest(
        fixture.item, fixture.generation, fixture.requestId, fixture.provisionalPayloadId);

    QCOMPARE(requestStatus(fixture.item), ImageViewportRequestStatus::Loading);
    QCOMPARE(requestReason(fixture.item), ImageViewportRequestReason::RenderWaiting);
    QCOMPARE(displayStatus(fixture.item), ImageViewportDisplayStatus::Empty);
    QVERIFY(hasPendingRenderCommitForTest(fixture.item));
    QCOMPARE(pendingRenderPayloadIdForTest(fixture.item), fixture.authoritativePayloadId);

    acknowledgeRenderCommitForTest(
        fixture.item, fixture.generation, fixture.requestId, fixture.authoritativePayloadId);

    QCOMPARE(requestStatus(fixture.item), ImageViewportRequestStatus::Ready);
    QCOMPARE(requestReason(fixture.item), ImageViewportRequestReason::Ready);
    QCOMPARE(displayStatus(fixture.item), ImageViewportDisplayStatus::Ready);
    QCOMPARE(
        fixture.item.state().primary().display().quality(), ImageViewportPayloadQuality::Exact);
    QCOMPARE(fixture.item.state().primary().display().payloadRasterSize(), QSizeF(16.0, 8.0));
}

void ImageViewportProviderFrameAdmissionTest::providerTerminalFailureClearsProvisionalDisplay()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    const ImageSequenceProviderRequestToken token = sessionFactory->lastSession()->lastFrameToken();

    const auto releaseCount = std::make_shared<int>(0);
    auto* previewHandle = new ImageSequenceProviderFrameHandle(
        providerDetailFrame(QSize(8, 4), ImageViewportPayloadQuality::Preview, Qt::red).release(),
        [releaseCount](ImageFrame* frame) {
            ++*releaseCount;
            delete frame;
        });
    QVERIFY(previewHandle->frame()->isValid());
    emitProviderProvisionalFrameHandleReady(sessionFactory->lastSession(), token, previewHandle);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(displayStatus(item), ImageViewportDisplayStatus::Ready);
    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Loading);
    QCOMPARE(*releaseCount, 0);

    emitProviderFailed(
        sessionFactory->lastSession(), token, QStringLiteral("authoritative decode failed"));

    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Error);
    QCOMPARE(requestReason(item), ImageViewportRequestReason::ProviderFailure);
    QCOMPARE(displayStatus(item), ImageViewportDisplayStatus::Empty);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(item.state().primary().display().belongsToAcceptedPresentationTarget(), false);
    QCOMPARE(item.state().primary().display().retained(), false);
    QCOMPARE(item.state().primary().display().quality(), ImageViewportPayloadQuality::Unknown);
    QCOMPARE(*releaseCount, 1);
    QVERIFY(!hasPendingRenderCommitForTest(item));
}

void ImageViewportProviderFrameAdmissionTest::providerSpreadDoesNotExposePartialProvisionalRoles()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    const auto primaryFrameRequestCount = std::make_shared<int>(0);
    const auto primaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    auto primarySessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, primaryFrameRequestCount, primaryLastRequestedFrame, closeCount);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, secondaryFrameRequestCount, secondaryLastRequestedFrame, closeCount);
    CountingProviderAdapter primaryAdapter(primarySessionFactory);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> primary(factory.fromProvider(&primaryAdapter));
    QScopedPointer<ImageSequenceFactoryResult> secondary(factory.fromProvider(&secondaryAdapter));
    QVERIFY(primary->sequence());
    QVERIFY(secondary->sequence());

    ImageViewport item;
    useSynchronousProviderEventDeliveryForTest(item);
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(
                     ImageViewportPresentationTarget(primary->sequence(), secondary->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);

    QVERIFY(primarySessionFactory->lastSession());
    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(primarySessionFactory->lastSession(),
        primarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QCOMPARE(*primaryFrameRequestCount, 1);
    QCOMPARE(*secondaryFrameRequestCount, 1);
    const ImageSequenceProviderRequestToken primaryToken
        = primarySessionFactory->lastSession()->lastFrameToken();
    const ImageSequenceProviderRequestToken secondaryToken
        = secondarySessionFactory->lastSession()->lastFrameToken();

    const std::unique_ptr<ImageFrame> primaryPreview
        = providerDetailFrame(QSize(8, 4), ImageViewportPayloadQuality::Preview, Qt::red);
    const std::unique_ptr<ImageFrame> secondaryPreview
        = providerDetailFrame(QSize(8, 4), ImageViewportPayloadQuality::Preview, Qt::green);
    emitProviderProvisionalFrameReady(
        primarySessionFactory->lastSession(), primaryToken, primaryPreview.get());

    QCOMPARE(displayStatus(item), ImageViewportDisplayStatus::Empty);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(secondaryDisplayedFrame(item), -1);
    QVERIFY(!hasPendingRenderCommitForTest(item));

    emitProviderProvisionalFrameReady(
        secondarySessionFactory->lastSession(), secondaryToken, secondaryPreview.get());
    QVERIFY(hasPendingRenderCommitForTest(item));
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Loading);
    QCOMPARE(requestReason(item), ImageViewportRequestReason::ProviderWaiting);
    QCOMPARE(displayStatus(item), ImageViewportDisplayStatus::Ready);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(secondaryDisplayedFrame(item), 0);
    QCOMPARE(item.state().primary().display().quality(), ImageViewportPayloadQuality::Preview);
    QCOMPARE(item.state().secondary().display().quality(), ImageViewportPayloadQuality::Preview);

    const std::unique_ptr<ImageFrame> primaryAuthoritative
        = providerDetailFrame(QSize(16, 8), ImageViewportPayloadQuality::Exact, Qt::blue);
    emitProviderFrameReady(
        primarySessionFactory->lastSession(), primaryToken, primaryAuthoritative.get());
    QVERIFY(hasPendingRenderCommitForTest(item));
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Loading);
    QCOMPARE(requestReason(item), ImageViewportRequestReason::ProviderWaiting);
    QCOMPARE(displayStatus(item), ImageViewportDisplayStatus::Ready);
    QCOMPARE(item.state().primary().display().quality(), ImageViewportPayloadQuality::Exact);
    QCOMPARE(item.state().secondary().display().quality(), ImageViewportPayloadQuality::Preview);
    QCOMPARE(item.state().primary().display().currentForDemand(), true);
    QCOMPARE(item.state().secondary().display().currentForDemand(), false);

    const std::unique_ptr<ImageFrame> secondaryAuthoritative
        = providerDetailFrame(QSize(16, 8), ImageViewportPayloadQuality::Exact, Qt::yellow);
    emitProviderFrameReady(
        secondarySessionFactory->lastSession(), secondaryToken, secondaryAuthoritative.get());
    QVERIFY(hasPendingRenderCommitForTest(item));
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatus(item), ImageViewportRequestStatus::Ready);
    QCOMPARE(requestReason(item), ImageViewportRequestReason::Ready);
    QCOMPARE(displayStatus(item), ImageViewportDisplayStatus::Ready);
    QCOMPARE(item.state().primary().display().quality(), ImageViewportPayloadQuality::Exact);
    QCOMPARE(item.state().secondary().display().quality(), ImageViewportPayloadQuality::Exact);
    QCOMPARE(item.state().primary().display().currentForDemand(), true);
    QCOMPARE(item.state().secondary().display().currentForDemand(), true);
    QCOMPARE(*primaryFrameRequestCount, 1);
    QCOMPARE(*secondaryFrameRequestCount, 1);
}

void ImageViewportProviderFrameAdmissionTest::providerStillFrameUsesDeviceIndependentPayloadSize()
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

    QQuickWindow window;
    window.resize(20, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(20.0, 20.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(2.0, 1.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(2.0);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(displayedImageSize(item), QSizeF(2.0, 1.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 5.0, 20.0, 10.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 0.0, 2.0, 1.0));

    auto* imageNode = dynamic_cast<QSGImageNode*>(root->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(imageNode->sourceRect(), QRectF(0.0, 0.0, 4.0, 2.0));
}

void ImageViewportProviderFrameAdmissionTest::
    providerOrientedFramePayloadCommitsNormalizedLogicalSize()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(2.0, 3.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    QImage image(3, 2, QImage::Format_ARGB32_Premultiplied);
    image.setPixelColor(0, 0, QColor(255, 0, 0, 255));
    image.setPixelColor(1, 0, QColor(0, 255, 0, 255));
    image.setPixelColor(2, 0, QColor(0, 0, 255, 255));
    image.setPixelColor(0, 1, QColor(255, 255, 0, 255));
    image.setPixelColor(1, 1, QColor(0, 255, 255, 255));
    image.setPixelColor(2, 1, QColor(255, 0, 255, 255));
    ImageFrame frame(image, ImageFrame::OrientationPolicy::Rotate90);
    emitProviderFrameReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayedImageSize(item), QSizeF(2.0, 3.0));

    const QImage normalized = imageForTest(frame);
    QCOMPARE(normalized.size(), QSize(2, 3));
    QCOMPARE(
        normalized.pixelColor(0, 0), image.transformed(QTransform().rotate(90)).pixelColor(0, 0));
}

void ImageViewportProviderFrameAdmissionTest::providerTimedFrameReadyCommitsTimedDisplay()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));
    QCOMPARE(contentRect(item), QRectF());

    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
}

void ImageViewportProviderFrameAdmissionTest::providerFrameRejectsLogicalSizeMismatch()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    QImage image(8, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(
        viewportErrorString(item).contains(QStringLiteral("provider frame logical size mismatch")));
    const auto observations = internalObservationsForTest(item);
    QVERIFY(!observations.isEmpty());
    const InternalObservationForTest admission = observations.constLast();
    QCOMPARE(admission.subsystem, InternalObservationSubsystemForTest::Preparation);
    QCOMPARE(admission.category, InternalObservationCategoryForTest::AdmissionFailure);
    QCOMPARE(admission.cause, InternalObservationCauseForTest::ProviderFrameRejected);
    QVERIFY(admission.identity.roleValid);
    QCOMPARE(admission.identity.role, ImageViewportPageRole::Primary);
    QVERIFY(admission.identity.generation > 0);
    QVERIFY(admission.identity.requestId > 0);
    QVERIFY(admission.identity.providerToken > 0);
}

void ImageViewportProviderFrameAdmissionTest::providerFrameWithoutDemandRevisionRejectsPayload()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QCOMPARE(*frameRequestCount, 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(
        static_cast<ImageSequenceProviderSession*>(sessionFactory->lastSession()),
        sessionFactory->lastSession()->lastFrameToken(), &frame,
        ImageSequenceProviderFrameEnvelope::stillFrame());

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(!hasPendingRenderCommitForTest(item));
    const auto observations = internalObservationsForTest(item);
    QVERIFY(!observations.isEmpty());
    const InternalObservationForTest admission = observations.constLast();
    QCOMPARE(admission.subsystem, InternalObservationSubsystemForTest::Preparation);
    QCOMPARE(admission.category, InternalObservationCategoryForTest::AdmissionFailure);
    QCOMPARE(admission.cause, InternalObservationCauseForTest::ProviderFrameRejected);
}

void ImageViewportProviderFrameAdmissionTest::providerTimedFrameEnvelopeMismatchRejectsPayload()
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
    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    emitTimedProviderFrameReady(sessionFactory->lastSession(), frameToken, &frame, 1, 100);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(viewportErrorString(item).contains(
        QStringLiteral("provider frame resolved frame mismatch")));

    emitTimedProviderFrameReady(sessionFactory->lastSession(), frameToken, &frame, 0, 0);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
}

void ImageViewportProviderFrameAdmissionTest::
    providerTotalDurationSeekRejectsPublicPositionEnvelope()
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
        std::shared_ptr<int>(), std::shared_ptr<ImageSequenceProviderRequestToken>(),
        positionRequestCount, lastPositionFrame, lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 350).outcome(),
        ImageViewportCommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RequestQueued"));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 350);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 350);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastPositionToken();
    emitProviderFrameReady(sessionFactory->lastSession(), frameToken, &frame,
        providerTimedFrameEnvelope(sessionFactory->lastSession()->lastFrameDemand(), 1, 350, 250));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 350);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QVERIFY(viewportErrorString(item).contains(
        QStringLiteral("provider frame start position mismatch")));

    emitTimedProviderFrameReady(sessionFactory->lastSession(), frameToken, &frame, 1, 100);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
}

void ImageViewportProviderFrameAdmissionTest::
    providerFrameEnvelopeMismatchKeepsGenerationPositionSeekable()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QVERIFY(viewportErrorString(item).contains(
        QStringLiteral("provider frame resolved frame mismatch")));

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 350).outcome(),
        ImageViewportCommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 350);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderFrameAdmissionTest::providerStillFrameEnvelopeMismatchRejectsPayload()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastFrameToken(), &frame,
        providerTimedFrameEnvelope(sessionFactory->lastSession()->lastFrameDemand(), 0, 0, -1));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(
        viewportErrorString(item).contains(QStringLiteral("provider frame metadata is invalid")));
}

void ImageViewportProviderFrameAdmissionTest::providerTimedFrameRejectsStillEnvelope()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastFrameToken(), &frame,
        providerStillFrameEnvelope(sessionFactory->lastSession()->lastFrameDemand()));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(
        viewportErrorString(item).contains(QStringLiteral("provider frame metadata is invalid")));
}

void ImageViewportProviderFrameAdmissionTest::providerTimedFrameDurationMismatchRejectsPayload()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastFrameToken(), &frame,
        providerTimedFrameEnvelope(sessionFactory->lastSession()->lastFrameDemand(), 0, 0, 250));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider frame duration mismatch")));
}

void ImageViewportProviderFrameAdmissionTest::
    providerTimedFramePayloadLimitReportsUnsupportedPayload()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    uchar pixel = 0;
    const qsizetype excessiveStride = ImageSequenceLimits::maximumPayloadBytes() / 8 + 1;
    QImage image(&pixel, 16, 8, excessiveStride, QImage::Format_ARGB32_Premultiplied);
    QVERIFY(image.sizeInBytes() > ImageSequenceLimits::maximumPayloadBytes());
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(viewportErrorString(item).contains(
        QStringLiteral("provider frame payload exceeds maximumPayloadBytes")));
}

void ImageViewportProviderFrameAdmissionTest::providerPayloadLimitKeepsGenerationFrameSeekable()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    uchar pixel = 0;
    const qsizetype excessiveStride = ImageSequenceLimits::maximumPayloadBytes() / 8 + 1;
    QImage excessiveImage(&pixel, 16, 8, excessiveStride, QImage::Format_ARGB32_Premultiplied);
    QVERIFY(excessiveImage.sizeInBytes() > ImageSequenceLimits::maximumPayloadBytes());
    ImageFrame excessiveFrame(excessiveImage);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &excessiveFrame, 0, 0);

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QVERIFY(viewportErrorString(item).contains(
        QStringLiteral("provider frame payload exceeds maximumPayloadBytes")));

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(viewportErrorString(item), QString());

    QImage validImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    validImage.fill(Qt::transparent);
    ImageFrame validFrame(validImage);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &validFrame, 1, 100);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedPosition(item), 100);
}

void ImageViewportProviderFrameAdmissionTest::providerFrameRejectsInvalidPayloadByteSize()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const std::unique_ptr<ImageFrame> frame = makeImageFrameWithPayloadByteSizeForTest(image, -1);
    emitProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastFrameToken(), frame.get());
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(viewportErrorString(item).contains(
        QStringLiteral("provider frame payload byte size is invalid")));
}

void ImageViewportProviderFrameAdmissionTest::providerRejectedOwnedFramePayloadReleasesOnce()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    const auto releaseCount = std::make_shared<int>(0);
    auto* payload
        = new ImageSequenceProviderFrameHandle(new ImageFrame, [releaseCount](ImageFrame* frame) {
              ++*releaseCount;
              delete frame;
          });
    emitProviderFrameHandleReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken(), payload);
    drainQueuedProviderResults();

    QCOMPARE(*releaseCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QVERIFY(
        viewportErrorString(item).contains(QStringLiteral("provider frame payload is invalid")));
}

void ImageViewportProviderFrameAdmissionTest::providerStaleOwnedFramePayloadReleasesOnce()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken staleToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Accepted);
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 2);
    QVERIFY(staleToken != sessionFactory->lastSession()->lastFrameToken());

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const auto releaseCount = std::make_shared<int>(0);
    auto* payload = new ImageSequenceProviderFrameHandle(
        new ImageFrame(image), [releaseCount](ImageFrame* frame) {
            ++*releaseCount;
            delete frame;
        });
    emitProviderFrameHandleReady(sessionFactory->lastSession(), staleToken, payload);
    drainQueuedProviderResults();

    QCOMPARE(*releaseCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderFrameAdmissionTest::
    providerClosedGenerationOwnedFramePayloadReleasesOnce()
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

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const auto releaseCount = std::make_shared<int>(0);
    auto* payload = new ImageSequenceProviderFrameHandle(
        new ImageFrame(image), [releaseCount](ImageFrame* frame) {
            ++*releaseCount;
            delete frame;
        });
    emitProviderFrameHandleReady(sessionFactory->lastSession(), frameToken, payload);

    QCOMPARE(item.clear().outcome(), ImageViewportCommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*releaseCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderFrameAdmissionTest::providerAcceptedOwnedFramePayloadReleasesOnce()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const auto releaseCount = std::make_shared<int>(0);
    auto* payload = new ImageSequenceProviderFrameHandle(
        new ImageFrame(image), [releaseCount](ImageFrame* frame) {
            ++*releaseCount;
            delete frame;
        });
    emitProviderFrameHandleReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken(), payload);
    drainQueuedProviderResults();

    QCOMPARE(*releaseCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));

    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(*releaseCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));

    QCOMPARE(item.clear().outcome(), ImageViewportCommandOutcome::Accepted);
    drainQueuedProviderResults();
    QCOMPARE(*releaseCount, 1);
}

void ImageViewportProviderFrameAdmissionTest::
    secondaryProviderAcceptedOwnedFramePayloadCompletesSpreadAndReleasesOnce()
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
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(20.0, 10.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);

    QImage secondaryImage(20, 10, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::black);
    const auto releaseCount = std::make_shared<int>(0);
    auto* payload = new ImageSequenceProviderFrameHandle(
        new ImageFrame(secondaryImage), [releaseCount](ImageFrame* frame) {
            ++*releaseCount;
            delete frame;
        });
    emitProviderFrameHandleReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken(), payload);
    drainQueuedProviderResults();

    QCOMPARE(*releaseCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(*releaseCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(secondaryDisplayedFrame(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(secondaryDisplayedImageSize(item), QSizeF(20.0, 10.0));

    QCOMPARE(item.clear().outcome(), ImageViewportCommandOutcome::Accepted);
    drainQueuedProviderResults();
    QCOMPARE(*releaseCount, 1);
}

void ImageViewportProviderFrameAdmissionTest::
    providerRetainedOwnedFramePayloadOutlivesClosingSessionUntilReplacementCommit()
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
    QScopedPointer<ImageSequenceFactoryResult> providerResult(factory.fromProvider(&adapter));
    QVERIFY(providerResult->sequence());

    QImage replacementImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::black);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(providerResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    CountingProviderSession* session = sessionFactory->lastSession();
    QVERIFY(session);
    QPointer<CountingProviderSession> sessionGuard(session);
    emitProviderMetadataReady(session, session->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const auto releaseCount = std::make_shared<int>(0);
    auto* payload = new ImageSequenceProviderFrameHandle(
        new ImageFrame(image), [releaseCount](ImageFrame* frame) {
            ++*releaseCount;
            delete frame;
        });
    emitProviderFrameHandleReady(session, session->lastFrameToken(), payload);
    drainQueuedProviderResults();
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(*releaseCount, 0);

    QCOMPARE(
        item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
                PresentationTargetTransitionPolicy {})
            .outcome(),
        ImageViewportCommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(*closeCount, 1);
    QCOMPARE(*releaseCount, 0);
    QVERIFY(sessionGuard);

    acknowledgePendingRenderCommitForTest(item);
    drainQueuedProviderResults();

    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(*releaseCount, 1);
    QVERIFY(!sessionGuard);
}

void ImageViewportProviderFrameAdmissionTest::
    providerResourcePressureDiscardsOnlyRetainedOwnedFramePayload()
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
    QScopedPointer<ImageSequenceFactoryResult> providerResult(factory.fromProvider(&adapter));
    QVERIFY(providerResult->sequence());

    QImage replacementImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::black);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(providerResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);

    CountingProviderSession* session = sessionFactory->lastSession();
    QVERIFY(session);
    QPointer<CountingProviderSession> sessionGuard(session);
    emitProviderMetadataReady(session, session->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const auto releaseCount = std::make_shared<int>(0);
    auto* payload = new ImageSequenceProviderFrameHandle(
        new ImageFrame(image), [releaseCount](ImageFrame* frame) {
            ++*releaseCount;
            delete frame;
        });
    emitProviderFrameHandleReady(session, session->lastFrameToken(), payload);
    drainQueuedProviderResults();
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(*releaseCount, 0);

    QCOMPARE(
        item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
                PresentationTargetTransitionPolicy {})
            .outcome(),
        ImageViewportCommandOutcome::Accepted);
    drainQueuedProviderResults();
    QCOMPARE(item.state().display().status(), ImageViewportDisplayStatus::Retained);
    QVERIFY(hasPendingRenderCommitForTest(item));

    const ImageViewportStateSnapshot beforePressure = item.state();
    discardRetainedDisplayForResourcePressureForTest(item);
    drainQueuedProviderResults();
    const ImageViewportStateSnapshot afterPressure = item.state();

    QCOMPARE(afterPressure.display().status(), ImageViewportDisplayStatus::Empty);
    QCOMPARE(afterPressure.display().phase(), ImageViewportDisplayPhase::TransitioningPlaceholder);
    QCOMPARE(afterPressure.request().status(), beforePressure.request().status());
    QCOMPARE(afterPressure.request().reason(), beforePressure.request().reason());
    QCOMPARE(afterPressure.request().acceptedPresentationTargetGeneration(),
        beforePressure.request().acceptedPresentationTargetGeneration());
    QCOMPARE(afterPressure.revisions().request(), beforePressure.revisions().request());
    QCOMPARE(afterPressure.revisions().presentation(), beforePressure.revisions().presentation());
    QVERIFY(afterPressure.revisions().display() != beforePressure.revisions().display());
    QVERIFY(afterPressure.revisions().snapshot() != beforePressure.revisions().snapshot());
    QVERIFY(hasPendingRenderCommitForTest(item));
    QCOMPARE(*releaseCount, 1);
    QVERIFY(!sessionGuard);

    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(item.state().display().status(), ImageViewportDisplayStatus::Ready);
    const ImageViewportStateSnapshot ready = item.state();
    discardRetainedDisplayForResourcePressureForTest(item);
    QCOMPARE(item.state().revisions().display(), ready.revisions().display());
    QCOMPARE(item.state().revisions().snapshot(), ready.revisions().snapshot());
}

void ImageViewportProviderFrameAdmissionTest::
    providerFrameReadyWithoutWindowPublishesRenderWaiting()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QVERIFY(hasPendingRenderCommitForTest(item));

    const ImageViewportRevisionToken uploadPendingRevision
        = revisionTokenProperty(item, "requestRevision");
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    verifyRevisionChanged(item, "requestRevision", uploadPendingRevision);
}

void ImageViewportProviderFrameAdmissionTest::providerFrameReadyWithZeroGeometryKeepsRenderWaiting()
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
    item.setSize(QSizeF(0.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    const ImageViewportRevisionToken renderWaitingRevision
        = revisionTokenProperty(item, "requestRevision");
    item.setSize(QSizeF(100.0, 100.0));
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    verifyRevisionChanged(item, "requestRevision", renderWaitingRevision);
}

QTEST_MAIN(ImageViewportProviderFrameAdmissionTest)

#include "tst_imageviewport_provider_frame_admission.moc"
