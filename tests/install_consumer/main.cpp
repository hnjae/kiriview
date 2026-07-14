#include <ImageViewport/ImageViewport>

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
            ImageSequenceProviderFrameEnvelope envelope;
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

class ConsumerSessionFactory final
{
public:
    ImageSequenceProviderSession* createSession(QObject* parent)
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
        auto factory = std::make_shared<ConsumerSessionFactory>();
        return ImageSequenceProviderDescriptor(
            metadata, ImageSequenceProviderThreadingContract::AffinityBound, [factory]() {
                return ImageSequenceProviderSessionFactoryResult::created(
                    factory->createSession(nullptr));
            });
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

class TokenCaptureSessionFactory final
{
public:
    explicit TokenCaptureSessionFactory(
        std::shared_ptr<ImageSequenceProviderRequestToken> capturedToken)
        : m_capturedToken(std::move(capturedToken))
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent)
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
        auto factory = std::make_shared<TokenCaptureSessionFactory>(m_capturedToken);
        return ImageSequenceProviderDescriptor(
            {}, ImageSequenceProviderThreadingContract::AffinityBound, [factory]() {
                return ImageSequenceProviderSessionFactoryResult::created(
                    factory->createSession(nullptr));
            });
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
    property var maximumSourceLogicalWidth: ImageSequenceLimits.maximumSourceLogicalWidth
    property var maximumSourceLogicalHeight: ImageSequenceLimits.maximumSourceLogicalHeight
    property var maximumSourceLogicalPixels: ImageSequenceLimits.maximumSourceLogicalPixels
    property var maximumPayloadRasterWidth: ImageSequenceLimits.maximumPayloadRasterWidth
    property var maximumPayloadRasterHeight: ImageSequenceLimits.maximumPayloadRasterHeight
    property var maximumPayloadBytes: ImageSequenceLimits.maximumPayloadBytes
    property var maximumFrameCount: ImageSequenceLimits.maximumFrameCount
    property var maximumFrameDurationMilliseconds: ImageSequenceLimits.maximumFrameDurationMilliseconds
    property var maximumTotalDurationMilliseconds: ImageSequenceLimits.maximumTotalDurationMilliseconds
    property var maximumDiagnosticCharacters: ImageSequenceLimits.maximumDiagnosticCharacters
    property var maximumFormatIdentifierCharacters: ImageSequenceLimits.maximumFormatIdentifierCharacters
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

    if (object->property("maximumSourceLogicalWidth").toInt()
            != ImageSequenceLimits::maximumSourceLogicalWidth()
        || object->property("maximumSourceLogicalHeight").toInt()
            != ImageSequenceLimits::maximumSourceLogicalHeight()
        || object->property("maximumSourceLogicalPixels").toLongLong()
            != ImageSequenceLimits::maximumSourceLogicalPixels()
        || object->property("maximumPayloadRasterWidth").toInt()
            != ImageSequenceLimits::maximumPayloadRasterWidth()
        || object->property("maximumPayloadRasterHeight").toInt()
            != ImageSequenceLimits::maximumPayloadRasterHeight()
        || object->property("maximumPayloadBytes").toLongLong()
            != ImageSequenceLimits::maximumPayloadBytes()
        || object->property("maximumFrameCount").toInt() != ImageSequenceLimits::maximumFrameCount()
        || object->property("maximumFrameDurationMilliseconds").toInt()
            != ImageSequenceLimits::maximumFrameDurationMilliseconds()
        || object->property("maximumTotalDurationMilliseconds").toInt()
            != ImageSequenceLimits::maximumTotalDurationMilliseconds()
        || object->property("maximumDiagnosticCharacters").toInt()
            != ImageSequenceLimits::maximumDiagnosticCharacters()
        || object->property("maximumFormatIdentifierCharacters").toInt()
            != ImageSequenceLimits::maximumFormatIdentifierCharacters()
        || object->property("maximumManualZoomPercent").toDouble()
            != ImageViewportDisplayLimits::maximumManualZoomPercent()) {
        return false;
    }

    return ImageSequenceLimits::maximumSourceLogicalWidth() >= 8192
        && ImageSequenceLimits::maximumSourceLogicalHeight() >= 8192
        && ImageSequenceLimits::maximumSourceLogicalPixels() >= 67108864LL
        && ImageSequenceLimits::maximumPayloadRasterWidth() >= 8192
        && ImageSequenceLimits::maximumPayloadRasterHeight() >= 8192
        && ImageSequenceLimits::maximumPayloadBytes() >= 268435456LL
        && ImageSequenceLimits::maximumFrameCount() >= 10000
        && ImageSequenceLimits::maximumFrameDurationMilliseconds() >= 86400000
        && ImageSequenceLimits::maximumTotalDurationMilliseconds() >= 86400000
        && ImageSequenceLimits::maximumDiagnosticCharacters() >= 4096
        && ImageSequenceLimits::maximumFormatIdentifierCharacters() > 0
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
        && frameResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Rejected
        && frameResult.reason === ImageSequenceFactoryResult.FactoryReason.InvalidFrame
        && frameResult.errorString.length > 0
        && listResult.sequence === null
        && listResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Rejected
        && listResult.reason === ImageSequenceFactoryResult.FactoryReason.InvalidTiming
        && listResult.errorString.length > 0
        && providerResult.sequence === null
        && providerResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Rejected
        && providerResult.reason === ImageSequenceFactoryResult.FactoryReason.InvalidProviderDescriptor
        && providerResult.errorString.length > 0
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

    session.request(
        ImageSequenceProviderRequest::playback(token, ImageViewportPageRole::Primary, 1, 100, {}));
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

    session.request(
        ImageSequenceProviderRequest::playback(token, ImageViewportPageRole::Primary, 3, 250, {}));

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
    ImageSequenceProviderFrameEnvelope envelope;
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

    [[maybe_unused]] const auto knownTrue = ImageViewportCapabilitySupport::True;
    [[maybe_unused]] const auto knownFalse = ImageViewportCapabilitySupport::False;
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
    if (metadata.sourceLogicalSize() != QSizeF(2.0, 2.0)
        || metadata.frameDurations() != QVector<int>({ 100, 200 })) {
        return 1;
    }

    const ImageSequenceProviderMetadata fixedMetadata
        = ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(2.0, 2.0), 3, 50);
    if (!fixedMetadata.isSpecified() || !fixedMetadata.isValid() || fixedMetadata.isStill()
        || !fixedMetadata.isTimedFrameList()) {
        return 1;
    }
    if (fixedMetadata.sourceLogicalSize() != QSizeF(2.0, 2.0)
        || fixedMetadata.frameDurations() != QVector<int>({ 50, 50, 50 })) {
        return 1;
    }

    const ImageSequenceProviderMetadata stillMetadata
        = ImageSequenceProviderMetadata::still(QSizeF(2.0, 2.0));
    if (!stillMetadata.isSpecified() || !stillMetadata.isValid() || !stillMetadata.isStill()
        || stillMetadata.isTimedFrameList()) {
        return 1;
    }
    if (stillMetadata.sourceLogicalSize() != QSizeF(2.0, 2.0)
        || !stillMetadata.frameDurations().isEmpty()) {
        return 1;
    }

    const ImageSequenceProviderFrameEnvelope frameMetadata
        = ImageSequenceProviderFrameEnvelope::timedFrame(1, 100, 200);
    if (!frameMetadata.isValid() || !frameMetadata.isTimedFrame() || frameMetadata.frame() != 1
        || frameMetadata.frameStartPosition() != 100 || frameMetadata.frameDuration() != 200) {
        return 1;
    }

    const ImageSequenceProviderFrameEnvelope stillFrameMetadata
        = ImageSequenceProviderFrameEnvelope::stillFrame();
    if (!stillFrameMetadata.isValid() || !stillFrameMetadata.isStillFrame()
        || stillFrameMetadata.isTimedFrame() || stillFrameMetadata.frame() != 0
        || stillFrameMetadata.frameStartPosition() != -1
        || stillFrameMetadata.frameDuration() != -1) {
        return 1;
    }

    ImageSequenceFactory factory;
    const ImageFrame emptyFrame;
    if (emptyFrame.isValid() || emptyFrame.sourceLogicalSize() != QSizeF()
        || emptyFrame.payloadByteSize() != 0 || emptyFrame.hasAlpha()
        || emptyFrame.orientationPolicy() != ImageFrame::OrientationPolicy::Identity) {
        return 1;
    }

    QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const ImageFrame imageFrame(image);
    if (!imageFrame.isValid() || imageFrame.sourceLogicalSize() != QSizeF(2.0, 2.0)
        || imageFrame.payloadByteSize() <= 0 || !imageFrame.hasAlpha()
        || imageFrame.orientationPolicy() != ImageFrame::OrientationPolicy::Identity) {
        return 1;
    }
    if (imageFrame.payloadRasterSize() != QSizeF(2.0, 2.0)
        || imageFrame.sourceToPayloadScale() != QSizeF(1.0, 1.0)
        || imageFrame.quality() != ImageViewportPayloadQuality::Exact
        || imageFrame.exactness() != ImageViewportPayloadExactness::ExactForSource) {
        return 1;
    }

    const ImageFrame previewFrame(image, QSizeF(4.0, 4.0), QSizeF(2.0, 2.0), QSizeF(0.5, 0.5),
        image.sizeInBytes(), ImageViewportPayloadQuality::Preview,
        ImageViewportPayloadExactness::NotExact, true, ImageFrame::OrientationPolicy::Identity,
        QStringLiteral("argb32"));
    if (!previewFrame.isValid() || previewFrame.sourceLogicalSize() != QSizeF(4.0, 4.0)
        || previewFrame.payloadRasterSize() != QSizeF(2.0, 2.0)
        || previewFrame.sourceToPayloadScale() != QSizeF(0.5, 0.5)
        || previewFrame.quality() != ImageViewportPayloadQuality::Preview) {
        return 1;
    }

    ImageSequenceProviderDisplayDemand installedDemand;
    installedDemand.setRole(ImageViewportPageRole::Secondary);
    installedDemand.setResolvedFrame(1);
    installedDemand.setRequestedPosition(100);
    const ImageSequenceProviderRequest installedRequest = ImageSequenceProviderRequest::position(
        token, ImageViewportPageRole::Secondary, 100, 1, installedDemand);
    if (!installedRequest.isValid()
        || installedRequest.kind() != ImageSequenceProviderRequestKind::Position
        || installedRequest.demand().role() != ImageViewportPageRole::Secondary
        || installedRequest.demand().demandRevision().isValid()) {
        return 1;
    }
    auto installedHandleFrame = std::make_unique<ImageFrame>(image);
    ImageSequenceProviderFrameHandle installedHandle(std::move(installedHandleFrame));
    ImageSequenceProviderFrameEnvelope exactEnvelope;
    exactEnvelope.setFrame(0);
    exactEnvelope.setFrameStartPosition(-1);
    exactEnvelope.setFrameDuration(-1);
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
        || deviceIndependentFrame.sourceLogicalSize() != QSizeF(2.0, 1.0)
        || deviceIndependentFrame.payloadByteSize() != deviceIndependentImage.sizeInBytes()) {
        return 1;
    }

    std::unique_ptr<ImageSequenceFactoryResult> stillResult(factory.fromFrame(image));
    if (!stillResult || !stillResult->sequence()) {
        return 1;
    }
    if (stillResult->outcome() != ImageSequenceFactoryOutcome::Created) {
        return 1;
    }

    std::unique_ptr<ImageSequenceFactoryResult> deviceIndependentStillResult(
        factory.fromFrame(deviceIndependentImage));
    if (!deviceIndependentStillResult || !deviceIndependentStillResult->sequence()) {
        return 1;
    }

    std::unique_ptr<ImageSequenceFactoryResult> invalidFrameResult(factory.fromFrame(QImage()));
    if (!invalidFrameResult || invalidFrameResult->sequence()
        || invalidFrameResult->outcome() != ImageSequenceFactoryOutcome::Rejected
        || invalidFrameResult->errorString().isEmpty()) {
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
    if (timedResult->outcome() != ImageSequenceFactoryOutcome::Created) {
        return 1;
    }

    ConsumerAdapter consumerAdapter;
    const ImageSequenceProviderDescriptor consumerDescriptor = consumerAdapter.descriptor();
    if (!consumerDescriptor.isValid()
        || !consumerDescriptor.constructionMetadata().isTimedFrameList()
        || consumerDescriptor.constructionMetadata().timedPlaybackSupport()
            != ImageViewportCapabilitySupport::True) {
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
        != ImageViewportCommandOutcome::Accepted) {
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
        != ImageViewportCommandOutcome::Accepted) {
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
            != ImageViewportCommandOutcome::Invalid
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
    primaryPageCoordinate.setSourceSpace(ImageViewportCoordinateSpace::DisplayedPage);
    primaryPageCoordinate.setTargetSpace(ImageViewportCoordinateSpace::DisplayedPage);
    primaryPageCoordinate.setRole(QVariant::fromValue(ImageViewportPageRole::Primary));
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
            != ImageViewportCommandOutcome::Accepted
        || !nearlyEqual(steppedCommandViewport.state().presentation().zoomPercent(), 125.0)) {
        return 1;
    }

    ImageViewport snapshotViewport;
    const ImageViewportStateSnapshot snapshot = snapshotViewport.state();
    const ImageViewportStateSnapshot snapshotCopy = snapshot;
    if (snapshotCopy != snapshot
        || snapshot.request().status() != ImageViewportRequestStatus::NoRequest
        || snapshot.request().reason() != ImageViewportRequestReason::NoRequest
        || snapshot.display().status() != ImageViewportDisplayStatus::Empty
        || snapshot.display().phase() != ImageViewportDisplayPhase::NoPresentation
        || snapshot.primary().present() || snapshot.secondary().present()
        || snapshot.diagnostics().commandReason() != ImageViewportCommandReason::NoCommand
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
    if (installedCommandResult.outcome() != ImageViewportCommandOutcome::Accepted
        || installedCommandResult.reason() != ImageViewportCommandReason::NoCommand
        || installedCommandResult.commandRevision().isValid()
        || installedCommandResult.snapshotRevision().isValid()) {
        return 1;
    }
    ImageViewportPresentationCommand installedPresentationCommand;
    installedPresentationCommand.setManualZoomPercent(125.0);
    installedPresentationCommand.setPageGap(3.0);
    installedPresentationCommand.setQualityPreference(ImageViewportQualityPreference::ExactDetail);
    installedPresentationCommand.setExactnessPreference(
        ImageViewportExactnessPreference::RequireExact);
    if (!installedPresentationCommand.hasManualZoomPercent()
        || !installedPresentationCommand.hasPageGap()
        || !installedPresentationCommand.hasQualityPreference()
        || !installedPresentationCommand.hasExactnessPreference()
        || helperViewport.setPresentation(installedPresentationCommand).outcome()
            != ImageViewportCommandOutcome::Accepted
        || helperViewport.state().presentation().fitMode() != ImageViewportFitMode::Manual
        || !nearlyEqual(helperViewport.state().presentation().zoomPercent(), 125.0)
        || helperViewport.state().presentation().pageGap() != 3.0
        || helperViewport.state().presentation().qualityPreference()
            != ImageViewportQualityPreference::ExactDetail
        || helperViewport.state().presentation().exactnessPreference()
            != ImageViewportExactnessPreference::RequireExact) {
        return 1;
    }
    ImageViewportCoordinateInput installedCoordinateInput;
    installedCoordinateInput.setSourceSpace(ImageViewportCoordinateSpace::Item);
    installedCoordinateInput.setTargetSpace(ImageViewportCoordinateSpace::DisplayedPage);
    installedCoordinateInput.setRole(QVariant::fromValue(ImageViewportPageRole::Primary));
    installedCoordinateInput.setPoint(QPointF(1.0, 2.0));
    const ImageViewportCoordinateResult installedCoordinateResult(
        false, QPointF(), installedCoordinateInput.targetSpace(), installedCoordinateInput.role());
    if (installedCoordinateInput.role().value<ImageViewportPageRole>()
            != ImageViewportPageRole::Primary
        || installedCoordinateResult.isValid()
        || installedCoordinateResult.space() != ImageViewportCoordinateSpace::DisplayedPage
        || installedCoordinateResult.role().value<ImageViewportPageRole>()
            != ImageViewportPageRole::Primary
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
        || mismatchedTimedResult->outcome() != ImageSequenceFactoryOutcome::Rejected
        || !mismatchedTimedResult->errorString().contains(QStringLiteral("same count"))) {
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
    if (builder.count() != 0 || !builder.errorString().isEmpty()) {
        return 1;
    }

    ConsumerAdapter adapter;
    std::unique_ptr<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    if (!result || !result->sequence()) {
        return 1;
    }
    if (result->outcome() != ImageSequenceFactoryOutcome::Created) {
        return 1;
    }

    ImageViewport providerViewport;
    providerViewport.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    if (providerViewport.state().request().status() != ImageViewportRequestStatus::Loading
        || providerViewport.state().request().reason()
            != ImageViewportRequestReason::ProviderWaiting
        || providerViewport.state().primary().request().frame() != 0
        || providerViewport.state().primary().request().position() != 0
        || providerViewport.state().primary().metadata().frameCount() != 2
        || providerViewport.state().primary().metadata().totalDuration() != 200) {
        return 1;
    }

    QCoreApplication::processEvents();
    if (providerViewport.state().request().status() != ImageViewportRequestStatus::Loading
        || providerViewport.state().request().reason() != ImageViewportRequestReason::RenderWaiting
        || providerViewport.state().display().status() != ImageViewportDisplayStatus::Empty
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
