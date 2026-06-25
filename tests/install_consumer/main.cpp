#include <imageviewport.h>

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QtPlugin>
#include <QUrl>
#include <QVariant>

#include <memory>

Q_IMPORT_PLUGIN(ImageViewportPlugin)

class ConsumerSession final : public ImageSequenceProviderSession
{
public:
    using ImageSequenceProviderSession::ImageSequenceProviderSession;

    void requestMetadata(const ImageSequenceProviderRequestToken &token) override
    {
        emit metadataReady(token, ImageSequenceProviderMetadata::still(QSizeF(2.0, 2.0)));
    }

    void requestFrame(const ImageSequenceProviderRequestToken &token, int frame) override
    {
        QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        auto payload = std::make_unique<ImageFrame>(image);
        emit frameReady(token,
            new ImageSequenceProviderFrameHandle(std::move(payload)),
            ImageSequenceProviderFrameMetadata::timedFrame(frame, 0, 100));
    }

    void requestPlayback(const ImageSequenceProviderRequestToken &token, int frame, int position) override
    {
        m_lastPlaybackFrame = frame;
        m_lastPlaybackPosition = position;
        emit providerProgress(token, 0.5);
        emit endOfSequence(token);
    }

    void cancelRequest(const ImageSequenceProviderRequestToken &token) override
    {
        m_lastCancelledToken = token;
        emit providerCancelled(token, QStringLiteral("cancelled"));
    }

    void close() override
    {
        m_closed = true;
    }

    int lastPlaybackFrame() const
    {
        return m_lastPlaybackFrame;
    }

    int lastPlaybackPosition() const
    {
        return m_lastPlaybackPosition;
    }

    ImageSequenceProviderRequestToken lastCancelledToken() const
    {
        return m_lastCancelledToken;
    }

    bool closed() const
    {
        return m_closed;
    }

private:
    int m_lastPlaybackFrame = -1;
    int m_lastPlaybackPosition = -1;
    ImageSequenceProviderRequestToken m_lastCancelledToken;
    bool m_closed = false;
};

class ConsumerSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    ImageSequenceProviderSession *createSession(QObject *parent) override
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

    CapabilitySupport frameSeekCapability() const override
    {
        return CapabilitySupport::KnownTrue;
    }

    CapabilitySupport positionSeekCapability() const override
    {
        return CapabilitySupport::KnownTrue;
    }
};

namespace {
bool canCreateInstalledQmlViewport()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT));

    QQmlComponent component(&engine);
    component.setData("import QtQuick\nimport ImageViewport 1.0\nImageViewport { width: 10; height: 10 }\n", QUrl());
    if (!component.isReady()) {
        qWarning() << component.errors();
        return false;
    }

    const std::unique_ptr<QObject> object(component.create());
    if (!object) {
        qWarning() << component.errors();
        return false;
    }
    return qobject_cast<ImageViewport *>(object.get()) != nullptr;
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

    if (object->property("maximumLogicalWidth").toInt() != ImageSequenceLimits::maximumLogicalWidth()
        || object->property("maximumLogicalHeight").toInt() != ImageSequenceLimits::maximumLogicalHeight()
        || object->property("maximumPixelsPerFrame").toLongLong() != ImageSequenceLimits::maximumPixelsPerFrame()
        || object->property("maximumPayloadBytesPerFrame").toLongLong() != ImageSequenceLimits::maximumPayloadBytesPerFrame()
        || object->property("maximumTimedListFrameCount").toInt() != ImageSequenceLimits::maximumTimedListFrameCount()
        || object->property("maximumFrameDuration").toInt() != ImageSequenceLimits::maximumFrameDuration()
        || object->property("maximumTotalSequenceDuration").toInt() != ImageSequenceLimits::maximumTotalSequenceDuration()
        || object->property("maximumDiagnosticStringLength").toInt() != ImageSequenceLimits::maximumDiagnosticStringLength()) {
        return false;
    }

    return ImageSequenceLimits::maximumLogicalWidth() >= 8192
        && ImageSequenceLimits::maximumLogicalHeight() >= 8192
        && ImageSequenceLimits::maximumPixelsPerFrame() >= 67108864LL
        && ImageSequenceLimits::maximumPayloadBytesPerFrame() >= 268435456LL
        && ImageSequenceLimits::maximumTimedListFrameCount() >= 10000
        && ImageSequenceLimits::maximumFrameDuration() >= 86400000
        && ImageSequenceLimits::maximumTotalSequenceDuration() >= 86400000
        && ImageSequenceLimits::maximumDiagnosticStringLength() >= 4096;
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

    Component.onCompleted: {
        const playOutcome = play()
        const pauseOutcome = pause()
        const stopOutcome = stop()
        const seekOutcome = seek(0)
        const positionSeekOutcome = seekToPosition(0)
        zoom = 2
        pan = Qt.point(3, 4)
        const resetViewOutcome = resetView()
        commandSurfaceAvailable = requestStatus === ImageViewport.RequestStatus.NoRequest
            && requestReason === ImageViewport.RequestReason.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && playbackPhase === ImageViewport.PlaybackPhase.Stopped
            && playOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && pauseOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && stopOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && seekOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && positionSeekOutcome === ImageViewport.CommandOutcome.IgnoredNoRequest
            && resetViewOutcome === ImageViewport.CommandOutcome.Accepted
            && commandReason === ImageViewport.CommandReason.NoCommand
            && commandRevision === 6
            && zoom === 1
            && pan.x === 0
            && pan.y === 0
            && frameSeekBounds.minimum === -1
            && frameSeekBounds.maximum === -1
            && positionSeekBounds.minimum === -1
            && positionSeekBounds.maximum === -1
            && itemToImage(1, 1).valid === false
            && imageToItem(1, 1).x === 0
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
    const ImageSequenceProviderRequestToken token(7);
    bool progressReceived = false;
    bool endReceived = false;
    bool cancellationReceived = false;
    QObject::connect(&session,
        &ImageSequenceProviderSession::providerProgress,
        [&progressReceived, &token](const ImageSequenceProviderRequestToken &receivedToken, double progress) {
            progressReceived = receivedToken == token && progress == 0.5;
        });
    QObject::connect(&session,
        &ImageSequenceProviderSession::endOfSequence,
        [&endReceived, &token](const ImageSequenceProviderRequestToken &receivedToken) {
            endReceived = receivedToken == token;
        });
    QObject::connect(&session,
        &ImageSequenceProviderSession::providerCancelled,
        [&cancellationReceived, &token](const ImageSequenceProviderRequestToken &receivedToken, const QString &diagnostic) {
            cancellationReceived = receivedToken == token && diagnostic == QStringLiteral("cancelled");
        });

    session.requestPlayback(token, 1, 100);
    session.cancelRequest(token);
    session.close();

    return progressReceived
        && endReceived
        && cancellationReceived
        && session.lastPlaybackFrame() == 1
        && session.lastPlaybackPosition() == 100
        && session.lastCancelledToken() == token
        && session.closed();
}
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    [[maybe_unused]] const auto knownTrue = ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue;
    [[maybe_unused]] const auto knownFalse = ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse;
    ImageSequenceProviderRequestToken token(1);
    if (!token.isValid() || token.id() != 1 || token != ImageSequenceProviderRequestToken(1)) {
        return 1;
    }

    const ImageSequenceProviderMetadata metadata = ImageSequenceProviderMetadata::timedFrameList(QSizeF(2.0, 2.0), {100, 200});
    if (!metadata.isSpecified() || !metadata.isValid() || metadata.isStill() || !metadata.isTimedFrameList()) {
        return 1;
    }
    if (metadata.logicalSize() != QSizeF(2.0, 2.0) || metadata.frameDurations() != QVector<int>({100, 200})) {
        return 1;
    }

    const ImageSequenceProviderMetadata fixedMetadata = ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(2.0, 2.0), 3, 50);
    if (!fixedMetadata.isSpecified() || !fixedMetadata.isValid() || fixedMetadata.isStill() || !fixedMetadata.isTimedFrameList()) {
        return 1;
    }
    if (fixedMetadata.logicalSize() != QSizeF(2.0, 2.0) || fixedMetadata.frameDurations() != QVector<int>({50, 50, 50})) {
        return 1;
    }

    const ImageSequenceProviderMetadata stillMetadata = ImageSequenceProviderMetadata::still(QSizeF(2.0, 2.0));
    if (!stillMetadata.isSpecified() || !stillMetadata.isValid() || !stillMetadata.isStill() || stillMetadata.isTimedFrameList()) {
        return 1;
    }
    if (stillMetadata.logicalSize() != QSizeF(2.0, 2.0) || !stillMetadata.frameDurations().isEmpty()) {
        return 1;
    }

    const ImageSequenceProviderFrameMetadata frameMetadata = ImageSequenceProviderFrameMetadata::timedFrame(1, 100, 200);
    if (!frameMetadata.isValid()
        || !frameMetadata.isTimedFrame()
        || frameMetadata.frame() != 1
        || frameMetadata.frameStartPosition() != 100
        || frameMetadata.frameDuration() != 200) {
        return 1;
    }

    const ImageSequenceProviderFrameMetadata stillFrameMetadata = ImageSequenceProviderFrameMetadata::stillFrame();
    if (!stillFrameMetadata.isValid()
        || !stillFrameMetadata.isStillFrame()
        || stillFrameMetadata.isTimedFrame()
        || stillFrameMetadata.frame() != 0
        || stillFrameMetadata.frameStartPosition() != -1
        || stillFrameMetadata.frameDuration() != -1) {
        return 1;
    }

    ImageSequenceFactory factory;
    const ImageFrame emptyFrame;
    if (emptyFrame.isValid()
        || emptyFrame.logicalSize() != QSizeF()
        || emptyFrame.payloadByteSize() != 0
        || emptyFrame.hasAlphaChannel()
        || emptyFrame.orientationPolicy() != ImageFrame::OrientationPolicy::Identity) {
        return 1;
    }

    QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const ImageFrame imageFrame(image);
    if (!imageFrame.isValid()
        || imageFrame.logicalSize() != QSizeF(2.0, 2.0)
        || imageFrame.payloadByteSize() <= 0
        || !imageFrame.hasAlphaChannel()
        || imageFrame.orientationPolicy() != ImageFrame::OrientationPolicy::Identity) {
        return 1;
    }

    std::unique_ptr<ImageSequenceFactoryResult> stillResult(factory.fromFrame(image));
    if (!stillResult || !stillResult->sequence()) {
        return 1;
    }
    if (stillResult->outcome() != ImageSequenceFactoryResult::FactoryOutcome::Created) {
        return 1;
    }

    std::unique_ptr<ImageSequenceFactoryResult> invalidFrameResult(factory.fromFrame(QImage()));
    if (!invalidFrameResult
        || invalidFrameResult->sequence()
        || invalidFrameResult->outcome() != ImageSequenceFactoryResult::FactoryOutcome::Invalid
        || invalidFrameResult->errorString().isEmpty()
        || !invalidFrameResult->warningString().isEmpty()) {
        return 1;
    }

    QVector<QImage> timedImages;
    timedImages.append(image);
    timedImages.append(image);
    QVector<int> timedDurations{100, 200};
    std::unique_ptr<ImageSequenceFactoryResult> timedResult(factory.fromTimedFrameList(timedImages, timedDurations));
    if (!timedResult || !timedResult->sequence()) {
        return 1;
    }
    if (timedResult->outcome() != ImageSequenceFactoryResult::FactoryOutcome::Created) {
        return 1;
    }

    std::unique_ptr<ImageSequenceFactoryResult> mismatchedTimedResult(factory.fromTimedFrameList(timedImages, {100}));
    if (!mismatchedTimedResult
        || mismatchedTimedResult->sequence()
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
    if (builder.appendFrame(image, 0) || builder.count() != 1 || !builder.errorString().contains(QStringLiteral("positive"))) {
        return 1;
    }
    builder.clear();
    if (builder.count() != 0 || !builder.errorString().isEmpty() || !builder.warningString().isEmpty()) {
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
        || providerViewport.requestedFrame() != 0
        || providerViewport.requestedPosition() != 0
        || providerViewport.frameCount() != 2
        || providerViewport.totalDuration() != 200) {
        return 1;
    }

    QCoreApplication::processEvents();
    if (providerViewport.requestStatus() != ImageViewport::RequestStatus::Loading
        || providerViewport.requestReason() != ImageViewport::RequestReason::RenderWaiting
        || providerViewport.displayStatus() != ImageViewport::DisplayStatus::Empty
        || providerViewport.displayedFrame() != -1) {
        return 1;
    }

    return canCreateInstalledQmlViewport()
            && canReadInstalledQmlLimits()
            && canUseInstalledQmlFactorySurface()
            && installedQmlSingletonTypesAreNotCreatable()
            && canUseInstalledQmlCommandSurface()
            && canUseInstalledProviderSessionSurface()
        ? 0
        : 1;
}
