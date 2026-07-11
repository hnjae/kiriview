#include "imageviewport.h"
#include "viewportcontroller_p.h"
#include "viewportcontrollercommandcontract_p.h"

#include <QtTest/QTest>

#include <memory>

namespace {

class StubProviderSession final : public ImageSequenceProviderSession
{
    Q_OBJECT

public:
    using ImageSequenceProviderSession::ImageSequenceProviderSession;

    void request(const ImageSequenceProviderRequest&) override { }
};

class StubProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    ImageSequenceProviderSession* createSession(QObject* parent) override
    {
        return new StubProviderSession(parent);
    }
};

class StubProviderAdapter final : public ImageSequenceProviderAdapter
{
    Q_OBJECT

public:
    explicit StubProviderAdapter(
        ImageSequenceProviderMetadata knownMetadata = {}, QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , factory(std::make_shared<StubProviderSessionFactory>())
        , knownMetadata(std::move(knownMetadata))
    {
    }

    ImageSequenceProviderDescriptor descriptor() const override
    {
        ImageSequenceProviderDescriptor descriptor;
        descriptor.setSessionFactory(factory);
        descriptor.setKnownMetadata(knownMetadata);
        if (knownMetadata.isStill()) {
            descriptor.setKnownFacts(
                ImageSequenceProviderKnownFacts::still(knownMetadata.logicalSize()));
        } else if (knownMetadata.isTimedFrameList()) {
            descriptor.setKnownFacts(ImageSequenceProviderKnownFacts::timedFrameList(
                knownMetadata.logicalSize(), knownMetadata.frameDurations()));
        }
        return descriptor;
    }

private:
    std::shared_ptr<ImageSequenceProviderSessionFactory> factory;
    ImageSequenceProviderMetadata knownMetadata;
};

std::unique_ptr<ImageSequenceFactoryResult> makeProviderSequence(
    ImageSequenceFactory& factory, ImageSequenceProviderMetadata knownMetadata = {})
{
    StubProviderAdapter adapter(std::move(knownMetadata));
    return std::unique_ptr<ImageSequenceFactoryResult>(factory.fromProvider(&adapter));
}

const ViewportProviderTransportCommand* findTransport(const ViewportProviderTransportBatch& batch,
    ViewportProviderTransportCommand::Kind kind, ImageViewport::PageRole role)
{
    for (const auto& effect : batch) {
        if (effect.kind == kind && effect.role == role) {
            return &effect;
        }
    }
    return nullptr;
}

struct ProviderSessionIdentity
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    quint64 generation = 0;
    quint64 sessionSerial = 0;
};

ProviderSessionIdentity sessionIdentity(const ViewportCommandResult& assignment,
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary)
{
    const auto* open = findTransport(assignment.transition.providerAfterPublication,
        ViewportProviderTransportCommand::Kind::OpenSession, role);
    return open ? ProviderSessionIdentity { role, open->generation, open->sessionSerial }
                : ProviderSessionIdentity {};
}

ViewportProviderEvent providerEvent(const ProviderSessionIdentity& identity,
    ViewportProviderEvent::Kind kind, ImageSequenceProviderRequestToken token)
{
    ViewportProviderEvent event;
    event.kind = kind;
    event.role = identity.role;
    event.generation = identity.generation;
    event.sessionSerial = identity.sessionSerial;
    event.token = token;
    return event;
}

ViewportProviderHostEvent hostProviderEvent(const ViewportProviderEvent& event)
{
    ViewportProviderHostEvent hostEvent;
    hostEvent.kind = ViewportProviderHostEvent::Kind::ProviderEvent;
    hostEvent.role = event.role;
    hostEvent.providerEvent = event;
    return hostEvent;
}

} // namespace

class ViewportControllerProviderTest : public QObject // clazy:exclude=ctor-missing-parent-argument
{
    Q_OBJECT

private slots:
    void sessionOpenAcknowledgementProducesOrderedMetadataRequest();
    void metadataReadyProducesFrameRequest();
    void staleSessionEventIsIgnored();
    void dispatchFailureClosesActiveGeneration();
    void queuedProviderFlushReturnsChangesAndTransport();
    void secondaryMetadataReadyUsesRoleIdentity();
    void frameReadyStagesUpload();
    void terminalEventClosesActiveGeneration();
    void sessionOpenFailureIsReducedThroughHostEvent();
};

void ViewportControllerProviderTest::sessionOpenAcknowledgementProducesOrderedMetadataRequest()
{
    ImageSequenceFactory factory;
    const auto sequence = makeProviderSequence(factory);
    QVERIFY(sequence);
    ViewportController controller([] { return QRectF(0.0, 0.0, 100.0, 100.0); });
    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    const auto identity = sessionIdentity(controller.assignSequence(assignment));
    QVERIFY(identity.generation != 0);
    QVERIFY(identity.sessionSerial != 0);

    const auto result = controller.handleProviderHostEvent(
        { ViewportProviderHostEvent::Kind::SessionOpened, identity.role });

    QVERIFY(result.providerBeforePublication.isEmpty());
    QCOMPARE(result.providerAfterPublication.size(), 1);
    QCOMPARE(result.providerAfterPublication[0].kind,
        ViewportProviderTransportCommand::Kind::SendRequest);
    QCOMPARE(result.providerAfterPublication[0].role, identity.role);
    QCOMPARE(result.providerAfterPublication[0].request.kind(),
        ImageSequenceProviderRequestKind::Metadata);
    QVERIFY(result.providerAfterPublication[0].request.token().isValid());
}

void ViewportControllerProviderTest::metadataReadyProducesFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sequence = makeProviderSequence(factory);
    QVERIFY(sequence);
    ViewportController controller([] { return QRectF(0.0, 0.0, 100.0, 100.0); });
    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    const auto identity = sessionIdentity(controller.assignSequence(assignment));
    const auto opened = controller.handleProviderHostEvent(
        { ViewportProviderHostEvent::Kind::SessionOpened, identity.role });
    const auto* metadataRequest = findTransport(opened.providerAfterPublication,
        ViewportProviderTransportCommand::Kind::SendRequest, identity.role);
    QVERIFY(metadataRequest);

    auto event = providerEvent(
        identity, ViewportProviderEvent::Kind::MetadataReady, metadataRequest->request.token());
    event.metadata = ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0));
    const auto result = controller.handleProviderHostEvent(hostProviderEvent(event));

    const auto* frameRequest = findTransport(result.providerBeforePublication,
        ViewportProviderTransportCommand::Kind::SendRequest, identity.role);
    QVERIFY(frameRequest);
    QCOMPARE(frameRequest->request.kind(), ImageSequenceProviderRequestKind::Frame);
    QCOMPARE(frameRequest->request.frame(), 0);
    QCOMPARE(controller.requestState().roles[0].activeRequest.target.frame, 0);
}

void ViewportControllerProviderTest::staleSessionEventIsIgnored()
{
    ImageSequenceFactory factory;
    const auto sequence = makeProviderSequence(factory);
    QVERIFY(sequence);
    ViewportController controller([] { return QRectF(0.0, 0.0, 100.0, 100.0); });
    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    auto identity = sessionIdentity(controller.assignSequence(assignment));
    ++identity.sessionSerial;

    const auto result = controller.handleProviderHostEvent(
        hostProviderEvent(providerEvent(identity, ViewportProviderEvent::Kind::Waiting, {})));
    QCOMPARE(result.changes.requestState, false);
    QVERIFY(result.providerBeforePublication.isEmpty());
    QVERIFY(result.providerAfterPublication.isEmpty());
}

void ViewportControllerProviderTest::dispatchFailureClosesActiveGeneration()
{
    ImageSequenceFactory factory;
    const auto sequence = makeProviderSequence(factory);
    QVERIFY(sequence);
    ViewportController controller([] { return QRectF(0.0, 0.0, 100.0, 100.0); });
    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    const auto identity = sessionIdentity(controller.assignSequence(assignment));
    const auto opened = controller.handleProviderHostEvent(
        { ViewportProviderHostEvent::Kind::SessionOpened, identity.role });
    const auto* request = findTransport(opened.providerAfterPublication,
        ViewportProviderTransportCommand::Kind::SendRequest, identity.role);
    QVERIFY(request);

    ViewportProviderHostEvent failure;
    failure.kind = ViewportProviderHostEvent::Kind::DispatchFailed;
    failure.role = identity.role;
    failure.token = request->request.token();
    failure.diagnostic = QStringLiteral("delivery failed");
    const auto result = controller.handleProviderHostEvent(failure);

    QCOMPARE(result.changes.requestState, true);
    QVERIFY(findTransport(result.providerAfterPublication,
        ViewportProviderTransportCommand::Kind::CloseSession, identity.role));
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::ProviderFailure);
}

void ViewportControllerProviderTest::queuedProviderFlushReturnsChangesAndTransport()
{
    ImageSequenceFactory factory;
    const auto metadata
        = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 100 });
    const auto sequence = makeProviderSequence(factory, metadata);
    QVERIFY(sequence);
    ViewportController controller([] { return QRectF(0.0, 0.0, 100.0, 100.0); });
    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    const auto identity = sessionIdentity(controller.assignSequence(assignment));
    controller.handleProviderHostEvent(
        { ViewportProviderHostEvent::Kind::SessionOpened, identity.role });

    const auto seek = controller.seek(identity.role, 1);
    QVERIFY(findTransport(seek.transition.providerBeforePublication,
        ViewportProviderTransportCommand::Kind::ScheduleDeferredEvent, identity.role));
    const auto flush = controller.handleProviderHostEvent(
        { ViewportProviderHostEvent::Kind::FlushQueuedFrameRequest, identity.role });
    const auto* request = findTransport(flush.providerAfterPublication,
        ViewportProviderTransportCommand::Kind::SendRequest, identity.role);
    QVERIFY(request);
    QCOMPARE(request->request.frame(), 1);
    QCOMPARE(flush.changes.requestRevision, true);

    const auto staleFlush = controller.handleProviderHostEvent(
        { ViewportProviderHostEvent::Kind::FlushQueuedFrameRequest, identity.role });
    QVERIFY(staleFlush.providerAfterPublication.isEmpty());
}

void ViewportControllerProviderTest::secondaryMetadataReadyUsesRoleIdentity()
{
    ImageSequenceFactory factory;
    const auto primary = makeProviderSequence(factory);
    const auto secondary = makeProviderSequence(factory);
    QVERIFY(primary);
    QVERIFY(secondary);
    ViewportController controller([] { return QRectF(0.0, 0.0, 100.0, 100.0); });
    ViewportSequenceAssignment assignment;
    assignment.sequence = primary->sequence();
    assignment.secondarySequence = secondary->sequence();
    const auto identity = sessionIdentity(
        controller.assignSequence(assignment), ImageViewport::PageRole::Secondary);
    QVERIFY(identity.generation != 0);
    const auto opened = controller.handleProviderHostEvent(
        { ViewportProviderHostEvent::Kind::SessionOpened, identity.role });
    const auto* metadataRequest = findTransport(opened.providerAfterPublication,
        ViewportProviderTransportCommand::Kind::SendRequest, identity.role);
    QVERIFY(metadataRequest);

    auto event = providerEvent(
        identity, ViewportProviderEvent::Kind::MetadataReady, metadataRequest->request.token());
    event.metadata = ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0));
    const auto result = controller.handleProviderHostEvent(hostProviderEvent(event));

    QVERIFY(findTransport(result.providerBeforePublication,
        ViewportProviderTransportCommand::Kind::SendRequest, identity.role));
    QCOMPARE(controller.requestState().roles[1].activeRequest.target.frame, 0);
}

void ViewportControllerProviderTest::frameReadyStagesUpload()
{
    ImageSequenceFactory factory;
    const auto metadata = ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0));
    const auto sequence = makeProviderSequence(factory, metadata);
    QVERIFY(sequence);
    ViewportController controller([] { return QRectF(0.0, 0.0, 100.0, 100.0); });
    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    const auto identity = sessionIdentity(controller.assignSequence(assignment));
    const auto opened = controller.handleProviderHostEvent(
        { ViewportProviderHostEvent::Kind::SessionOpened, identity.role });
    const auto* frameRequest = findTransport(opened.providerAfterPublication,
        ViewportProviderTransportCommand::Kind::SendRequest, identity.role);
    QVERIFY(frameRequest);
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    ImageFrame frame(image);
    auto event = providerEvent(
        identity, ViewportProviderEvent::Kind::ImageFrameReady, frameRequest->request.token());
    event.imageFrame = &frame;

    const auto result = controller.handleProviderHostEvent(hostProviderEvent(event));
    QCOMPARE(result.changes.requestState, true);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::UploadPending);
    QVERIFY(controller.displayState().roles[0].pendingRenderPayload.commitPending);
}

void ViewportControllerProviderTest::terminalEventClosesActiveGeneration()
{
    ImageSequenceFactory factory;
    const auto sequence = makeProviderSequence(factory);
    QVERIFY(sequence);
    ViewportController controller([] { return QRectF(0.0, 0.0, 100.0, 100.0); });
    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    const auto identity = sessionIdentity(controller.assignSequence(assignment));
    const auto opened = controller.handleProviderHostEvent(
        { ViewportProviderHostEvent::Kind::SessionOpened, identity.role });
    const auto* request = findTransport(opened.providerAfterPublication,
        ViewportProviderTransportCommand::Kind::SendRequest, identity.role);
    QVERIFY(request);
    auto event = providerEvent(
        identity, ViewportProviderEvent::Kind::Cancellation, request->request.token());
    const auto result = controller.handleProviderHostEvent(hostProviderEvent(event));

    QCOMPARE(result.changes.requestState, true);
    QVERIFY(findTransport(result.providerAfterPublication,
        ViewportProviderTransportCommand::Kind::CloseSession, identity.role));
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::ProviderFailure);
}

void ViewportControllerProviderTest::sessionOpenFailureIsReducedThroughHostEvent()
{
    ImageSequenceFactory factory;
    const auto sequence = makeProviderSequence(factory);
    QVERIFY(sequence);
    ViewportController controller([] { return QRectF(0.0, 0.0, 100.0, 100.0); });
    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    controller.assignSequence(assignment);

    ViewportProviderHostEvent failure;
    failure.kind = ViewportProviderHostEvent::Kind::SessionOpenFailed;
    failure.role = ImageViewport::PageRole::Primary;
    failure.diagnostic = QStringLiteral("session failed");
    const auto result = controller.handleProviderHostEvent(failure);
    QCOMPARE(result.changes.requestState, true);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::ProviderFailure);
}

QTEST_MAIN(ViewportControllerProviderTest)

#include "tst_viewportcontroller_provider.moc"
