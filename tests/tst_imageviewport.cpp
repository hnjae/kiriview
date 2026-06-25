#include "imageviewport.h"

#include <QtCore/QMetaEnum>
#include <QtCore/QPointF>
#include <QtGui/QImage>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <memory>

class ImageViewportTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructsAsQuickItem();
    void doesNotExposeSourceProperty();
    void exposesDocumentedQmlSurface();
    void hasDocumentedDefaultState();
    void qmlImportsDocumentedSurface();
    void imageSequenceIsNotQmlCreatable();
    void imageSequenceProviderAdapterIsNotQmlCreatable();
    void exposesTypedSequenceFactorySurface();
    void exposesImageSequenceLimits();
    void commandsWithoutRequestAreIgnoredDiagnostics();
    void stillImageSequenceAssignmentPublishesReadyState();
    void stillImageCommandsPreserveOrReplaceDocumentedState();
    void stillImageFillModesAndMirroringUseDocumentedGeometry();
    void stillImageAssignmentWaitsForPositiveGeometry();
    void stillImageFactoryRejectsPublishedLimitViolations();
    void timedFrameListBuilderValidatesEntries();
    void timedFrameListAssignmentPublishesInitialTimedState();
    void timedFrameListSeekCommandsSelectDocumentedTargets();
    void timedFrameListPlaybackCommandsUpdatePhase();
    void timedFrameListPlaybackAdvancesDeterministically();
    void timedFrameListLoopingPlaybackWrapsToFirstFrame();
    void replacementRetainsPreviousDisplayWhileWaitingForGeometry();
    void providerFactoryRejectsBaseAdapterWithoutSessionFactory();
    void providerSequenceOpensSessionAfterAdapterDestruction();
    void providerSessionClosesWhenViewportIsDestroyed();
    void providerStillMetadataSelectsInitialFrameRequest();
    void providerTimedMetadataSelectsInitialFrameRequest();
    void providerFrameSeekBeforeMetadataResolvesAfterMetadata();
    void providerStillFrameReadyCommitsDisplay();
    void providerTimedFrameReadyCommitsTimedDisplay();
    void providerTimedFrameSeekRequestsSelectedFrame();
    void providerTimedPositionSeekRequestsResolvedFrame();
    void providerTimedPlaybackCommandsUpdatePhase();
    void providerTimedPlaybackAdvancesDeterministically();
    void providerTimedPlaybackWaitsForMetadata();
    void providerTimedStopSupersedesPlaybackRequest();
    void providerTimedSeekWhilePlayingWaitsForFrame();
    void providerMetadataFailureReportsProviderFailure();
    void providerFrameFailureKeepsGenerationSeekable();
    void providerMetadataUnsupportedReportsUnsupportedRequest();
    void providerFrameUnsupportedKeepsGenerationSeekable();
    void providerMetadataCancellationReportsProviderFailure();
    void providerFrameCancellationReportsProviderFailure();
    void presentationChangesNotifyGeometryState();
};

namespace {

QString componentErrors(const QQmlComponent &component)
{
    QStringList messages;
    for (const QQmlError &error : component.errors()) {
        messages.append(error.toString());
    }
    return messages.join(QLatin1Char('\n'));
}

int enumValue(const QMetaObject *metaObject, const char *enumName, const char *key)
{
    const int index = metaObject->indexOfEnumerator(enumName);
    if (index < 0) {
        return -1;
    }
    return metaObject->enumerator(index).keyToValue(key);
}

class CountingProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit CountingProviderSession(const std::shared_ptr<int> &metadataRequestCount,
        const std::shared_ptr<int> &frameRequestCount,
        const std::shared_ptr<int> &lastRequestedFrame,
        const std::shared_ptr<int> &closeCount,
        QObject *parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
        , m_lastRequestedFrame(lastRequestedFrame)
        , m_closeCount(closeCount)
    {
    }

    void requestMetadata(const ImageSequenceProviderRequestToken &token) override
    {
        m_lastMetadataToken = token;
        ++*m_metadataRequestCount;
    }

    void requestFrame(const ImageSequenceProviderRequestToken &token, int frame) override
    {
        m_lastFrameToken = token;
        *m_lastRequestedFrame = frame;
        ++*m_frameRequestCount;
    }

    void close() override
    {
        ++*m_closeCount;
    }

    ImageSequenceProviderRequestToken lastMetadataToken() const
    {
        return m_lastMetadataToken;
    }

    ImageSequenceProviderRequestToken lastFrameToken() const
    {
        return m_lastFrameToken;
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
    std::shared_ptr<int> m_lastRequestedFrame;
    std::shared_ptr<int> m_closeCount;
    ImageSequenceProviderRequestToken m_lastMetadataToken;
    ImageSequenceProviderRequestToken m_lastFrameToken;
};

class CountingProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit CountingProviderSessionFactory(const std::shared_ptr<int> &sessionCount,
        const std::shared_ptr<int> &metadataRequestCount,
        const std::shared_ptr<int> &frameRequestCount,
        const std::shared_ptr<int> &lastRequestedFrame,
        const std::shared_ptr<int> &closeCount)
        : m_sessionCount(sessionCount)
        , m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
        , m_lastRequestedFrame(lastRequestedFrame)
        , m_closeCount(closeCount)
    {
    }

    ImageSequenceProviderSession *createSession(QObject *parent) override
    {
        ++*m_sessionCount;
        CountingProviderSession *session = new CountingProviderSession(m_metadataRequestCount,
            m_frameRequestCount,
            m_lastRequestedFrame,
            m_closeCount,
            parent);
        m_lastSession = session;
        return session;
    }

    CountingProviderSession *lastSession() const
    {
        return m_lastSession;
    }

private:
    std::shared_ptr<int> m_sessionCount;
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
    std::shared_ptr<int> m_lastRequestedFrame;
    std::shared_ptr<int> m_closeCount;
    QPointer<CountingProviderSession> m_lastSession;
};

class CountingProviderAdapter final : public ImageSequenceProviderAdapter
{
public:
    explicit CountingProviderAdapter(std::shared_ptr<ImageSequenceProviderSessionFactory> factory, QObject *parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , m_factory(std::move(factory))
    {
    }

    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory() const override
    {
        return m_factory;
    }

private:
    std::shared_ptr<ImageSequenceProviderSessionFactory> m_factory;
};

}

void ImageViewportTest::defaultConstructsAsQuickItem()
{
    ImageViewport item;

    QVERIFY(item.flags().testFlag(QQuickItem::ItemHasContents));
}

void ImageViewportTest::doesNotExposeSourceProperty()
{
    ImageViewport item;

    QCOMPARE(item.metaObject()->indexOfProperty("source"), -1);
}

void ImageViewportTest::exposesDocumentedQmlSurface()
{
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();

    const QList<QByteArray> properties = {
        "sequence",
        "requestStatus",
        "requestReason",
        "commandReason",
        "displayStatus",
        "playbackPhase",
        "displayedFrame",
        "requestedFrame",
        "displayedPosition",
        "requestedPosition",
        "frameCount",
        "totalDuration",
        "frameSeekBounds",
        "positionSeekBounds",
        "timedPlaybackSupport",
        "frameSeekSupport",
        "positionSeekSupport",
        "displayedImageSize",
        "fillMode",
        "horizontalAlignment",
        "verticalAlignment",
        "contentRect",
        "visibleImageRect",
        "displayRevision",
        "requestRevision",
        "commandRevision",
        "errorString",
        "warningString",
        "zoom",
        "pan",
        "smoothing",
        "mipmap",
        "mirrorHorizontally",
        "mirrorVertically",
        "backgroundMode",
        "backgroundColor",
        "looping",
    };

    for (const QByteArray &property : properties) {
        QVERIFY2(metaObject->indexOfProperty(property.constData()) >= 0, property.constData());
    }

    const QList<QByteArray> enumerators = {
        "RequestStatus",
        "RequestReason",
        "CommandReason",
        "DisplayStatus",
        "PlaybackPhase",
        "TriState",
        "CommandOutcome",
        "FillMode",
        "HorizontalAlignment",
        "VerticalAlignment",
        "BackgroundMode",
    };

    for (const QByteArray &enumerator : enumerators) {
        QVERIFY2(metaObject->indexOfEnumerator(enumerator.constData()) >= 0, enumerator.constData());
    }

    const QList<QByteArray> methods = {
        "play()",
        "pause()",
        "stop()",
        "seek(int)",
        "seekToPosition(int)",
        "clear()",
        "resetView()",
        "itemToImage(double,double)",
        "imageToItem(double,double)",
        "containsVisibleImagePoint(double,double)",
    };

    for (const QByteArray &method : methods) {
        QVERIFY2(metaObject->indexOfMethod(QMetaObject::normalizedSignature(method.constData())) >= 0, method.constData());
    }
}

void ImageViewportTest::hasDocumentedDefaultState()
{
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.property("sequence").value<QObject *>(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    QCOMPARE(item.property("displayRevision").toUInt(), 0U);
    QCOMPARE(item.property("requestRevision").toUInt(), 0U);
    QCOMPARE(item.property("commandRevision").toUInt(), 0U);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(item.property("fillMode").toInt(), enumValue(metaObject, "FillMode", "Contain"));
    QCOMPARE(item.property("horizontalAlignment").toInt(), enumValue(metaObject, "HorizontalAlignment", "AlignHCenter"));
    QCOMPARE(item.property("verticalAlignment").toInt(), enumValue(metaObject, "VerticalAlignment", "AlignVCenter"));
    QCOMPARE(item.property("zoom").toDouble(), 1.0);
    QCOMPARE(item.property("pan").toPointF(), QPointF(0.0, 0.0));
    QCOMPARE(item.property("smoothing").toBool(), true);
    QCOMPARE(item.property("mipmap").toBool(), false);
    QCOMPARE(item.property("mirrorHorizontally").toBool(), false);
    QCOMPARE(item.property("mirrorVertically").toBool(), false);
    QCOMPARE(item.property("backgroundMode").toInt(), enumValue(metaObject, "BackgroundMode", "Transparent"));
    QCOMPARE(item.property("backgroundColor").value<QColor>(), QColor(Qt::transparent));
    QCOMPARE(item.property("looping").toBool(), false);
}

void ImageViewportTest::qmlImportsDocumentedSurface()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    property int noRequest: ImageViewport.RequestStatus.NoRequest
    property int loading: ImageViewport.RequestStatus.Loading
    property int retained: ImageViewport.DisplayStatus.Retained
    property int waiting: ImageViewport.PlaybackPhase.Waiting
    property int accepted: ImageViewport.CommandOutcome.Accepted
    property int unsupported: ImageViewport.CommandOutcome.Unsupported
    property int invalid: ImageViewport.CommandOutcome.Invalid
    property int ignoredNoRequest: ImageViewport.CommandOutcome.IgnoredNoRequest
    property int cover: ImageViewport.FillMode.Cover
    property int center: ImageViewport.FillMode.Center
    property bool factoryReturnsNull: ImageSequenceFactory.fromFrame(null).sequence === null
    property bool mappingInvalid: itemToImage(1, 1).valid === false
    property bool mappingHasFlatFields: imageToItem(1, 1).x === 0 && imageToItem(1, 1).y === 0
    property bool limitsAvailable: ImageSequenceLimits.maximumLogicalWidth >= 8192
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(object->property("noRequest").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(object->property("loading").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(object->property("retained").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(object->property("waiting").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(object->property("accepted").toInt(), enumValue(metaObject, "CommandOutcome", "Accepted"));
    QCOMPARE(object->property("unsupported").toInt(), enumValue(metaObject, "CommandOutcome", "Unsupported"));
    QCOMPARE(object->property("invalid").toInt(), enumValue(metaObject, "CommandOutcome", "Invalid"));
    QCOMPARE(object->property("ignoredNoRequest").toInt(), enumValue(metaObject, "CommandOutcome", "IgnoredNoRequest"));
    QCOMPARE(object->property("cover").toInt(), enumValue(metaObject, "FillMode", "Cover"));
    QCOMPARE(object->property("center").toInt(), enumValue(metaObject, "FillMode", "Center"));
    QCOMPARE(object->property("factoryReturnsNull").toBool(), true);
    QCOMPARE(object->property("mappingInvalid").toBool(), true);
    QCOMPARE(object->property("mappingHasFlatFields").toBool(), true);
    QCOMPARE(object->property("limitsAvailable").toBool(), true);
}

void ImageViewportTest::imageSequenceIsNotQmlCreatable()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import ImageViewport 1.0

ImageSequence {}
)",
        QUrl());

    QVERIFY(component.isError());
}

void ImageViewportTest::imageSequenceProviderAdapterIsNotQmlCreatable()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import ImageViewport 1.0

ImageSequenceProviderAdapter {}
)",
        QUrl());

    QVERIFY(component.isError());
}

void ImageViewportTest::exposesTypedSequenceFactorySurface()
{
    ImageSequenceFactory factory;
    const QMetaObject *metaObject = factory.metaObject();

    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromFrame(ImageFrame*)")) >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromTimedFrameList(TimedImageFrameList*)")) >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromProvider(ImageSequenceProviderAdapter*)")) >= 0);

    QObject *result = factory.fromFrame(nullptr);
    QVERIFY(result);
    QCOMPARE(result->property("sequence").value<QObject *>(), nullptr);
    QCOMPARE(result->property("outcome").toInt(), enumValue(result->metaObject(), "FactoryOutcome", "Invalid"));
    QVERIFY(!result->property("errorString").toString().isEmpty());
}

void ImageViewportTest::exposesImageSequenceLimits()
{
    ImageSequenceLimits limits;

    QCOMPARE(limits.property("maximumLogicalWidth").toInt(), ImageSequenceLimits::maximumLogicalWidth());
    QVERIFY(limits.property("maximumLogicalWidth").toInt() >= 8192);
    QVERIFY(limits.property("maximumLogicalHeight").toInt() >= 8192);
    QVERIFY(limits.property("maximumPixelsPerFrame").toLongLong() >= 67108864LL);
    QVERIFY(limits.property("maximumPayloadBytesPerFrame").toLongLong() >= 268435456LL);
    QVERIFY(limits.property("maximumTimedListFrameCount").toInt() >= 10000);
    QVERIFY(limits.property("maximumFrameDuration").toInt() >= 86400000);
    QVERIFY(limits.property("maximumTotalSequenceDuration").toInt() >= 86400000);
    QVERIFY(limits.property("maximumDiagnosticStringLength").toInt() >= 4096);
}

void ImageViewportTest::commandsWithoutRequestAreIgnoredDiagnostics()
{
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 1U);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));

    QCOMPARE(item.seek(-1), ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(item.property("commandRevision").toUInt(), 2U);
}

void ImageViewportTest::stillImageSequenceAssignmentPublishesReadyState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Created);
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 0);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));

    const QVariantMap centerImage = item.itemToImage(50.0, 50.0);
    QCOMPARE(centerImage.value("valid").toBool(), true);
    QCOMPARE(centerImage.value("x").toDouble(), 8.0);
    QCOMPARE(centerImage.value("y").toDouble(), 4.0);

    const QVariantMap rightEdgeImage = item.itemToImage(100.0, 50.0);
    QCOMPARE(rightEdgeImage.value("valid").toBool(), false);
    QCOMPARE(item.containsVisibleImagePoint(8.0, 4.0), true);
    QCOMPARE(item.containsVisibleImagePoint(16.0, 4.0), false);

    const QVariantMap centerItem = item.imageToItem(8.0, 4.0);
    QCOMPARE(centerItem.value("valid").toBool(), true);
    QCOMPARE(centerItem.value("x").toDouble(), 50.0);
    QCOMPARE(centerItem.value("y").toDouble(), 50.0);
}

void ImageViewportTest::stillImageCommandsPreserveOrReplaceDocumentedState()
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
    const QMetaObject *metaObject = item.metaObject();

    const uint readyRequestRevision = item.property("requestRevision").toUInt();
    const uint readyDisplayRevision = item.property("displayRevision").toUInt();
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QVERIFY(item.property("requestRevision").toUInt() > readyRequestRevision);
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    const uint afterAcceptedSeekRequestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 1U);
    QCOMPARE(item.property("requestRevision").toUInt(), afterAcceptedSeekRequestRevision);
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 2U);
    QCOMPARE(item.property("requestRevision").toUInt(), afterAcceptedSeekRequestRevision);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 3U);

    item.setZoom(2.0);
    item.setPan(QPointF(4.0, 8.0));
    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("zoom").toDouble(), 1.0);
    QCOMPARE(item.property("pan").toPointF(), QPointF(0.0, 0.0));
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("commandRevision").toUInt(), 4U);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportTest::stillImageFillModesAndMirroringUseDocumentedGeometry()
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

    item.setFillMode(ImageViewport::FillMode::Cover);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(-50.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(4.0, 0.0, 8.0, 8.0));
    QCOMPARE(item.itemToImage(0.0, 50.0).value("x").toDouble(), 4.0);
    QCOMPARE(item.itemToImage(99.0, 50.0).value("valid").toBool(), true);
    QCOMPARE(item.containsVisibleImagePoint(3.999, 4.0), false);
    QCOMPARE(item.containsVisibleImagePoint(12.0, 4.0), false);
    QCOMPARE(item.containsVisibleImagePoint(11.999, 4.0), true);
    QCOMPARE(item.imageToItem(4.0, 4.0).value("x").toDouble(), 0.0);
    QCOMPARE(item.imageToItem(12.0, 4.0).value("valid").toBool(), false);

    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignLeft);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 8.0, 8.0));
    QCOMPARE(item.itemToImage(99.0, 50.0).value("x").toDouble(), 7.92);

    item.setFillMode(ImageViewport::FillMode::Stretch);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 100.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(item.itemToImage(50.0, 50.0).value("x").toDouble(), 8.0);
    QCOMPARE(item.itemToImage(50.0, 50.0).value("y").toDouble(), 4.0);

    item.setFillMode(ImageViewport::FillMode::Center);
    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignHCenter);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(42.0, 46.0, 16.0, 8.0));
    QCOMPARE(item.itemToImage(42.0, 46.0).value("valid").toBool(), true);
    QCOMPARE(item.itemToImage(58.0, 50.0).value("valid").toBool(), false);

    item.setMirrorHorizontally(true);
    item.setMirrorVertically(true);
    const QVariantMap mirrored = item.itemToImage(42.001, 46.001);
    QCOMPARE(mirrored.value("valid").toBool(), true);
    QCOMPARE(mirrored.value("x").toDouble(), 15.999);
    QCOMPARE(mirrored.value("y").toDouble(), 7.999);

    const QVariantMap mirroredItem = item.imageToItem(15.999, 7.999);
    QCOMPARE(mirroredItem.value("valid").toBool(), true);
    QCOMPARE(mirroredItem.value("x").toDouble(), 42.001);
    QCOMPARE(mirroredItem.value("y").toDouble(), 46.001);
}

void ImageViewportTest::stillImageAssignmentWaitsForPositiveGeometry()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(0.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.itemToImage(0.0, 0.0).value("valid").toBool(), false);

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));

    const uint readyDisplayRevision = item.property("displayRevision").toUInt();
    item.setSize(QSizeF(0.0, 100.0));
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.containsVisibleImagePoint(8.0, 4.0), false);
}

void ImageViewportTest::stillImageFactoryRejectsPublishedLimitViolations()
{
    ImageSequenceFactory factory;
    QImage oversized(ImageSequenceLimits::maximumLogicalWidth() + 1,
        1,
        QImage::Format_ARGB32_Premultiplied);
    oversized.fill(Qt::transparent);
    ImageFrame frame(oversized);

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
    QVERIFY(result->errorString().contains(QStringLiteral("maximumLogicalWidth")));
}

void ImageViewportTest::timedFrameListBuilderValidatesEntries()
{
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QImage differentSizeImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    differentSizeImage.fill(Qt::transparent);
    ImageFrame differentSizeFrame(differentSizeImage);

    TimedImageFrameList list;
    const QMetaObject *metaObject = list.metaObject();
    QVERIFY(metaObject->indexOfProperty("count") >= 0);
    QVERIFY(metaObject->indexOfProperty("errorString") >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("appendFrame(ImageFrame*,int)")) >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("clear()")) >= 0);

    QCOMPARE(list.count(), 0);
    QCOMPARE(list.appendFrame(nullptr, 100), false);
    QCOMPARE(list.count(), 0);
    QVERIFY(list.errorString().contains(QStringLiteral("ImageFrame")));

    QCOMPARE(list.appendFrame(&frame, 0), false);
    QCOMPARE(list.count(), 0);
    QVERIFY(list.errorString().contains(QStringLiteral("duration")));

    QCOMPARE(list.appendFrame(&frame, 100), true);
    QCOMPARE(list.appendFrame(&differentSizeFrame, 100), false);
    QCOMPARE(list.count(), 1);
    QVERIFY(list.errorString().contains(QStringLiteral("logical size")));

    list.clear();
    QCOMPARE(list.count(), 0);
    QCOMPARE(list.errorString(), QString());
}

void ImageViewportTest::timedFrameListAssignmentPublishesInitialTimedState()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Created);
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), 350);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
}

void ImageViewportTest::timedFrameListSeekCommandsSelectDocumentedTargets()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
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
    const QMetaObject *metaObject = item.metaObject();

    const uint initialRequestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QVERIFY(item.property("requestRevision").toUInt() > initialRequestRevision);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    const uint acceptedFrameSeekRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(2), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(item.property("requestRevision").toUInt(), acceptedFrameSeekRevision);
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);

    QCOMPARE(item.seekToPosition(349), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 349);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    QCOMPARE(item.seekToPosition(350), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    const uint acceptedPositionSeekRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seekToPosition(351), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("requestRevision").toUInt(), acceptedPositionSeekRevision);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::timedFrameListPlaybackCommandsUpdatePhase()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
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
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);

    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Paused"));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportTest::timedFrameListPlaybackAdvancesDeterministically()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
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
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(99);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    item.setSize(QSizeF(0.0, 100.0));
    item.advancePlaybackForTest(1);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    item.advancePlaybackForTest(249);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);

    item.advancePlaybackForTest(1);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::timedFrameListLoopingPlaybackWrapsToFirstFrame()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
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
    item.setLooping(true);
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(350);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    item.advancePlaybackForTest(100);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::replacementRetainsPreviousDisplayWhileWaitingForGeometry()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage replacementImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(factory.fromFrame(&replacementFrame));
    QVERIFY(firstResult->sequence());
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(firstResult->sequence());
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));

    item.setSize(QSizeF(0.0, 100.0));
    item.setSequence(replacementResult->sequence());
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.itemToImage(1.0, 1.0).value("valid").toBool(), false);

    const uint retainedRequestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QVERIFY(item.property("requestRevision").toUInt() > retainedRequestRevision);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(8.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 100.0, 100.0));
}

void ImageViewportTest::providerFactoryRejectsBaseAdapterWithoutSessionFactory()
{
    ImageSequenceFactory factory;
    ImageSequenceProviderAdapter adapter;

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
    QVERIFY(result->errorString().contains(QStringLiteral("session")));
}

void ImageViewportTest::providerSequenceOpensSessionAfterAdapterDestruction()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);

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
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
}

void ImageViewportTest::providerSessionClosesWhenViewportIsDestroyed()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    {
        ImageViewport item;
        item.setSequence(result->sequence());
        QCOMPARE(*sessionCount, 1);
        QCOMPARE(*metadataRequestCount, 1);
        QCOMPARE(*closeCount, 0);
    }

    QCOMPARE(*closeCount, 1);
}

void ImageViewportTest::providerStillMetadataSelectsInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    QCOMPARE(*frameRequestCount, 0);

    const ImageSequenceProviderMetadata metadata = ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0));
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(), metadata);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 0);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
}

void ImageViewportTest::providerTimedMetadataSelectsInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderMetadata metadata = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250});
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(), metadata);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), 350);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
}

void ImageViewportTest::providerFrameSeekBeforeMetadataResolvesAfterMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
}

void ImageViewportTest::providerStillFrameReadyCommitsDisplay()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QCOMPARE(*frameRequestCount, 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
}

void ImageViewportTest::providerTimedFrameReadyCommitsTimedDisplay()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    QCOMPARE(*frameRequestCount, 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
}

void ImageViewportTest::providerTimedFrameSeekRequestsSelectedFrame()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    const uint readyDisplayRevision = item.property("displayRevision").toUInt();

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);
}

void ImageViewportTest::providerTimedPositionSeekRequestsResolvedFrame()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    QCOMPARE(item.seekToPosition(349), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 349);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
}

void ImageViewportTest::providerTimedPlaybackCommandsUpdatePhase()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));

    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Paused"));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportTest::providerTimedPlaybackAdvancesDeterministically()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(99);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(*frameRequestCount, 1);

    item.advancePlaybackForTest(1);

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::providerTimedPlaybackWaitsForMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(*frameRequestCount, 0);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
}

void ImageViewportTest::providerTimedStopSupersedesPlaybackRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    const ImageSequenceProviderRequestToken playbackToken = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emit sessionFactory->lastSession()->frameReady(playbackToken, &frame);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
}

void ImageViewportTest::providerTimedSeekWhilePlayingWaitsForFrame()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);

    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::providerMetadataFailureReportsProviderFailure()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata service unavailable"));

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("metadata service unavailable")));
}

void ImageViewportTest::providerFrameFailureKeepsGenerationSeekable()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QCOMPARE(*frameRequestCount, 1);

    emit sessionFactory->lastSession()->providerFailed(sessionFactory->lastSession()->lastFrameToken(),
        QStringLiteral("frame decode failed"));

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("frame decode failed")));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
}

void ImageViewportTest::providerMetadataUnsupportedReportsUnsupportedRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerUnsupported(sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("unsupported codec"));

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("unsupported codec")));
}

void ImageViewportTest::providerFrameUnsupportedKeepsGenerationSeekable()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QCOMPARE(*frameRequestCount, 1);

    emit sessionFactory->lastSession()->providerUnsupported(sessionFactory->lastSession()->lastFrameToken(),
        QStringLiteral("unsupported frame shape"));

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("unsupported frame shape")));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
}

void ImageViewportTest::providerMetadataCancellationReportsProviderFailure()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerCancelled(sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata cancelled by provider"));

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("metadata cancelled by provider")));
}

void ImageViewportTest::providerFrameCancellationReportsProviderFailure()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QCOMPARE(*frameRequestCount, 1);

    emit sessionFactory->lastSession()->providerCancelled(sessionFactory->lastSession()->lastFrameToken(),
        QStringLiteral("cancelled by provider"));

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("cancelled by provider")));
}

void ImageViewportTest::presentationChangesNotifyGeometryState()
{
    ImageViewport item;
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setZoom(2.0);
    QCOMPARE(geometrySpy.count(), 1);

    item.setPan(QPointF(4.0, 8.0));
    QCOMPARE(geometrySpy.count(), 2);

    item.setFillMode(ImageViewport::FillMode::Stretch);
    QCOMPARE(geometrySpy.count(), 3);

    item.setMirrorHorizontally(true);
    QCOMPARE(geometrySpy.count(), 4);
}

QTEST_MAIN(ImageViewportTest)

#include "tst_imageviewport.moc"
