#include "imageviewport_paint_test_support.h"
#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

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
    void deviceIndependentStillImageUsesPhysicalTextureSourceRect();
    void solidBackgroundRendersBehindImageNode();
    void qualityAndMirroringConfigureTextureNode();
    void coverImageTextureNodeUsesVisibleSourceRect();
    void providerStillFrameCreatesTexturePaintNode();
    void providerStillFrameWaitingForGeometryCreatesTexturePaintNode();
};

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
    item.setSmoothing(false);
    item.setMipmap(true);
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
    item.setFillMode(ImageViewport::FillMode::Cover);

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

QTEST_MAIN(ImageViewportRenderSceneGraphTest)

#include "tst_imageviewport_render_scenegraph.moc"
