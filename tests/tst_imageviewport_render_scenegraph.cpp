#include "imageviewport_paint_test_support.h"
#include "imageviewport_provider_test_support.h"
#include "renderadapter_p.h"
#include "renderadapter_scenegraph_p.h"

#include <QtCore/QElapsedTimer>
#include <QtGui/QMatrix4x4>

#include <memory>
#include <utility>

class ImageViewportRenderSceneGraphTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportRenderSceneGraphTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void transparentBackgroundDoesNotCreatePaintNode();
    void solidBackgroundCreatesPaintNode();
    void backgroundOnlyPaintDoesNotAdvanceProviderRequest();
    void checkerboardBackgroundCreatesPaintNode();
    void stillImageCreatesTexturePaintNode();
    void twoPageStillSpreadCreatesRoleTextureNodes();
    void retainedTwoPageSpreadKeepsSecondaryLayerAfterPrimaryReplacement();
    void secondaryProviderFrameCompletesSpreadTextureNodes();
    void primaryAndSecondaryProviderFramesCommitOneSpread();
    void deviceIndependentStillImageUsesPhysicalTextureSourceRect();
    void solidBackgroundRendersBehindImageNode();
    void qualityAndMirroringConfigureTextureNode();
    void rotatedImageTextureNodeUsesTransform_data();
    void rotatedImageTextureNodeUsesTransform();
    void renderAdapterReportsMissingWindowFailureCause();
    void renderAdapterReportsTextureCreationFailureCause();
    void renderAdapterReportsImageNodeCreationFailureCause();
    void renderAdapterReportsInvalidRolePayloadFailureCause();
    void initialRenderFailureWithoutRetainedContentReturnsNullNode();
    void retainedRenderFailureKeepsOldPaintNode();
    void renderPlanBuildsBackgroundPrimitivesWithoutSceneGraph();
    void renderPlanBuildsRoleLayerMappingWithoutSceneGraph();
    void renderPlanReportsPreMaterializationFailureIntent();
    void coverImageTextureNodeUsesVisibleSourceRect();
    void providerStillFrameCreatesTexturePaintNode();
    void providerStillFrameWaitingForGeometryCreatesTexturePaintNode();
    void providerRetainedFrameWaitingForGeometryIgnoresEmptyPaint();
};

namespace {

ImageViewportInternal::PreparedPayload renderAdapterPayload(QImage image)
{
    return { true, 1, 1, 1, std::move(image) };
}

RenderAdapter::Input renderAdapterInputForPayload(
    const ImageViewportInternal::PreparedPayload& payload)
{
    RenderAdapter::Input input;
    input.itemSize = QSizeF(10.0, 10.0);
    input.preparedPayload = payload;
    input.targetRect = QRectF(0.0, 0.0, 10.0, 10.0);
    input.sourceRect = QRectF(0.0, 0.0, 2.0, 2.0);
    return input;
}

ImageViewport::CommandOutcome setPageGapCommand(ImageViewport& item, double gap)
{
    ImageViewportPresentationCommand command;
    command.setPageGap(gap);
    return item.setPresentation(command);
}

ImageViewport::CommandOutcome setFitModeCommand(ImageViewport& item, ImageViewport::FitMode mode)
{
    ImageViewportPresentationCommand command;
    command.setFitMode(mode);
    return item.setPresentation(command);
}

ImageViewport::CommandOutcome setQualityTogglesCommand(
    ImageViewport& item, bool smoothing, bool mipmap)
{
    ImageViewportPresentationCommand command;
    command.setSmoothing(smoothing);
    command.setMipmap(mipmap);
    return item.setPresentation(command);
}

class NullTextureSceneGraphFactory final : public RenderAdapterSceneGraph::Factory
{
public:
    QSGTexture* createTexture(
        QQuickWindow*, const QImage&, QQuickWindow::CreateTextureOptions) const override
    {
        return nullptr;
    }

    QSGImageNode* createImageNode(QQuickWindow* window) const override
    {
        return window->createImageNode();
    }
};

class NullImageNodeSceneGraphFactory final : public RenderAdapterSceneGraph::Factory
{
public:
    QSGTexture* createTexture(
        QQuickWindow* window, const QImage& image,
        QQuickWindow::CreateTextureOptions options) const override
    {
        return window->createTextureFromImage(image, options);
    }

    QSGImageNode* createImageNode(QQuickWindow*) const override { return nullptr; }
};

}

void ImageViewportRenderSceneGraphTest::transparentBackgroundDoesNotCreatePaintNode()
{
    PaintProbeViewport item;
    item.setSize(QSizeF(24.0, 12.0));
    item.setBackgroundMode(ImageViewport::BackgroundMode::Transparent);

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(!root);
}

void ImageViewportRenderSceneGraphTest::solidBackgroundCreatesPaintNode()
{
    PaintProbeViewport item;
    item.setSize(QSizeF(24.0, 12.0));
    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(QColor(20, 40, 60, 255));

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);
    QCOMPARE(root->childCount(), 1);

    auto* background = dynamic_cast<QSGSimpleRectNode*>(root->firstChild());
    QVERIFY(background);
    QCOMPARE(background->rect(), QRectF(0.0, 0.0, 24.0, 12.0));
    QCOMPARE(background->color(), QColor(20, 40, 60, 255));
}

void ImageViewportRenderSceneGraphTest::backgroundOnlyPaintDoesNotAdvanceProviderRequest()
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

    PaintProbeViewport item;
    item.setSize(QSizeF(24.0, 12.0));
    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(QColor(20, 40, 60, 255));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy playbackSpy(&item, &ImageViewport::playbackPhaseChanged);
    QScopedPointer<QSGNode> root(item.takePaintNode());

    QVERIFY(root);
    QCOMPARE(root->childCount(), 1);
    auto* background = dynamic_cast<QSGSimpleRectNode*>(root->firstChild());
    QVERIFY(background);
    QCOMPARE(background->rect(), QRectF(0.0, 0.0, 24.0, 12.0));
    QCOMPARE(background->color(), QColor(20, 40, 60, 255));
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displayStateSpy.count(), 0);
    QCOMPARE(playbackSpy.count(), 0);
}

void ImageViewportRenderSceneGraphTest::checkerboardBackgroundCreatesPaintNode()
{
    PaintProbeViewport item;
    item.setSize(QSizeF(18.0, 10.0));
    item.setBackgroundMode(ImageViewport::BackgroundMode::Checkerboard);

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);
    QCOMPARE(root->childCount(), 6);

    const QList<QRectF> expectedRects = {
        QRectF(0.0, 0.0, 8.0, 8.0),
        QRectF(8.0, 0.0, 8.0, 8.0),
        QRectF(16.0, 0.0, 2.0, 8.0),
        QRectF(0.0, 8.0, 8.0, 2.0),
        QRectF(8.0, 8.0, 8.0, 2.0),
        QRectF(16.0, 8.0, 2.0, 2.0),
    };
    const QList<QColor> expectedColors = {
        QColor(238, 238, 238),
        QColor(204, 204, 204),
        QColor(238, 238, 238),
        QColor(204, 204, 204),
        QColor(238, 238, 238),
        QColor(204, 204, 204),
    };

    QSGNode* child = root->firstChild();
    for (int index = 0; index < expectedRects.size(); ++index) {
        auto* tile = dynamic_cast<QSGSimpleRectNode*>(child);
        QVERIFY(tile);
        QCOMPARE(tile->rect(), expectedRects.at(index));
        QCOMPARE(tile->color(), expectedColors.at(index));
        child = child->nextSibling();
    }
    QVERIFY(!child);
}

void ImageViewportRenderSceneGraphTest::stillImageCreatesTexturePaintNode()
{
    ImageSequenceFactory factory;
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);

    auto* imageNode = dynamic_cast<QSGImageNode*>(root->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(imageNode->rect(), item.property("contentRect").toRectF());
}

void ImageViewportRenderSceneGraphTest::twoPageStillSpreadCreatesRoleTextureNodes()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(QColor(255, 0, 0, 255));
    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 255, 0, 255));
    ImageFrame primaryFrame(primaryImage);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    QQuickWindow window;
    window.resize(88, 44);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(88.0, 44.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);
    QCOMPARE(root->childCount(), 2);

    auto* primaryNode = dynamic_cast<QSGImageNode*>(root->firstChild());
    QVERIFY(primaryNode);
    QVERIFY(primaryNode->texture());
    QCOMPARE(primaryNode->rect(), item.property("primaryItemRect").toRectF());
    QCOMPARE(primaryNode->sourceRect(), item.property("visiblePrimaryPageRect").toRectF());

    auto* secondaryNode = dynamic_cast<QSGImageNode*>(root->firstChild()->nextSibling());
    QVERIFY(secondaryNode);
    QVERIFY(secondaryNode->texture());
    QCOMPARE(secondaryNode->rect(), item.property("secondaryItemRect").toRectF());
    QCOMPARE(secondaryNode->sourceRect(), item.property("visibleSecondaryPageRect").toRectF());
}

void ImageViewportRenderSceneGraphTest::
    retainedTwoPageSpreadKeepsSecondaryLayerAfterPrimaryReplacement()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(QColor(255, 0, 0, 255));
    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 255, 0, 255));
    ImageFrame primaryFrame(primaryImage);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    QQuickWindow window;
    window.resize(88, 44);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(88.0, 44.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QScopedPointer<QSGNode> readyRoot(item.takePaintNode());
    QVERIFY(readyRoot);
    QCOMPARE(readyRoot->childCount(), 2);
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> loadingResult(factory.fromProvider(&adapter));
    QVERIFY(loadingResult->sequence());

    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(loadingResult->sequence()), {}),
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("secondaryDisplayedImageSize").toSizeF(), QSizeF(30.0, 20.0));

    QScopedPointer<QSGNode> retainedRoot(item.takePaintNode());
    QVERIFY(retainedRoot);
    QCOMPARE(retainedRoot->childCount(), 2);

    auto* primaryNode = dynamic_cast<QSGImageNode*>(retainedRoot->firstChild());
    QVERIFY(primaryNode);
    QCOMPARE(primaryNode->rect(), item.property("primaryItemRect").toRectF());

    auto* secondaryNode = dynamic_cast<QSGImageNode*>(retainedRoot->firstChild()->nextSibling());
    QVERIFY(secondaryNode);
    QCOMPARE(secondaryNode->rect(), item.property("secondaryItemRect").toRectF());
    QCOMPARE(secondaryNode->sourceRect(), item.property("visibleSecondaryPageRect").toRectF());

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QScopedPointer<QSGNode> clearedRoot(item.takePaintNode());
    QVERIFY(clearedRoot.isNull());
}

void ImageViewportRenderSceneGraphTest::secondaryProviderFrameCompletesSpreadTextureNodes()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(QColor(255, 0, 0, 255));
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

    QQuickWindow window;
    window.resize(88, 44);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(88.0, 44.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));

    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 255, 0, 255));
    ImageFrame secondaryFrame(secondaryImage);
    emit sessionFactory->lastSession()->imageFrameReady(
        sessionFactory->lastSession()->lastFrameToken(), &secondaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);
    QCOMPARE(root->childCount(), 2);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    auto* primaryNode = dynamic_cast<QSGImageNode*>(root->firstChild());
    QVERIFY(primaryNode);
    QCOMPARE(primaryNode->rect(), item.property("primaryItemRect").toRectF());

    auto* secondaryNode = dynamic_cast<QSGImageNode*>(root->firstChild()->nextSibling());
    QVERIFY(secondaryNode);
    QCOMPARE(secondaryNode->rect(), item.property("secondaryItemRect").toRectF());
}

void ImageViewportRenderSceneGraphTest::primaryAndSecondaryProviderFramesCommitOneSpread()
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

    QQuickWindow window;
    window.resize(88, 44);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(88.0, 44.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*primarySessionCount, 1);
    QCOMPARE(*secondarySessionCount, 1);
    QCOMPARE(*primaryMetadataRequestCount, 1);
    QCOMPARE(*secondaryMetadataRequestCount, 1);

    emit primarySessionFactory->lastSession()->metadataReady(
        primarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(10.0, 20.0)));
    drainQueuedProviderResults();

    QCOMPARE(*primaryFrameRequestCount, 1);
    QCOMPARE(*primaryLastRequestedFrame, 0);

    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(QColor(255, 0, 0, 255));
    ImageFrame primaryFrame(primaryImage);
    emit primarySessionFactory->lastSession()->imageFrameReady(
        primarySessionFactory->lastSession()->lastFrameToken(), &primaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QVERIFY(hasPendingRenderCommitForTest(item));
    QScopedPointer<QSGNode> partialRoot(item.takePaintNode());
    QVERIFY(partialRoot.isNull());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));

    emit secondarySessionFactory->lastSession()->metadataReady(
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(30.0, 20.0)));
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 1);
    QCOMPARE(*secondaryLastRequestedFrame, 0);

    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 255, 0, 255));
    ImageFrame secondaryFrame(secondaryImage);
    emit secondarySessionFactory->lastSession()->imageFrameReady(
        secondarySessionFactory->lastSession()->lastFrameToken(), &secondaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);
    QCOMPARE(root->childCount(), 2);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportRenderSceneGraphTest::deviceIndependentStillImageUsesPhysicalTextureSourceRect()
{
    ImageSequenceFactory factory;
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(2.0);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(20, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(20.0, 20.0));
    item.setSequence(result->sequence());

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);

    auto* imageNode = dynamic_cast<QSGImageNode*>(root->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 2.0, 1.0));
    QCOMPARE(imageNode->sourceRect(), QRectF(0.0, 0.0, 4.0, 2.0));
}

void ImageViewportRenderSceneGraphTest::solidBackgroundRendersBehindImageNode()
{
    ImageSequenceFactory factory;
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(40.0, 20.0));
    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(QColor(20, 40, 60, 255));
    item.setSequence(result->sequence());

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);
    QCOMPARE(root->childCount(), 2);

    auto* background = dynamic_cast<QSGSimpleRectNode*>(root->firstChild());
    QVERIFY(background);
    QCOMPARE(background->rect(), QRectF(0.0, 0.0, 40.0, 20.0));
    QCOMPARE(background->color(), QColor(20, 40, 60, 255));

    auto* imageNode = dynamic_cast<QSGImageNode*>(root->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(imageNode->rect(), item.property("contentRect").toRectF());
}

void ImageViewportRenderSceneGraphTest::qualityAndMirroringConfigureTextureNode()
{
    ImageSequenceFactory factory;
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(40.0, 20.0));
    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewport::CommandOutcome::Accepted);
    item.setMirrorHorizontally(true);
    item.setMirrorVertically(true);
    item.setSequence(result->sequence());

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);

    auto* imageNode = dynamic_cast<QSGImageNode*>(root->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(imageNode->filtering(), QSGTexture::Nearest);
    if (imageNode->texture()->hasMipmaps()) {
        QCOMPARE(imageNode->mipmapFiltering(), QSGTexture::Linear);
    } else {
        QCOMPARE(imageNode->mipmapFiltering(), QSGTexture::None);
    }
    QVERIFY(imageNode->textureCoordinatesTransform() & QSGImageNode::MirrorHorizontally);
    QVERIFY(imageNode->textureCoordinatesTransform() & QSGImageNode::MirrorVertically);
}

void ImageViewportRenderSceneGraphTest::rotatedImageTextureNodeUsesTransform_data()
{
    QTest::addColumn<int>("rotationDegrees");
    QTest::addColumn<bool>("mirrorHorizontally");
    QTest::addColumn<bool>("mirrorVertically");

    for (int rotationDegrees : { 90, 180, 270 }) {
        QTest::addRow("rotate-%d", rotationDegrees) << rotationDegrees << false << false;
        QTest::addRow("rotate-%d-mirror-h", rotationDegrees) << rotationDegrees << true << false;
        QTest::addRow("rotate-%d-mirror-v", rotationDegrees) << rotationDegrees << false << true;
        QTest::addRow("rotate-%d-mirror-both", rotationDegrees) << rotationDegrees << true
                                                               << true;
    }
}

void ImageViewportRenderSceneGraphTest::rotatedImageTextureNodeUsesTransform()
{
    QFETCH(int, rotationDegrees);
    QFETCH(bool, mirrorHorizontally);
    QFETCH(bool, mirrorVertically);

    ImageSequenceFactory factory;
    QImage image(10, 20, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setMirrorHorizontally(mirrorHorizontally);
    item.setMirrorVertically(mirrorVertically);
    item.setSequence(result->sequence());
    ImageViewportPresentationCommand rotationCommand;
    rotationCommand.setRotationDegrees(rotationDegrees);
    QCOMPARE(item.setPresentation(rotationCommand), ImageViewport::CommandOutcome::Accepted);

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);
    QCOMPARE(root->childCount(), 1);

    auto* transformNode = dynamic_cast<QSGTransformNode*>(root->firstChild());
    QVERIFY(transformNode);
    QCOMPARE(transformNode->childCount(), 1);

    auto* imageNode = dynamic_cast<QSGImageNode*>(transformNode->firstChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());

    const QRectF targetRect = item.property("contentRect").toRectF();
    const bool swapsAxes = rotationDegrees == 90 || rotationDegrees == 270;
    const QSizeF unrotatedSize = swapsAxes
        ? QSizeF(targetRect.height(), targetRect.width())
        : targetRect.size();
    const QRectF unrotatedRect(
        targetRect.center().x() - unrotatedSize.width() / 2.0,
        targetRect.center().y() - unrotatedSize.height() / 2.0,
        unrotatedSize.width(),
        unrotatedSize.height());
    QCOMPARE(imageNode->rect(), unrotatedRect);
    QCOMPARE(imageNode->sourceRect(), item.property("visibleImageRect").toRectF());
    QCOMPARE(bool(imageNode->textureCoordinatesTransform() & QSGImageNode::MirrorHorizontally),
        mirrorHorizontally);
    QCOMPARE(bool(imageNode->textureCoordinatesTransform() & QSGImageNode::MirrorVertically),
        mirrorVertically);

    QMatrix4x4 expectedTransform;
    expectedTransform.translate(targetRect.center().x(), targetRect.center().y());
    expectedTransform.rotate(rotationDegrees, 0.0f, 0.0f, 1.0f);
    expectedTransform.translate(-targetRect.center().x(), -targetRect.center().y());
    const QMatrix4x4 actualTransform = transformNode->matrix();
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            QVERIFY(qFuzzyCompare(actualTransform(row, column) + 1.0f,
                expectedTransform(row, column) + 1.0f));
        }
    }
}

void ImageViewportRenderSceneGraphTest::renderAdapterReportsMissingWindowFailureCause()
{
    QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));

    RenderAdapter adapter;
    const RenderAdapterSceneGraph::Output output = RenderAdapterSceneGraph::createNode(adapter,
        nullptr, { renderAdapterInputForPayload(renderAdapterPayload(image)), nullptr });

    QCOMPARE(output.result, RenderAdapter::CommitResult::Failed);
    QCOMPARE(output.failureCause, RenderFailureCause::MissingWindow);
}

void ImageViewportRenderSceneGraphTest::renderAdapterReportsTextureCreationFailureCause()
{
    QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    QQuickWindow window;
    NullTextureSceneGraphFactory sceneGraphFactory;
    RenderAdapter::Input input
        = renderAdapterInputForPayload(renderAdapterPayload(image));

    RenderAdapter adapter;
    const RenderAdapterSceneGraph::Output output = RenderAdapterSceneGraph::createNode(adapter,
        nullptr, { input, &window, &sceneGraphFactory });

    QCOMPARE(output.result, RenderAdapter::CommitResult::Failed);
    QCOMPARE(output.failureCause, RenderFailureCause::TextureCreationFailure);
}

void ImageViewportRenderSceneGraphTest::renderAdapterReportsImageNodeCreationFailureCause()
{
    QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    QQuickWindow window;
    NullImageNodeSceneGraphFactory sceneGraphFactory;
    RenderAdapter::Input input
        = renderAdapterInputForPayload(renderAdapterPayload(image));

    RenderAdapter adapter;
    const RenderAdapterSceneGraph::Output output = RenderAdapterSceneGraph::createNode(adapter,
        nullptr, { input, &window, &sceneGraphFactory });

    QCOMPARE(output.result, RenderAdapter::CommitResult::Failed);
    QCOMPARE(output.failureCause, RenderFailureCause::ImageNodeCreationFailure);
}

void ImageViewportRenderSceneGraphTest::renderAdapterReportsInvalidRolePayloadFailureCause()
{
    QQuickWindow window;
    RenderAdapter::Input input;
    input.itemSize = QSizeF(10.0, 10.0);
    input.imageLayers.append({ ImageViewport::PageRole::Primary,
        renderAdapterPayload({}), QRectF(0.0, 0.0, 10.0, 10.0),
        QRectF(0.0, 0.0, 2.0, 2.0) });

    RenderAdapter adapter;
    const RenderAdapterSceneGraph::Output output
        = RenderAdapterSceneGraph::createNode(adapter, nullptr, { input, &window });

    QCOMPARE(output.result, RenderAdapter::CommitResult::Failed);
    QCOMPARE(output.failureCause, RenderFailureCause::InvalidRolePayload);
}

void ImageViewportRenderSceneGraphTest::initialRenderFailureWithoutRetainedContentReturnsNullNode()
{
    ImageSequenceFactory factory;
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    PaintProbeViewport item;
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QScopedPointer<QSGNode> root(item.takePaintNode());

    QVERIFY(root.isNull());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
}

void ImageViewportRenderSceneGraphTest::retainedRenderFailureKeepsOldPaintNode()
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

    QQuickWindow window;
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    std::unique_ptr<QSGNode> readyRoot(item.takePaintNode());
    QVERIFY(readyRoot);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    item.setParentItem(nullptr);
    QSGNode* oldNode = readyRoot.release();
    QScopedPointer<QSGNode> retainedRoot(item.takePaintNode(oldNode));

    QCOMPARE(retainedRoot.data(), oldNode);
    QCOMPARE(retainedRoot->childCount(), 1);
    auto* imageNode = dynamic_cast<QSGImageNode*>(retainedRoot->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(!hasPendingRenderCommitForTest(item));
}

void ImageViewportRenderSceneGraphTest::renderPlanBuildsBackgroundPrimitivesWithoutSceneGraph()
{
    RenderAdapter::Input input;
    input.itemSize = QSizeF(18.0, 10.0);
    input.backgroundMode = ImageViewport::BackgroundMode::Checkerboard;

    RenderAdapter adapter;
    const RenderAdapter::RenderPlan plan = adapter.createPlan(input);

    QCOMPARE(plan.result, RenderAdapter::CommitResult::Empty);
    QCOMPARE(plan.backgroundRects.size(), 6);
    QCOMPARE(plan.backgroundRects.at(0).rect, QRectF(0.0, 0.0, 8.0, 8.0));
    QCOMPARE(plan.backgroundRects.at(0).color, QColor(238, 238, 238));
    QCOMPARE(plan.backgroundRects.at(1).rect, QRectF(8.0, 0.0, 8.0, 8.0));
    QCOMPARE(plan.backgroundRects.at(1).color, QColor(204, 204, 204));
    QCOMPARE(plan.backgroundRects.at(5).rect, QRectF(16.0, 8.0, 2.0, 2.0));
    QCOMPARE(plan.backgroundRects.at(5).color, QColor(204, 204, 204));
    QVERIFY(plan.imageLayers.isEmpty());
}

void ImageViewportRenderSceneGraphTest::renderPlanBuildsRoleLayerMappingWithoutSceneGraph()
{
    QImage secondaryImage(4, 4, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(QColor(0, 255, 0, 255));
    secondaryImage.setDevicePixelRatio(2.0);
    QImage primaryImage(2, 2, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(QColor(255, 0, 0, 255));
    ImageViewportInternal::PreparedPayload secondaryPayload
        = { true, 3, 5, 7, secondaryImage };
    ImageViewportInternal::PreparedPayload primaryPayload = { true, 3, 5, 8, primaryImage };

    RenderAdapter::Input input;
    input.itemSize = QSizeF(30.0, 20.0);
    input.imageLayers.append({ ImageViewport::PageRole::Secondary, secondaryPayload,
        QRectF(10.0, 0.0, 10.0, 20.0), QRectF(1.0, 2.0, 3.0, 4.0), 90, true, false });
    input.imageLayers.append({ ImageViewport::PageRole::Primary, primaryPayload,
        QRectF(0.0, 0.0, 10.0, 20.0), QRectF(0.0, 0.0, 2.0, 2.0), 0, false, true });

    RenderAdapter adapter;
    const RenderAdapter::RenderPlan plan = adapter.createPlan(input);

    QCOMPARE(plan.result, RenderAdapter::CommitResult::Committed);
    QCOMPARE(plan.imageLayers.size(), 2);
    QCOMPARE(plan.imageLayers.at(0).role, ImageViewport::PageRole::Secondary);
    QCOMPARE(plan.imageLayers.at(0).preparedPayload.payloadId, quint64(7));
    QCOMPARE(plan.imageLayers.at(0).unrotatedTargetRect, QRectF(5.0, 5.0, 20.0, 10.0));
    QCOMPARE(plan.imageLayers.at(0).physicalSourceRect, QRectF(2.0, 4.0, 6.0, 8.0));
    QCOMPARE(plan.imageLayers.at(0).rotationDegrees, 90);
    QCOMPARE(plan.imageLayers.at(0).mirrorHorizontally, true);
    QCOMPARE(plan.imageLayers.at(0).mirrorVertically, false);
    QCOMPARE(plan.imageLayers.at(1).role, ImageViewport::PageRole::Primary);
    QCOMPARE(plan.imageLayers.at(1).preparedPayload.payloadId, quint64(8));
    QCOMPARE(plan.rolePayloads.size(), 2);
    QCOMPARE(plan.rolePayloads.at(0).role, ImageViewport::PageRole::Secondary);
    QCOMPARE(plan.rolePayloads.at(0).preparedPayload.payloadId, quint64(7));
    QCOMPARE(plan.preparedPayload.payloadId, quint64(7));
}

void ImageViewportRenderSceneGraphTest::renderPlanReportsPreMaterializationFailureIntent()
{
    RenderAdapter adapter;

    RenderAdapter::Input invalidPayload;
    invalidPayload.itemSize = QSizeF(10.0, 10.0);
    invalidPayload.imageLayers.append({ ImageViewport::PageRole::Secondary,
        renderAdapterPayload({}), QRectF(0.0, 0.0, 10.0, 10.0),
        QRectF(0.0, 0.0, 2.0, 2.0) });
    const RenderAdapter::RenderPlan invalidPayloadPlan = adapter.createPlan(invalidPayload);
    QCOMPARE(invalidPayloadPlan.result, RenderAdapter::CommitResult::Failed);
    QCOMPARE(invalidPayloadPlan.failureCause, RenderFailureCause::InvalidRolePayload);
    QCOMPARE(invalidPayloadPlan.failedRole, ImageViewport::PageRole::Secondary);
}

void ImageViewportRenderSceneGraphTest::coverImageTextureNodeUsesVisibleSourceRect()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);

    auto* imageNode = dynamic_cast<QSGImageNode*>(root->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(imageNode->rect(), QRectF(0.0, 0.0, 100.0, 100.0));
    QCOMPARE(imageNode->sourceRect(), item.property("visibleImageRect").toRectF());
}

void ImageViewportRenderSceneGraphTest::providerStillFrameCreatesTexturePaintNode()
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
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(4.0, 2.0)));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->imageFrameReady(
        sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);

    auto* imageNode = dynamic_cast<QSGImageNode*>(root->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(imageNode->rect(), item.property("contentRect").toRectF());
}

void ImageViewportRenderSceneGraphTest::
    providerStillFrameWaitingForGeometryCreatesTexturePaintNode()
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
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(0.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(4.0, 2.0)));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->imageFrameReady(
        sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));

    QScopedPointer<QSGNode> zeroSizeRoot(item.takePaintNode());
    QVERIFY(zeroSizeRoot.isNull());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));

    item.setSize(QSizeF(40.0, 20.0));

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QVERIFY(commitPaintNode(item));

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);

    auto* imageNode = dynamic_cast<QSGImageNode*>(root->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(imageNode->rect(), item.property("contentRect").toRectF());
}

void ImageViewportRenderSceneGraphTest::providerRetainedFrameWaitingForGeometryIgnoresEmptyPaint()
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
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage firstImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(QColor(255, 0, 0, 255));
    ImageFrame firstFrame(firstImage);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &firstFrame, 0, 0);

    QVERIFY(commitPaintNode(item));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    item.setSize(QSizeF(0.0, 20.0));
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*lastRequestedFrame, 1);

    QImage secondImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(QColor(0, 255, 0, 255));
    ImageFrame secondFrame(secondImage);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &secondFrame, 1, 100);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(hasPendingRenderCommitForTest(item));
    const quint64 pendingPayloadId = pendingRenderPayloadIdForTest(item);

    QScopedPointer<QSGNode> zeroSizeRoot(item.takePaintNode());
    QVERIFY(zeroSizeRoot.isNull());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(hasPendingRenderCommitForTest(item));
    QCOMPARE(pendingRenderPayloadIdForTest(item), pendingPayloadId);

    item.setSize(QSizeF(40.0, 20.0));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QVERIFY(commitPaintNode(item));

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(pendingRenderPayloadIdForTest(item), 0U);
}

QTEST_MAIN(ImageViewportRenderSceneGraphTest)

#include "tst_imageviewport_render_scenegraph.moc"
