#include <imageviewport.h>

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QUrl>
#include <QVariant>
#include <QtPlugin>

#include <cmath>
#include <memory>

Q_IMPORT_PLUGIN(ImageViewportPlugin)

class ConsumerSession final : public ImageSequenceProviderSession
{
public:
    using ImageSequenceProviderSession::ImageSequenceProviderSession;

    void requestMetadata(ImageSequenceProviderRequestToken token) override
    {
        emit metadataReady(token, ImageSequenceProviderMetadata::still(QSizeF(2.0, 2.0)));
    }

    void requestFrame(ImageSequenceProviderRequestToken token, int frame) override
    {
        QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        auto payload = std::make_unique<ImageFrame>(image);
        emit frameHandleWithMetadataReady(token,
            new ImageSequenceProviderFrameHandle(std::move(payload)),
            ImageSequenceProviderFrameMetadata::timedFrame(frame, 0, 100));
    }

    void requestPlayback(ImageSequenceProviderRequestToken token, int frame, int position) override
    {
        m_lastPlaybackFrame = frame;
        m_lastPlaybackPosition = position;
        emit providerProgress(token, 0.5);
        emit endOfSequence(token);
    }

    void cancelRequest(ImageSequenceProviderRequestToken token) override
    {
        m_lastCancelledToken = token;
        emit providerCancelled(token, QStringLiteral("cancelled"));
    }

    void close() override { m_closed = true; }

    int lastPlaybackFrame() const { return m_lastPlaybackFrame; }

    int lastPlaybackPosition() const { return m_lastPlaybackPosition; }

    ImageSequenceProviderRequestToken lastCancelledToken() const { return m_lastCancelledToken; }

    bool closed() const { return m_closed; }

private:
    int m_lastPlaybackFrame = -1;
    int m_lastPlaybackPosition = -1;
    ImageSequenceProviderRequestToken m_lastCancelledToken;
    bool m_closed = false;
};

class DefaultPlaybackFallbackSession final : public ImageSequenceProviderSession
{
public:
    using ImageSequenceProviderSession::ImageSequenceProviderSession;

    void requestMetadata(ImageSequenceProviderRequestToken) override { }

    void requestFrame(ImageSequenceProviderRequestToken token, int frame) override
    {
        m_lastFrameToken = token;
        m_lastFrame = frame;
        ++m_frameRequestCount;
    }

    ImageSequenceProviderRequestToken lastFrameToken() const { return m_lastFrameToken; }

    int lastFrame() const { return m_lastFrame; }

    int frameRequestCount() const { return m_frameRequestCount; }

private:
    ImageSequenceProviderRequestToken m_lastFrameToken;
    int m_lastFrame = -1;
    int m_frameRequestCount = 0;
};

class ConsumerSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    ImageSequenceProviderSession* createSession(QObject* parent) override
    {
        return new ConsumerSession(parent);
    }
};

class ConsumerAdapter final : public ImageSequenceProviderAdapter
{
public:
    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory() const override
    {
        return std::make_shared<ConsumerSessionFactory>();
    }

    ImageSequenceProviderMetadata knownMetadata() const override
    {
        return ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(2.0, 2.0), 2, 100);
    }

    CapabilitySupport timedPlaybackCapability() const override
    {
        return CapabilitySupport::KnownTrue;
    }

    CapabilitySupport frameSeekCapability() const override { return CapabilitySupport::KnownTrue; }

    CapabilitySupport positionSeekCapability() const override
    {
        return CapabilitySupport::KnownTrue;
    }
};

class TokenCaptureSession final : public ImageSequenceProviderSession
{
public:
    explicit TokenCaptureSession(
        std::shared_ptr<ImageSequenceProviderRequestToken> capturedToken, QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_capturedToken(std::move(capturedToken))
    {
    }

    void requestMetadata(ImageSequenceProviderRequestToken token) override
    {
        *m_capturedToken = token;
        emit metadataReady(token, ImageSequenceProviderMetadata::still(QSizeF(2.0, 2.0)));
    }

private:
    std::shared_ptr<ImageSequenceProviderRequestToken> m_capturedToken;
};

class TokenCaptureSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit TokenCaptureSessionFactory(
        std::shared_ptr<ImageSequenceProviderRequestToken> capturedToken)
        : m_capturedToken(std::move(capturedToken))
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent) override
    {
        return new TokenCaptureSession(m_capturedToken, parent);
    }

private:
    std::shared_ptr<ImageSequenceProviderRequestToken> m_capturedToken;
};

class TokenCaptureAdapter final : public ImageSequenceProviderAdapter
{
public:
    explicit TokenCaptureAdapter(std::shared_ptr<ImageSequenceProviderRequestToken> capturedToken)
        : m_capturedToken(std::move(capturedToken))
    {
    }

    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory() const override
    {
        return std::make_shared<TokenCaptureSessionFactory>(m_capturedToken);
    }

private:
    std::shared_ptr<ImageSequenceProviderRequestToken> m_capturedToken;
};

namespace {
ImageSequenceProviderRequestToken makeInstalledProviderRequestToken()
{
    auto capturedToken = std::make_shared<ImageSequenceProviderRequestToken>();
    TokenCaptureAdapter adapter(capturedToken);
    ImageSequenceFactory factory;
    std::unique_ptr<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    if (!result || !result->sequence()) {
        return {};
    }
    ImageViewport viewport;
    viewport.setSequence(result->sequence());
    return *capturedToken;
}

bool canCreateInstalledQmlViewport()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT));

    QQmlComponent component(&engine);
    component.setData(
        "import QtQuick\nimport ImageViewport 1.0\nImageViewport { width: 10; height: 10 }\n",
        QUrl());
    if (!component.isReady()) {
        qWarning() << component.errors();
        return false;
    }

    const std::unique_ptr<QObject> object(component.create());
    if (!object) {
        qWarning() << component.errors();
        return false;
    }
    return qobject_cast<ImageViewport*>(object.get()) != nullptr;
}

bool canReadInstalledQmlLimits()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQml
import ImageViewport 1.0

QtObject {
    property var maximumLogicalWidth: ImageSequenceLimits.maximumLogicalWidth
    property var maximumLogicalHeight: ImageSequenceLimits.maximumLogicalHeight
    property var maximumPixelsPerFrame: ImageSequenceLimits.maximumPixelsPerFrame
    property var maximumPayloadBytesPerFrame: ImageSequenceLimits.maximumPayloadBytesPerFrame
    property var maximumTimedListFrameCount: ImageSequenceLimits.maximumTimedListFrameCount
    property var maximumFrameDuration: ImageSequenceLimits.maximumFrameDuration
    property var maximumTotalSequenceDuration: ImageSequenceLimits.maximumTotalSequenceDuration
    property var maximumDiagnosticStringLength: ImageSequenceLimits.maximumDiagnosticStringLength
    property var maximumManualZoomPercent: ImageViewportDisplayLimits.maximumManualZoomPercent
}
)",
        QUrl());
    if (!component.isReady()) {
        qWarning() << component.errors();
        return false;
    }

    const std::unique_ptr<QObject> object(component.create());
    if (!object) {
        qWarning() << component.errors();
        return false;
    }

    if (object->property("maximumLogicalWidth").toInt()
            != ImageSequenceLimits::maximumLogicalWidth()
        || object->property("maximumLogicalHeight").toInt()
            != ImageSequenceLimits::maximumLogicalHeight()
        || object->property("maximumPixelsPerFrame").toLongLong()
            != ImageSequenceLimits::maximumPixelsPerFrame()
        || object->property("maximumPayloadBytesPerFrame").toLongLong()
            != ImageSequenceLimits::maximumPayloadBytesPerFrame()
        || object->property("maximumTimedListFrameCount").toInt()
            != ImageSequenceLimits::maximumTimedListFrameCount()
        || object->property("maximumFrameDuration").toInt()
            != ImageSequenceLimits::maximumFrameDuration()
        || object->property("maximumTotalSequenceDuration").toInt()
            != ImageSequenceLimits::maximumTotalSequenceDuration()
        || object->property("maximumDiagnosticStringLength").toInt()
            != ImageSequenceLimits::maximumDiagnosticStringLength()
        || object->property("maximumManualZoomPercent").toDouble()
            != ImageViewportDisplayLimits::maximumManualZoomPercent()) {
        return false;
    }

    return ImageSequenceLimits::maximumLogicalWidth() >= 8192
        && ImageSequenceLimits::maximumLogicalHeight() >= 8192
        && ImageSequenceLimits::maximumPixelsPerFrame() >= 67108864LL
        && ImageSequenceLimits::maximumPayloadBytesPerFrame() >= 268435456LL
        && ImageSequenceLimits::maximumTimedListFrameCount() >= 10000
        && ImageSequenceLimits::maximumFrameDuration() >= 86400000
        && ImageSequenceLimits::maximumTotalSequenceDuration() >= 86400000
        && ImageSequenceLimits::maximumDiagnosticStringLength() >= 4096
        && ImageViewportDisplayLimits::maximumManualZoomPercent() >= 100.0;
}

bool canUseInstalledQmlFactorySurface()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQml
import ImageViewport 1.0

QtObject {
    readonly property var frameResult: ImageSequenceFactory.fromFrame(null)
    readonly property var listResult: ImageSequenceFactory.fromTimedFrameList(null)
    readonly property var providerResult: ImageSequenceFactory.fromProvider(null)
    property bool factorySurfaceAvailable: frameResult.sequence === null
        && frameResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Invalid
        && frameResult.errorString.length > 0
        && frameResult.warningString === ""
        && listResult.sequence === null
        && listResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Invalid
        && listResult.errorString.length > 0
        && listResult.warningString === ""
        && providerResult.sequence === null
        && providerResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Invalid
        && providerResult.errorString.length > 0
        && providerResult.warningString === ""
}
)",
        QUrl());
    if (!component.isReady()) {
        qWarning() << component.errors();
        return false;
    }

    const std::unique_ptr<QObject> object(component.create());
    if (!object) {
        qWarning() << component.errors();
        return false;
    }

    return object->property("factorySurfaceAvailable").toBool();
}

bool canUseInstalledQmlTypedFactorySurface()
{
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

Item {
    property ImageFrame suppliedFrame
    property bool typedFactorySurfaceAvailable: false

    ImageViewport {
        id: viewport
        width: 40
        height: 20
    }

    TimedImageFrameList {
        id: list
    }

    Component.onCompleted: {
        const frameResult = ImageSequenceFactory.fromFrame(suppliedFrame)
        const appendAccepted = list.appendFrame(suppliedFrame, 100)
        const timedResult = ImageSequenceFactory.fromTimedFrameList(list)
        viewport.sequence = timedResult.sequence
        typedFactorySurfaceAvailable = frameResult.sequence !== null
            && frameResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Created
            && appendAccepted === true
            && list.count === 1
            && timedResult.sequence !== null
            && timedResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Created
            && viewport.requestStatus === ImageViewport.RequestStatus.Loading
            && viewport.requestReason === ImageViewport.RequestReason.UploadPending
            && viewport.displayStatus === ImageViewport.DisplayStatus.Empty
            && viewport.frameCount === 1
            && viewport.totalDuration === 100
    }
}
)",
        QUrl());
    if (!component.isReady()) {
        qWarning() << component.errors();
        return false;
    }

    QVariantMap initialProperties;
    initialProperties.insert(
        QStringLiteral("suppliedFrame"), QVariant::fromValue<QObject*>(&frame));
    const std::unique_ptr<QObject> object(component.createWithInitialProperties(initialProperties));
    if (!object) {
        qWarning() << component.errors();
        return false;
    }

    return object->property("typedFactorySurfaceAvailable").toBool();
}

bool installedQmlSingletonTypesAreNotCreatable()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT));

    QQmlComponent factoryComponent(&engine);
    factoryComponent.setData("import ImageViewport 1.0\nImageSequenceFactory {}\n", QUrl());
    if (!factoryComponent.isError()) {
        return false;
    }

    QQmlComponent limitsComponent(&engine);
    limitsComponent.setData("import ImageViewport 1.0\nImageSequenceLimits {}\n", QUrl());
    return limitsComponent.isError();
}

bool installedQmlOpaqueTypesAreNotCreatable()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT));

    const QStringList typeNames = {
        QStringLiteral("ImageSequence"),
        QStringLiteral("ImageFrame"),
        QStringLiteral("ImageSequenceProviderFrameHandle"),
        QStringLiteral("ImageSequenceProviderAdapter"),
        QStringLiteral("ImageSequenceFactoryResult"),
    };

    for (const QString& typeName : typeNames) {
        QQmlComponent component(&engine);
        component.setData(
            QStringLiteral("import ImageViewport 1.0\n%1 {}\n").arg(typeName).toUtf8(), QUrl());
        if (!component.isError()) {
            return false;
        }
    }

    return true;
}

bool canUseInstalledQmlCommandSurface()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    width: 10
    height: 10

    property bool commandSurfaceAvailable: false

    function nearlyEqual(left, right) {
        return Math.abs(left - right) < 0.000001
    }

    Component.onCompleted: {
        const playOutcome = play()
        const pauseOutcome = pause()
        const stopOutcome = stop()
        const seekOutcome = seek(0)
        const positionSeekOutcome = seekToPosition(0)
        const zoomOutcome = setZoomPercent(200, Qt.point(5, 5))
        const stepOutcome = zoomByStep(1, Qt.point(5, 5))
        const resetViewOutcome = resetView()
        const minimum = minimumManualZoomPercent
        const maximum = maximumManualZoomPercent
        commandSurfaceAvailable = requestStatus === ImageViewport.RequestStatus.NoRequest
            && requestReason === ImageViewport.RequestReason.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && playbackPhase === ImageViewport.PlaybackPhase.Stopped
            && playOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && pauseOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && stopOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && seekOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && positionSeekOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && zoomOutcome === ImageViewport.CommandOutcome.Accepted
            && stepOutcome === ImageViewport.CommandOutcome.Accepted
            && resetViewOutcome === ImageViewport.CommandOutcome.Accepted
            && commandReason === ImageViewport.CommandReason.NoCommand
            && commandRevision.valid
            && fitMode === ImageViewport.FitMode.Contain
            && zoomPercent === 100
            && minimum > 0
            && maximum === ImageViewportDisplayLimits.maximumManualZoomPercent
            && manualZoomStepFactor === 1.25
            && clampedManualZoomPercent(-1) === minimum
            && clampedManualZoomPercent(maximum + 1) === maximum
            && nearlyEqual(steppedManualZoomPercent(1), 125)
            && contentPosition.x === 0
            && contentPosition.y === 0
            && frameSeekBounds.minimum === -1
            && frameSeekBounds.maximum === -1
            && positionSeekBounds.minimum === -1
            && positionSeekBounds.maximum === -1
            && itemToImage(1, 1).valid === false
            && imageToItem(1, 1).x === 0
            && nearestVisibleImagePoint(1, 1).valid === false
            && nearestVisibleSpreadPoint(1, 1).valid === false
            && nearestVisiblePagePoint(ImageViewport.PageRole.Primary, 1, 1).valid === false
            && containsVisibleImagePoint(1, 1) === false
    }
}
)",
        QUrl());
    if (!component.isReady()) {
        qWarning() << component.errors();
        return false;
    }

    const std::unique_ptr<QObject> object(component.create());
    if (!object) {
        qWarning() << component.errors();
        return false;
    }

    return object->property("commandSurfaceAvailable").toBool();
}

bool canUseInstalledProviderSessionSurface()
{
    ConsumerSession session;
    const ImageSequenceProviderRequestToken token = makeInstalledProviderRequestToken();
    if (!token.isValid()) {
        return false;
    }
    bool progressReceived = false;
    bool endReceived = false;
    bool cancellationReceived = false;
    QObject::connect(&session, &ImageSequenceProviderSession::providerProgress,
        [&progressReceived, &token](const ImageSequenceProviderRequestToken& receivedToken,
            double progress) { progressReceived = receivedToken == token && progress == 0.5; });
    QObject::connect(&session, &ImageSequenceProviderSession::endOfSequence,
        [&endReceived, &token](const ImageSequenceProviderRequestToken& receivedToken) {
            endReceived = receivedToken == token;
        });
    QObject::connect(&session, &ImageSequenceProviderSession::providerCancelled,
        [&cancellationReceived, &token](
            const ImageSequenceProviderRequestToken& receivedToken, const QString& diagnostic) {
            cancellationReceived
                = receivedToken == token && diagnostic == QStringLiteral("cancelled");
        });

    session.requestPlayback(token, 1, 100);
    session.cancelRequest(token);
    session.close();

    return progressReceived && endReceived && cancellationReceived
        && session.lastPlaybackFrame() == 1 && session.lastPlaybackPosition() == 100
        && session.lastCancelledToken() == token && session.closed();
}

bool canUseInstalledProviderPlaybackFallbackSurface()
{
    DefaultPlaybackFallbackSession session;
    const ImageSequenceProviderRequestToken token = makeInstalledProviderRequestToken();
    if (!token.isValid()) {
        return false;
    }

    session.requestPlayback(token, 3, 250);

    return session.frameRequestCount() == 1 && session.lastFrameToken() == token
        && session.lastFrame() == 3;
}

bool canUseInstalledProviderBorrowedRawFrameSignalSurface()
{
    ConsumerSession session;
    const ImageSequenceProviderRequestToken token = makeInstalledProviderRequestToken();
    if (!token.isValid()) {
        return false;
    }
    QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    const ImageSequenceProviderFrameMetadata metadata
        = ImageSequenceProviderFrameMetadata::timedFrame(1, 100, 100);
    bool rawFrameReceived = false;
    bool rawFrameWithMetadataReceived = false;

    QObject::connect(&session, &ImageSequenceProviderSession::imageFrameReady,
        [&rawFrameReceived, &token, &frame](
            const ImageSequenceProviderRequestToken& receivedToken, ImageFrame* receivedFrame) {
            rawFrameReceived = receivedToken == token && receivedFrame == &frame;
        });
    QObject::connect(&session, &ImageSequenceProviderSession::imageFrameWithMetadataReady,
        [&rawFrameWithMetadataReceived, &token, &frame, &metadata](
            const ImageSequenceProviderRequestToken& receivedToken, ImageFrame* receivedFrame,
            const ImageSequenceProviderFrameMetadata& receivedMetadata) {
            rawFrameWithMetadataReceived = receivedToken == token && receivedFrame == &frame
                && receivedMetadata.isTimedFrame() && receivedMetadata.frame() == metadata.frame()
                && receivedMetadata.frameStartPosition() == metadata.frameStartPosition()
                && receivedMetadata.frameDuration() == metadata.frameDuration();
        });

    emit session.imageFrameReady(token, &frame);
    emit session.imageFrameWithMetadataReady(token, &frame, metadata);

    return rawFrameReceived && rawFrameWithMetadataReceived;
}
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);

    [[maybe_unused]] const auto knownTrue
        = ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue;
    [[maybe_unused]] const auto knownFalse
        = ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse;
    ImageSequenceProviderRequestToken defaultToken;
    if (defaultToken.isValid() || defaultToken != ImageSequenceProviderRequestToken()) {
        return 1;
    }
    const ImageSequenceProviderRequestToken token = makeInstalledProviderRequestToken();
    if (!token.isValid() || token == defaultToken) {
        return 1;
    }

    const ImageSequenceProviderMetadata metadata
        = ImageSequenceProviderMetadata::timedFrameList(QSizeF(2.0, 2.0), { 100, 200 });
    if (!metadata.isSpecified() || !metadata.isValid() || metadata.isStill()
        || !metadata.isTimedFrameList()) {
        return 1;
    }
    if (metadata.logicalSize() != QSizeF(2.0, 2.0)
        || metadata.frameDurations() != QVector<int>({ 100, 200 })) {
        return 1;
    }

    const ImageSequenceProviderMetadata fixedMetadata
        = ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(2.0, 2.0), 3, 50);
    if (!fixedMetadata.isSpecified() || !fixedMetadata.isValid() || fixedMetadata.isStill()
        || !fixedMetadata.isTimedFrameList()) {
        return 1;
    }
    if (fixedMetadata.logicalSize() != QSizeF(2.0, 2.0)
        || fixedMetadata.frameDurations() != QVector<int>({ 50, 50, 50 })) {
        return 1;
    }

    const ImageSequenceProviderMetadata stillMetadata
        = ImageSequenceProviderMetadata::still(QSizeF(2.0, 2.0));
    if (!stillMetadata.isSpecified() || !stillMetadata.isValid() || !stillMetadata.isStill()
        || stillMetadata.isTimedFrameList()) {
        return 1;
    }
    if (stillMetadata.logicalSize() != QSizeF(2.0, 2.0)
        || !stillMetadata.frameDurations().isEmpty()) {
        return 1;
    }

    const ImageSequenceProviderFrameMetadata frameMetadata
        = ImageSequenceProviderFrameMetadata::timedFrame(1, 100, 200);
    if (!frameMetadata.isValid() || !frameMetadata.isTimedFrame() || frameMetadata.frame() != 1
        || frameMetadata.frameStartPosition() != 100 || frameMetadata.frameDuration() != 200) {
        return 1;
    }

    const ImageSequenceProviderFrameMetadata stillFrameMetadata
        = ImageSequenceProviderFrameMetadata::stillFrame();
    if (!stillFrameMetadata.isValid() || !stillFrameMetadata.isStillFrame()
        || stillFrameMetadata.isTimedFrame() || stillFrameMetadata.frame() != 0
        || stillFrameMetadata.frameStartPosition() != -1
        || stillFrameMetadata.frameDuration() != -1) {
        return 1;
    }

    ImageSequenceFactory factory;
    const ImageFrame emptyFrame;
    if (emptyFrame.isValid() || emptyFrame.logicalSize() != QSizeF()
        || emptyFrame.payloadByteSize() != 0 || emptyFrame.hasAlphaChannel()
        || emptyFrame.orientationPolicy() != ImageFrame::OrientationPolicy::Identity) {
        return 1;
    }

    QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const ImageFrame imageFrame(image);
    if (!imageFrame.isValid() || imageFrame.logicalSize() != QSizeF(2.0, 2.0)
        || imageFrame.payloadByteSize() <= 0 || !imageFrame.hasAlphaChannel()
        || imageFrame.orientationPolicy() != ImageFrame::OrientationPolicy::Identity) {
        return 1;
    }
    const ImageSequenceProviderFrameEnvelope exactEnvelope = imageFrame.envelope();
    if (!exactEnvelope.isValid() || exactEnvelope.sourceLogicalSize() != QSizeF(2.0, 2.0)
        || exactEnvelope.payloadRasterSize() != QSizeF(2.0, 2.0)
        || exactEnvelope.sourceToPayloadScale() != QSizeF(1.0, 1.0)
        || exactEnvelope.quality() != ImageViewport::PayloadQuality::Exact
        || exactEnvelope.exactness() != ImageViewport::PayloadExactness::ExactForSource) {
        return 1;
    }

    ImageSequenceProviderFrameEnvelope previewEnvelope;
    previewEnvelope.setSourceLogicalSize(QSizeF(4.0, 4.0));
    previewEnvelope.setPayloadRasterSize(QSizeF(2.0, 2.0));
    previewEnvelope.setSourceToPayloadScale(QSizeF(0.5, 0.5));
    previewEnvelope.setPayloadByteSize(image.sizeInBytes());
    previewEnvelope.setQuality(ImageViewport::PayloadQuality::Preview);
    previewEnvelope.setExactness(ImageViewport::PayloadExactness::NotExact);
    previewEnvelope.setFrame(0);
    previewEnvelope.setFrameStartPosition(-1);
    previewEnvelope.setFrameDuration(-1);
    previewEnvelope.setHasAlpha(true);
    const ImageFrame previewFrame(image, previewEnvelope);
    if (!previewEnvelope.isValid() || !previewFrame.isValid()
        || previewFrame.logicalSize() != QSizeF(4.0, 4.0)
        || previewFrame.payloadRasterSize() != QSizeF(2.0, 2.0)
        || previewFrame.sourceToPayloadScale() != QSizeF(0.5, 0.5)
        || previewFrame.envelope() != previewEnvelope) {
        return 1;
    }

    ImageSequenceProviderDisplayDemand installedDemand;
    installedDemand.setRole(ImageViewport::PageRole::Secondary);
    installedDemand.setResolvedFrame(1);
    installedDemand.setRequestedPosition(100);
    const ImageSequenceProviderRequest installedRequest = ImageSequenceProviderRequest::position(
        token, ImageViewport::PageRole::Secondary, 100, 1, installedDemand);
    if (!installedRequest.isValid()
        || installedRequest.kind() != ImageSequenceProviderRequestKind::Position
        || installedRequest.demand().role() != ImageViewport::PageRole::Secondary
        || installedRequest.demand().demandRevision().isValid()) {
        return 1;
    }
    auto installedHandleFrame = std::make_unique<ImageFrame>(image);
    ImageSequenceProviderFrameHandle installedHandle(std::move(installedHandleFrame));
    const ImageSequenceProviderEvent installedFrameEvent
        = ImageSequenceProviderEvent::frameReady(token, &installedHandle, exactEnvelope);
    if (!installedFrameEvent.isValid()
        || installedFrameEvent.kind() != ImageSequenceProviderEventKind::FrameReady
        || installedFrameEvent.frameHandle() != &installedHandle
        || installedFrameEvent.frameEnvelope() != exactEnvelope) {
        return 1;
    }
    const ImageSequenceProviderEvent unsupportedEvent = ImageSequenceProviderEvent::unsupported(
        token, ImageSequenceProviderUnsupportedCause::UnsupportedRequest,
        QStringLiteral("unsupported"));
    if (!unsupportedEvent.isValid()
        || unsupportedEvent.unsupportedCause()
            != ImageSequenceProviderUnsupportedCause::UnsupportedRequest) {
        return 1;
    }

    QImage deviceIndependentImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    deviceIndependentImage.setDevicePixelRatio(2.0);
    deviceIndependentImage.fill(Qt::transparent);
    const ImageFrame deviceIndependentFrame(deviceIndependentImage);
    if (!deviceIndependentFrame.isValid()
        || deviceIndependentFrame.logicalSize() != QSizeF(2.0, 1.0)
        || deviceIndependentFrame.payloadByteSize() != deviceIndependentImage.sizeInBytes()) {
        return 1;
    }

    std::unique_ptr<ImageSequenceFactoryResult> stillResult(factory.fromFrame(image));
    if (!stillResult || !stillResult->sequence()) {
        return 1;
    }
    if (stillResult->outcome() != ImageSequenceFactoryResult::FactoryOutcome::Created) {
        return 1;
    }

    std::unique_ptr<ImageSequenceFactoryResult> deviceIndependentStillResult(
        factory.fromFrame(deviceIndependentImage));
    if (!deviceIndependentStillResult || !deviceIndependentStillResult->sequence()) {
        return 1;
    }

    std::unique_ptr<ImageSequenceFactoryResult> invalidFrameResult(factory.fromFrame(QImage()));
    if (!invalidFrameResult || invalidFrameResult->sequence()
        || invalidFrameResult->outcome() != ImageSequenceFactoryResult::FactoryOutcome::Invalid
        || invalidFrameResult->errorString().isEmpty()
        || !invalidFrameResult->warningString().isEmpty()) {
        return 1;
    }

    QVector<QImage> timedImages;
    timedImages.append(image);
    timedImages.append(image);
    QVector<int> timedDurations { 100, 200 };
    std::unique_ptr<ImageSequenceFactoryResult> timedResult(
        factory.fromTimedFrameList(timedImages, timedDurations));
    if (!timedResult || !timedResult->sequence()) {
        return 1;
    }
    if (timedResult->outcome() != ImageSequenceFactoryResult::FactoryOutcome::Created) {
        return 1;
    }

    ConsumerAdapter consumerAdapter;
    const ImageSequenceProviderDescriptor consumerDescriptor = consumerAdapter.descriptor();
    if (!consumerDescriptor.isValid() || !consumerDescriptor.knownMetadata().isTimedFrameList()
        || consumerDescriptor.timedPlaybackCapability()
            != ImageSequenceProviderCapabilitySupport::KnownTrue) {
        return 1;
    }

    ImageViewport typedPageSetViewport;
    const ImageViewportPageSet installedSpread(stillResult->sequence(), timedResult->sequence());
    if (!installedSpread.isValid() || installedSpread.isClear()
        || installedSpread.primary() != stillResult->sequence()
        || installedSpread.secondary() != timedResult->sequence()) {
        return 1;
    }
    if (typedPageSetViewport.setPageSet(installedSpread)
        != ImageViewport::CommandOutcome::Accepted) {
        return 1;
    }
    if (typedPageSetViewport.primarySequence() != stillResult->sequence()
        || typedPageSetViewport.secondarySequence() != timedResult->sequence()) {
        return 1;
    }
    PageSetTransitionPolicy typedPageSetPolicy;
    typedPageSetPolicy.setPageGapTransition(
        PageSetTransitionPolicy::PageGapTransition::SetExplicit);
    typedPageSetPolicy.setPageGap(2.0);
    if (typedPageSetViewport.setPageSet(
            ImageViewportPageSet(deviceIndependentStillResult->sequence()), typedPageSetPolicy)
        != ImageViewport::CommandOutcome::Accepted) {
        return 1;
    }
    if (typedPageSetViewport.primarySequence() != deviceIndependentStillResult->sequence()
        || typedPageSetViewport.secondarySequence() != nullptr
        || typedPageSetViewport.pageGap() != 2.0) {
        return 1;
    }
    ImageViewportPageSet installedSecondaryOnly;
    installedSecondaryOnly.setSecondary(timedResult->sequence());
    if (installedSecondaryOnly.isValid()
        || typedPageSetViewport.setPageSet(installedSecondaryOnly)
            != ImageViewport::CommandOutcome::Invalid
        || typedPageSetViewport.primarySequence() != deviceIndependentStillResult->sequence()) {
        return 1;
    }

    ImageViewport helperViewport;
    const auto nearlyEqual
        = [](double left, double right) { return std::abs(left - right) < 0.000001; };
    const double minimumManualZoom = helperViewport.minimumManualZoomPercent();
    const double maximumManualZoom = helperViewport.maximumManualZoomPercent();
    if (minimumManualZoom <= 0.0
        || maximumManualZoom != ImageViewportDisplayLimits::maximumManualZoomPercent()
        || helperViewport.manualZoomStepFactor() != 1.25
        || helperViewport.clampedManualZoomPercent(-1.0) != minimumManualZoom
        || helperViewport.clampedManualZoomPercent(maximumManualZoom + 1.0) != maximumManualZoom
        || !nearlyEqual(helperViewport.steppedManualZoomPercent(1), 125.0)
        || helperViewport.nearestVisibleSpreadPoint(1.0, 1.0).isValid()
        || helperViewport.nearestVisiblePagePoint(ImageViewport::PageRole::Primary, 1.0, 1.0)
            .isValid()
        || helperViewport.nearestVisibleImagePoint(1.0, 1.0).isValid()) {
        return 1;
    }

    ImageViewport steppedCommandViewport;
    if (steppedCommandViewport.zoomByStep(1, QPointF()) != ImageViewport::CommandOutcome::Accepted
        || !nearlyEqual(steppedCommandViewport.zoomPercent(), 125.0)) {
        return 1;
    }

    ImageViewport snapshotViewport;
    const ImageViewportStateSnapshot snapshot = snapshotViewport.state();
    const ImageViewportStateSnapshot snapshotCopy = snapshot;
    if (snapshotCopy != snapshot
        || snapshot.request().status() != ImageViewport::RequestStatus::NoRequest
        || snapshot.request().reason() != ImageViewport::RequestReason::NoRequest
        || snapshot.display().status() != ImageViewport::DisplayStatus::Empty
        || snapshot.display().phase() != ImageViewport::DisplayPhase::NoPresentation
        || snapshot.primary().present() || snapshot.secondary().present()
        || snapshot.diagnostics().commandReason() != ImageViewport::CommandReason::NoCommand
        || snapshot.revisions().request().isValid()
        || snapshot.revisions().display() != ImageViewportRevisionToken()
        || ImageViewportPageSetGenerationToken().isValid()
        || ImageViewportDemandRevisionToken().isValid()) {
        return 1;
    }
    const ImageViewportRoleSet installedRoleSet(true, false);
    if (!installedRoleSet.primary() || installedRoleSet.secondary()) {
        return 1;
    }
    const ImageViewportCommandResult installedCommandResult;
    if (installedCommandResult.outcome() != ImageViewport::CommandOutcome::Accepted
        || installedCommandResult.reason() != ImageViewport::CommandReason::NoCommand
        || installedCommandResult.commandRevision().isValid()
        || installedCommandResult.snapshotRevision().isValid()) {
        return 1;
    }
    ImageViewportPresentationCommand installedPresentationCommand;
    installedPresentationCommand.setManualZoomPercent(125.0);
    installedPresentationCommand.setPageGap(3.0);
    if (!installedPresentationCommand.hasManualZoomPercent()
        || !installedPresentationCommand.hasPageGap()
        || helperViewport.setPresentation(installedPresentationCommand)
            != ImageViewport::CommandOutcome::Accepted
        || helperViewport.fitMode() != ImageViewport::FitMode::Manual
        || !nearlyEqual(helperViewport.zoomPercent(), 125.0) || helperViewport.pageGap() != 3.0) {
        return 1;
    }
    ImageViewportCoordinateInput installedCoordinateInput;
    installedCoordinateInput.setSourceSpace(ImageViewport::CoordinateSpace::Item);
    installedCoordinateInput.setTargetSpace(ImageViewport::CoordinateSpace::Page);
    installedCoordinateInput.setPageRole(QVariant::fromValue(ImageViewport::PageRole::Primary));
    installedCoordinateInput.setPoint(QPointF(1.0, 2.0));
    const ImageViewportCoordinateResult installedCoordinateResult(false, QPointF(),
        installedCoordinateInput.sourceSpace(), installedCoordinateInput.targetSpace(),
        installedCoordinateInput.pageRole());
    if (installedCoordinateInput.pageRole().value<ImageViewport::PageRole>()
            != ImageViewport::PageRole::Primary
        || installedCoordinateResult.isValid()
        || installedCoordinateResult.targetSpace() != ImageViewport::CoordinateSpace::Page
        || helperViewport.mapPoint(installedCoordinateInput).isValid()
        || helperViewport.containsPoint(installedCoordinateInput)
        || helperViewport.nearestVisiblePoint(installedCoordinateInput).isValid()) {
        return 1;
    }

    const PageGeometry installedPrimaryGeometry = typedPageSetViewport.primaryPageGeometry();
    const PageGeometry installedSecondaryGeometry = typedPageSetViewport.secondaryPageGeometry();
    if (installedPrimaryGeometry.role() != ImageViewport::PageRole::Primary
        || installedPrimaryGeometry.isAvailable() || installedPrimaryGeometry.pageRect() != QRectF()
        || installedPrimaryGeometry.itemRect() != QRectF()
        || installedPrimaryGeometry.visiblePageRect() != QRectF()) {
        return 1;
    }
    if (installedSecondaryGeometry.role() != ImageViewport::PageRole::Secondary
        || installedSecondaryGeometry.isAvailable()
        || installedSecondaryGeometry.pageRect() != QRectF()
        || installedSecondaryGeometry.itemRect() != QRectF()
        || installedSecondaryGeometry.visiblePageRect() != QRectF()
        || typedPageSetViewport.pageGeometry(ImageViewport::PageRole::Secondary)
            != installedSecondaryGeometry) {
        return 1;
    }

    QVector<QImage> deviceIndependentTimedImages;
    deviceIndependentTimedImages.append(deviceIndependentImage);
    deviceIndependentTimedImages.append(deviceIndependentImage);
    std::unique_ptr<ImageSequenceFactoryResult> deviceIndependentTimedResult(
        factory.fromTimedFrameList(deviceIndependentTimedImages, timedDurations));
    if (!deviceIndependentTimedResult || !deviceIndependentTimedResult->sequence()) {
        return 1;
    }

    std::unique_ptr<ImageSequenceFactoryResult> mismatchedTimedResult(
        factory.fromTimedFrameList(timedImages, { 100 }));
    if (!mismatchedTimedResult || mismatchedTimedResult->sequence()
        || mismatchedTimedResult->outcome() != ImageSequenceFactoryResult::FactoryOutcome::Invalid
        || !mismatchedTimedResult->errorString().contains(QStringLiteral("same count"))
        || !mismatchedTimedResult->warningString().isEmpty()) {
        return 1;
    }

    TimedImageFrameList builder;
    if (!builder.appendFrame(image, 100)) {
        return 1;
    }
    if (builder.count() != 1) {
        return 1;
    }
    if (builder.appendFrame(image, 0) || builder.count() != 1
        || !builder.errorString().contains(QStringLiteral("positive"))) {
        return 1;
    }
    builder.clear();
    if (builder.count() != 0 || !builder.errorString().isEmpty()
        || !builder.warningString().isEmpty()) {
        return 1;
    }

    ConsumerAdapter adapter;
    std::unique_ptr<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    if (!result || !result->sequence()) {
        return 1;
    }
    if (result->outcome() != ImageSequenceFactoryResult::FactoryOutcome::Created) {
        return 1;
    }

    ImageViewport providerViewport;
    providerViewport.setSequence(result->sequence());
    if (providerViewport.requestStatus() != ImageViewport::RequestStatus::Loading
        || providerViewport.requestReason() != ImageViewport::RequestReason::ProviderWaiting
        || providerViewport.requestedFrame() != 0 || providerViewport.requestedPosition() != 0
        || providerViewport.frameCount() != 2 || providerViewport.totalDuration() != 200) {
        return 1;
    }

    QCoreApplication::processEvents();
    if (providerViewport.requestStatus() != ImageViewport::RequestStatus::Loading
        || providerViewport.requestReason() != ImageViewport::RequestReason::RenderWaiting
        || providerViewport.displayStatus() != ImageViewport::DisplayStatus::Empty
        || providerViewport.displayedFrame() != -1) {
        return 1;
    }

    return canCreateInstalledQmlViewport() && canReadInstalledQmlLimits()
            && canUseInstalledQmlFactorySurface() && canUseInstalledQmlTypedFactorySurface()
            && installedQmlSingletonTypesAreNotCreatable()
            && installedQmlOpaqueTypesAreNotCreatable() && canUseInstalledQmlCommandSurface()
            && canUseInstalledProviderSessionSurface()
            && canUseInstalledProviderPlaybackFallbackSurface()
            && canUseInstalledProviderBorrowedRawFrameSignalSurface()
        ? 0
        : 1;
}
