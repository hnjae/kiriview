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

    void request(const ImageSequenceProviderRequest& request) override
    {
        switch (request.kind()) {
        case ImageSequenceProviderRequestKind::Metadata:
            emit providerEvent(ImageSequenceProviderEvent::metadataReady(
                request.token(), ImageSequenceProviderMetadata::still(QSizeF(2.0, 2.0))));
            break;
        case ImageSequenceProviderRequestKind::Frame: {
            QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            auto payload = std::make_unique<ImageFrame>(image);
            ImageSequenceProviderFrameEnvelope envelope = payload->envelope();
            envelope.setFrame(request.frame());
            envelope.setFrameStartPosition(0);
            envelope.setFrameDuration(100);
            emit providerEvent(ImageSequenceProviderEvent::frameReady(request.token(),
                new ImageSequenceProviderFrameHandle(std::move(payload)), envelope));
            break;
        }
        case ImageSequenceProviderRequestKind::Playback:
            m_lastPlaybackFrame = request.frame();
            m_lastPlaybackPosition = request.requestedPosition();
            emit providerEvent(ImageSequenceProviderEvent::progress(request.token(), 0.5));
            emit providerEvent(ImageSequenceProviderEvent::endOfSequence(request.token()));
            break;
        case ImageSequenceProviderRequestKind::Cancel:
            for (ImageSequenceProviderRequestToken token : request.tokens()) {
                m_lastCancelledToken = token;
                emit providerEvent(
                    ImageSequenceProviderEvent::cancelled(token, QStringLiteral("cancelled")));
            }
            break;
        case ImageSequenceProviderRequestKind::Close:
            m_closed = true;
            break;
        default:
            break;
        }
    }

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

    void request(const ImageSequenceProviderRequest& request) override
    {
        if (request.kind() == ImageSequenceProviderRequestKind::Frame
            || request.kind() == ImageSequenceProviderRequestKind::Playback
            || request.kind() == ImageSequenceProviderRequestKind::Position) {
            m_lastFrameToken = request.token();
            m_lastFrame = request.resolvedFrame();
            ++m_frameRequestCount;
        }
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
    ImageSequenceProviderDescriptor descriptor() const override
    {
        const ImageSequenceProviderMetadata metadata
            = ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(2.0, 2.0), 2, 100);
        ImageSequenceProviderDescriptor descriptor;
        descriptor.setSessionFactory(std::make_shared<ConsumerSessionFactory>());
        descriptor.setKnownMetadata(metadata);
        descriptor.setKnownFacts(ImageSequenceProviderKnownFacts::timedFrameList(
            metadata.logicalSize(), metadata.frameDurations()));
        descriptor.setTimedPlaybackCapability(CapabilitySupport::KnownTrue);
        descriptor.setFrameSeekCapability(CapabilitySupport::KnownTrue);
        descriptor.setPositionSeekCapability(CapabilitySupport::KnownTrue);
        return descriptor;
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

    void request(const ImageSequenceProviderRequest& request) override
    {
        if (request.kind() == ImageSequenceProviderRequestKind::Metadata) {
            *m_capturedToken = request.token();
            emit providerEvent(ImageSequenceProviderEvent::metadataReady(
                request.token(), ImageSequenceProviderMetadata::still(QSizeF(2.0, 2.0))));
        }
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

    ImageSequenceProviderDescriptor descriptor() const override
    {
        ImageSequenceProviderDescriptor descriptor;
        descriptor.setSessionFactory(std::make_shared<TokenCaptureSessionFactory>(m_capturedToken));
        return descriptor;
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
    viewport.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
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
    property imageViewportPresentationTarget presentationTarget
    property presentationTargetTransitionPolicy policy
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
        presentationTarget.primary = timedResult.sequence
        viewport.setPresentationTarget(presentationTarget, policy)
        typedFactorySurfaceAvailable = frameResult.sequence !== null
            && frameResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Created
            && appendAccepted === true
            && list.count === 1
            && timedResult.sequence !== null
            && timedResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Created
            && viewport.state.request.status === ImageViewport.RequestStatus.Loading
            && viewport.state.request.reason === ImageViewport.RequestReason.UploadPending
            && viewport.state.display.status === ImageViewport.DisplayStatus.Empty
            && viewport.state.primary.metadata.frameCount === 1
            && viewport.state.primary.metadata.totalDuration === 100
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
    property imageViewportPresentationCommand zoomCommand
    property imageViewportPresentationCommand zoomStepCommand
    property imageViewportCoordinateInput coordinateInput

    function nearlyEqual(left, right) {
        return Math.abs(left - right) < 0.000001
    }

    Component.onCompleted: {
        const playOutcome = play(ImageViewport.PageRole.Primary).outcome
        const pauseOutcome = pause(ImageViewport.PageRole.Primary).outcome
        const stopOutcome = stop(ImageViewport.PageRole.Primary).outcome
        const seekOutcome = seek(ImageViewport.PageRole.Primary, 0).outcome
        const positionSeekOutcome = seekToPosition(ImageViewport.PageRole.Primary, 0).outcome
        zoomCommand.manualZoomPercent = 200
        const zoomOutcome = setPresentation(zoomCommand).outcome
        zoomStepCommand.zoomStepDelta = 1
        const stepOutcome = setPresentation(zoomStepCommand).outcome
        const resetViewOutcome = resetView().outcome
        const minimum = state.presentation.minimumManualZoomPercent
        const maximum = state.presentation.maximumManualZoomPercent
        commandSurfaceAvailable = state.request.status === ImageViewport.RequestStatus.NoRequest
            && state.request.reason === ImageViewport.RequestReason.NoRequest
            && state.display.status === ImageViewport.DisplayStatus.Empty
            && state.request.playbackPhase === ImageViewport.PlaybackPhase.Stopped
            && playOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && pauseOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && stopOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && seekOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && positionSeekOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && zoomOutcome === ImageViewport.CommandOutcome.Accepted
            && stepOutcome === ImageViewport.CommandOutcome.Accepted
            && resetViewOutcome === ImageViewport.CommandOutcome.Accepted
            && state.diagnostics.commandReason === ImageViewport.CommandReason.NoCommand
            && state.revisions.command.valid
            && state.presentation.fitMode === ImageViewport.FitMode.Contain
            && state.presentation.zoomPercent === 100
            && minimum > 0
            && maximum === ImageViewportDisplayLimits.maximumManualZoomPercent
            && state.presentation.manualZoomStepFactor === 1.25
            && typeof clampedManualZoomPercent === "undefined"
            && typeof steppedManualZoomPercent === "undefined"
            && typeof zoomByStep === "undefined"
            && typeof setZoomPercent === "undefined"
            && typeof fitMode === "undefined"
            && typeof zoomPercent === "undefined"
            && typeof minimumManualZoomPercent === "undefined"
            && typeof maximumManualZoomPercent === "undefined"
            && typeof manualZoomStepFactor === "undefined"
            && state.display.contentPosition.x === 0
            && state.display.contentPosition.y === 0
            && state.primary.metadata.frameSeekBounds.minimum === -1
            && state.primary.metadata.frameSeekBounds.maximum === -1
            && state.primary.metadata.positionSeekBounds.minimum === -1
            && state.primary.metadata.positionSeekBounds.maximum === -1
            && mapPoint(coordinateInput).valid === false
            && mapPoint(coordinateInput).point.x === 0
            && containsPoint(coordinateInput) === false
            && typeof nearestVisiblePoint === "undefined"
            && typeof itemToImage === "undefined"
            && typeof imageToItem === "undefined"
            && typeof nearestVisibleImagePoint === "undefined"
            && typeof containsVisibleImagePoint === "undefined"
            && typeof itemToSpread === "undefined"
            && typeof spreadToItem === "undefined"
            && typeof itemToPage === "undefined"
            && typeof pageToItem === "undefined"
            && typeof nearestVisibleSpreadPoint === "undefined"
            && typeof nearestVisiblePagePoint === "undefined"
            && typeof containsVisibleSpreadPoint === "undefined"
            && typeof containsVisiblePagePoint === "undefined"
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
    QObject::connect(&session, &ImageSequenceProviderSession::providerEvent,
        [&progressReceived, &endReceived, &cancellationReceived, &token](
            const ImageSequenceProviderEvent& event) {
            if (event.kind() == ImageSequenceProviderEventKind::Progress) {
                progressReceived = event.token() == token && event.progress() == 0.5;
            } else if (event.kind() == ImageSequenceProviderEventKind::EndOfSequence) {
                endReceived = event.token() == token;
            } else if (event.kind() == ImageSequenceProviderEventKind::Cancelled) {
                cancellationReceived
                    = event.token() == token && event.diagnostic() == QStringLiteral("cancelled");
            }
        });

    session.request(ImageSequenceProviderRequest::playback(
        token, ImageViewport::PageRole::Primary, 1, 100, {}));
    session.request(ImageSequenceProviderRequest::cancel({ token }));
    session.request(ImageSequenceProviderRequest::close());

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

    session.request(ImageSequenceProviderRequest::playback(
        token, ImageViewport::PageRole::Primary, 3, 250, {}));

    return session.frameRequestCount() == 1 && session.lastFrameToken() == token
        && session.lastFrame() == 3;
}

bool canUseInstalledProviderTypedEventSurface()
{
    ConsumerSession session;
    const ImageSequenceProviderRequestToken token = makeInstalledProviderRequestToken();
    if (!token.isValid()) {
        return false;
    }
    QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    ImageSequenceProviderFrameEnvelope envelope = frame.envelope();
    envelope.setFrame(1);
    envelope.setFrameStartPosition(100);
    envelope.setFrameDuration(100);
    bool frameEventReceived = false;

    QObject::connect(&session, &ImageSequenceProviderSession::providerEvent,
        [&frameEventReceived, &token, &frame, &envelope](const ImageSequenceProviderEvent& event) {
            frameEventReceived = event.kind() == ImageSequenceProviderEventKind::FrameReady
                && event.token() == token && event.frameHandle()
                && event.frameHandle()->frame() == &frame && event.frameEnvelope() == envelope;
        });

    auto handle = std::make_unique<ImageSequenceProviderFrameHandle>(&frame, [](ImageFrame*) { });
    emit session.providerEvent(
        ImageSequenceProviderEvent::frameReady(token, handle.get(), envelope));

    return frameEventReceived;
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

    ImageViewport typedPresentationTargetViewport;
    const ImageViewportPresentationTarget installedSpread(
        stillResult->sequence(), timedResult->sequence());
    if (!installedSpread.isValid() || installedSpread.isClear()
        || installedSpread.primary() != stillResult->sequence()
        || installedSpread.secondary() != timedResult->sequence()) {
        return 1;
    }
    if (typedPresentationTargetViewport
            .setPresentationTarget(installedSpread, PresentationTargetTransitionPolicy {})
            .outcome()
        != ImageViewport::CommandOutcome::Accepted) {
        return 1;
    }
    if (typedPresentationTargetViewport.state().primary().sequence() != stillResult->sequence()
        || typedPresentationTargetViewport.state().secondary().sequence()
            != timedResult->sequence()) {
        return 1;
    }
    PresentationTargetTransitionPolicy typedPresentationTargetPolicy;
    typedPresentationTargetPolicy.setPageGapTransition(
        PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    typedPresentationTargetPolicy.setPageGap(2.0);
    if (typedPresentationTargetViewport
            .setPresentationTarget(
                ImageViewportPresentationTarget(deviceIndependentStillResult->sequence()),
                typedPresentationTargetPolicy)
            .outcome()
        != ImageViewport::CommandOutcome::Accepted) {
        return 1;
    }
    if (typedPresentationTargetViewport.state().primary().sequence()
            != deviceIndependentStillResult->sequence()
        || typedPresentationTargetViewport.state().secondary().sequence() != nullptr
        || typedPresentationTargetViewport.state().presentation().pageGap() != 2.0) {
        return 1;
    }
    ImageViewportPresentationTarget installedSecondaryOnly;
    installedSecondaryOnly.setSecondary(timedResult->sequence());
    if (installedSecondaryOnly.isValid()
        || typedPresentationTargetViewport
                .setPresentationTarget(
                    installedSecondaryOnly, PresentationTargetTransitionPolicy {})
                .outcome()
            != ImageViewport::CommandOutcome::Invalid
        || typedPresentationTargetViewport.state().primary().sequence()
            != deviceIndependentStillResult->sequence()) {
        return 1;
    }

    ImageViewport helperViewport;
    const auto nearlyEqual
        = [](double left, double right) { return std::abs(left - right) < 0.000001; };
    const ImageViewportPresentationSnapshot helperPresentation
        = helperViewport.state().presentation();
    const double minimumManualZoom = helperPresentation.minimumManualZoomPercent();
    const double maximumManualZoom = helperPresentation.maximumManualZoomPercent();
    ImageViewportCoordinateInput primaryPageCoordinate;
    primaryPageCoordinate.setSourceSpace(ImageViewport::CoordinateSpace::DisplayedPage);
    primaryPageCoordinate.setTargetSpace(ImageViewport::CoordinateSpace::DisplayedPage);
    primaryPageCoordinate.setRole(QVariant::fromValue(ImageViewport::PageRole::Primary));
    primaryPageCoordinate.setPoint(QPointF(1.0, 1.0));
    if (minimumManualZoom <= 0.0
        || maximumManualZoom != ImageViewportDisplayLimits::maximumManualZoomPercent()
        || helperPresentation.manualZoomStepFactor() != 1.25
        || helperViewport.mapPoint(primaryPageCoordinate).isValid()
        || helperViewport.containsPoint(primaryPageCoordinate)) {
        return 1;
    }

    ImageViewport steppedCommandViewport;
    ImageViewportPresentationCommand stepCommand;
    stepCommand.setZoomStepDelta(1);
    if (steppedCommandViewport.setPresentation(stepCommand).outcome()
            != ImageViewport::CommandOutcome::Accepted
        || !nearlyEqual(steppedCommandViewport.state().presentation().zoomPercent(), 125.0)) {
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
        || ImageViewportPresentationTargetGenerationToken().isValid()
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
    installedPresentationCommand.setQualityPreference(
        ImageViewport::QualityPreference::ExactDetail);
    installedPresentationCommand.setExactnessPreference(
        ImageViewport::ExactnessPreference::RequireExact);
    if (!installedPresentationCommand.hasManualZoomPercent()
        || !installedPresentationCommand.hasPageGap()
        || !installedPresentationCommand.hasQualityPreference()
        || !installedPresentationCommand.hasExactnessPreference()
        || helperViewport.setPresentation(installedPresentationCommand).outcome()
            != ImageViewport::CommandOutcome::Accepted
        || helperViewport.state().presentation().fitMode() != ImageViewport::FitMode::Manual
        || !nearlyEqual(helperViewport.state().presentation().zoomPercent(), 125.0)
        || helperViewport.state().presentation().pageGap() != 3.0
        || helperViewport.state().presentation().qualityPreference()
            != ImageViewport::QualityPreference::ExactDetail
        || helperViewport.state().presentation().exactnessPreference()
            != ImageViewport::ExactnessPreference::RequireExact) {
        return 1;
    }
    ImageViewportCoordinateInput installedCoordinateInput;
    installedCoordinateInput.setSourceSpace(ImageViewport::CoordinateSpace::Item);
    installedCoordinateInput.setTargetSpace(ImageViewport::CoordinateSpace::DisplayedPage);
    installedCoordinateInput.setRole(QVariant::fromValue(ImageViewport::PageRole::Primary));
    installedCoordinateInput.setPoint(QPointF(1.0, 2.0));
    const ImageViewportCoordinateResult installedCoordinateResult(
        false, QPointF(), installedCoordinateInput.targetSpace(), installedCoordinateInput.role());
    if (installedCoordinateInput.role().value<ImageViewport::PageRole>()
            != ImageViewport::PageRole::Primary
        || installedCoordinateResult.isValid()
        || installedCoordinateResult.space() != ImageViewport::CoordinateSpace::DisplayedPage
        || installedCoordinateResult.role().value<ImageViewport::PageRole>()
            != ImageViewport::PageRole::Primary
        || helperViewport.mapPoint(installedCoordinateInput).isValid()
        || helperViewport.containsPoint(installedCoordinateInput)) {
        return 1;
    }

    const ImageViewportStateSnapshot installedPresentationTargetState
        = typedPresentationTargetViewport.state();
    const ImageViewportRoleGeometrySnapshot installedPrimaryGeometry
        = installedPresentationTargetState.primary().geometry();
    const ImageViewportRoleGeometrySnapshot installedSecondaryGeometry
        = installedPresentationTargetState.secondary().geometry();
    if (!installedPresentationTargetState.primary().present()
        || installedPresentationTargetState.secondary().present()
        || installedPrimaryGeometry.acceptedPageRect() != QRectF()
        || installedPrimaryGeometry.acceptedItemRect() != QRectF()
        || installedPrimaryGeometry.acceptedVisiblePageRect() != QRectF()
        || installedPrimaryGeometry.displayedPageRect() != QRectF()
        || installedPrimaryGeometry.displayedItemRect() != QRectF()
        || installedPrimaryGeometry.displayedVisiblePageRect() != QRectF()) {
        return 1;
    }
    if (installedSecondaryGeometry.acceptedPageRect() != QRectF()
        || installedSecondaryGeometry.acceptedItemRect() != QRectF()
        || installedSecondaryGeometry.acceptedVisiblePageRect() != QRectF()
        || installedSecondaryGeometry.displayedPageRect() != QRectF()
        || installedSecondaryGeometry.displayedItemRect() != QRectF()
        || installedSecondaryGeometry.displayedVisiblePageRect() != QRectF()) {
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
    providerViewport.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    if (providerViewport.state().request().status() != ImageViewport::RequestStatus::Loading
        || providerViewport.state().request().reason()
            != ImageViewport::RequestReason::ProviderWaiting
        || providerViewport.state().primary().request().frame() != 0
        || providerViewport.state().primary().request().position() != 0
        || providerViewport.state().primary().metadata().frameCount() != 2
        || providerViewport.state().primary().metadata().totalDuration() != 200) {
        return 1;
    }

    QCoreApplication::processEvents();
    if (providerViewport.state().request().status() != ImageViewport::RequestStatus::Loading
        || providerViewport.state().request().reason()
            != ImageViewport::RequestReason::RenderWaiting
        || providerViewport.state().display().status() != ImageViewport::DisplayStatus::Empty
        || providerViewport.state().primary().display().frame() != -1) {
        return 1;
    }

    return canCreateInstalledQmlViewport() && canReadInstalledQmlLimits()
            && canUseInstalledQmlFactorySurface() && canUseInstalledQmlTypedFactorySurface()
            && installedQmlSingletonTypesAreNotCreatable()
            && installedQmlOpaqueTypesAreNotCreatable() && canUseInstalledQmlCommandSurface()
            && canUseInstalledProviderSessionSurface()
            && canUseInstalledProviderPlaybackFallbackSurface()
            && canUseInstalledProviderTypedEventSurface()
        ? 0
        : 1;
}
