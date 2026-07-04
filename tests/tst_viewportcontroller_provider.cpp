#include "imageviewport.h"
#include "viewportcontroller_p.h"

#include <QtTest/QTest>

#include <memory>
#include <utility>

namespace {

class StubProviderSession final : public ImageSequenceProviderSession
{
public:
    using ImageSequenceProviderSession::ImageSequenceProviderSession;

    void requestMetadata(ImageSequenceProviderRequestToken) override { }
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
public:
    explicit StubProviderAdapter(ImageSequenceProviderMetadata knownMetadata = {},
        QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , m_factory(std::make_shared<StubProviderSessionFactory>())
        , m_knownMetadata(std::move(knownMetadata))
    {
    }

    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory() const override
    {
        return m_factory;
    }

    ImageSequenceProviderMetadata knownMetadata() const override { return m_knownMetadata; }

private:
    std::shared_ptr<ImageSequenceProviderSessionFactory> m_factory;
    ImageSequenceProviderMetadata m_knownMetadata;
};

class ProviderControllerContext final : public ViewportControllerContext
{
public:
    ImageSequence* sequence = nullptr;
    bool providerSequence = false;
    bool completeKnownMetadata = false;
    ImageSequenceProviderKnownFacts knownFacts;

    QRectF itemBounds() const override { return QRectF(0.0, 0.0, 100.0, 100.0); }
    bool hasActiveRequest() const override { return sequence != nullptr; }
    bool hasProviderSequence() const override { return sequence && providerSequence; }
    bool providerHasCompleteKnownMetadata() const override { return completeKnownMetadata; }
    ImageSequenceProviderKnownFacts providerKnownFacts() const override { return knownFacts; }
    QSizeF providerKnownLogicalSize() const override { return knownFacts.logicalSize(); }
    double width() const override { return 100.0; }
    double height() const override { return 100.0; }
};

std::unique_ptr<ImageSequenceFactoryResult> makeProviderSequence(
    ImageSequenceFactory& factory, ProviderControllerContext& context,
    ImageSequenceProviderMetadata knownMetadata = {})
{
    StubProviderAdapter adapter(knownMetadata);
    auto result = std::unique_ptr<ImageSequenceFactoryResult>(factory.fromProvider(&adapter));
    if (!result || !result->sequence()) {
        return {};
    }
    context.sequence = result->sequence();
    context.providerSequence = true;
    if (knownMetadata.isSpecified()) {
        context.completeKnownMetadata = true;
        if (knownMetadata.isStill()) {
            context.knownFacts = ImageSequenceProviderKnownFacts::still(knownMetadata.logicalSize());
        } else if (knownMetadata.isTimedFrameList()) {
            context.knownFacts = ImageSequenceProviderKnownFacts::timedFrameList(
                knownMetadata.logicalSize(), knownMetadata.frameDurations());
        }
    }
    return result;
}

} // namespace

class ViewportControllerProviderTest : public QObject
{
    Q_OBJECT

public:
    explicit ViewportControllerProviderTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void metadataDispatchFailureRejectsStaleTokenAndClosesActiveGeneration();
    void frameDispatchFailureRejectsStaleTokenAndClosesActiveGeneration();
    void metadataDispatchFailureReportsNullSessionAfterAcceptance();
    void sessionSerialRejectsSupersededSessionResults();
    void metadataAndFrameEventsRejectStaleTokens();
    void cancellationTerminalEventClosesActiveMetadataGeneration();
};

void ViewportControllerProviderTest::
    metadataDispatchFailureRejectsStaleTokenAndClosesActiveGeneration()
{
    ImageSequenceFactory factory;
    ProviderControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence
        = makeProviderSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller(context);

    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    const ViewportSequenceAssignmentResult assigned = controller.assignSequence(assignment);
    QCOMPARE(assigned.openProviderSession, true);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Loading);

    StubProviderSession session;
    const quint64 sessionSerial = controller.installProviderSession(&session);
    QVERIFY(sessionSerial != 0);

    const ViewportProviderSessionOpenResult opened = controller.handleProviderSessionOpened();
    QCOMPARE(opened.providerMetadataTransport.sendCommand, true);
    const ImageSequenceProviderRequestToken activeToken = opened.providerMetadataTransport.token;
    QVERIFY(activeToken.isValid());

    const ViewportProviderTerminalEventResult staleFailure
        = controller.handleProviderDispatchFailure(ImageViewport::PageRole::Primary,
            { ImageSequenceProviderRequestToken(activeToken.id() + 1),
                QStringLiteral("stale delivery failure") });
    QCOMPARE(staleFailure.changes.requestState, false);
    QCOMPARE(staleFailure.providerFrameTransport.closeSession, false);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Loading);

    const ViewportProviderTerminalEventResult activeFailure
        = controller.handleProviderDispatchFailure(ImageViewport::PageRole::Primary,
            { activeToken, QStringLiteral("delivery failed") });
    QCOMPARE(activeFailure.changes.requestState, true);
    QCOMPARE(activeFailure.changes.requestRevision, true);
    QCOMPARE(activeFailure.changes.diagnostics, true);
    QCOMPARE(activeFailure.providerFrameTransport.closeSession, true);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Error);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::ProviderFailure);
    QVERIFY(controller.requestState().errorString.contains(QStringLiteral("delivery failed")));

    QCOMPARE(controller.takeProviderSession(), &session);
    const ViewportProviderTerminalEventResult closedFailure
        = controller.handleProviderDispatchFailure(
            ImageViewport::PageRole::Primary, { activeToken, QStringLiteral("late failure") });
    QCOMPARE(closedFailure.changes.requestState, false);
    QCOMPARE(closedFailure.providerFrameTransport.closeSession, false);
}

void ViewportControllerProviderTest::
    frameDispatchFailureRejectsStaleTokenAndClosesActiveGeneration()
{
    ImageSequenceFactory factory;
    ProviderControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeProviderSequence(
        factory, context, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QVERIFY(sequence);
    ViewportController controller(context);

    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    const ViewportSequenceAssignmentResult assigned = controller.assignSequence(assignment);
    QCOMPARE(assigned.openProviderSession, true);
    QCOMPARE(controller.requestState().activeRequest.target.frame, 0);

    StubProviderSession session;
    QVERIFY(controller.installProviderSession(&session) != 0);
    const ViewportProviderSessionOpenResult opened = controller.handleProviderSessionOpened();
    QCOMPARE(opened.providerFrameTransport.sendCommand, true);
    const ImageSequenceProviderRequestToken activeToken
        = opened.providerFrameTransport.command.token;
    QVERIFY(activeToken.isValid());

    const ViewportProviderTerminalEventResult staleFailure
        = controller.handleProviderDispatchFailure(ImageViewport::PageRole::Primary,
            { ImageSequenceProviderRequestToken(activeToken.id() + 1),
                QStringLiteral("stale delivery failure") });
    QCOMPARE(staleFailure.changes.requestState, false);
    QCOMPARE(staleFailure.providerFrameTransport.closeSession, false);

    const ViewportProviderTerminalEventResult activeFailure
        = controller.handleProviderDispatchFailure(ImageViewport::PageRole::Primary,
            { activeToken, QStringLiteral("delivery failed") });
    QCOMPARE(activeFailure.changes.requestState, true);
    QCOMPARE(activeFailure.providerFrameTransport.closeSession, true);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Error);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::ProviderFailure);
    QVERIFY(controller.requestState().errorString.contains(QStringLiteral("delivery failed")));

    QCOMPARE(controller.takeProviderSession(), &session);
}

void ViewportControllerProviderTest::
    metadataDispatchFailureReportsNullSessionAfterAcceptance()
{
    ImageSequenceFactory factory;
    ProviderControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence
        = makeProviderSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller(context);

    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    controller.assignSequence(assignment);

    StubProviderSession session;
    controller.installProviderSession(&session);
    const ViewportProviderSessionOpenResult opened = controller.handleProviderSessionOpened();
    const ImageSequenceProviderRequestToken metadataToken = opened.providerMetadataTransport.token;
    QVERIFY(metadataToken.isValid());
    QCOMPARE(controller.takeProviderSession(), &session);

    const ViewportProviderTerminalEventResult failure
        = controller.handleProviderDispatchFailure(ImageViewport::PageRole::Primary,
            { metadataToken, QStringLiteral("missing session") });
    QCOMPARE(failure.changes.requestState, true);
    QCOMPARE(failure.providerFrameTransport.closeSession, false);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Error);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::ProviderFailure);
    QVERIFY(controller.requestState().errorString.contains(QStringLiteral("missing session")));
}

void ViewportControllerProviderTest::sessionSerialRejectsSupersededSessionResults()
{
    ImageSequenceFactory factory;
    ProviderControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence
        = makeProviderSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller(context);

    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    controller.assignSequence(assignment);

    StubProviderSession firstSession;
    const quint64 firstSerial = controller.installProviderSession(&firstSession);
    QVERIFY(firstSerial != 0);
    QCOMPARE(controller.acceptsProviderSessionResult(firstSerial), true);

    StubProviderSession secondSession;
    const quint64 secondSerial = controller.installProviderSession(&secondSession);
    QVERIFY(secondSerial > firstSerial);
    QCOMPARE(controller.acceptsProviderSessionResult(firstSerial), false);
    QCOMPARE(controller.acceptsProviderSessionResult(secondSerial), true);
    QCOMPARE(controller.takeProviderSession(), &secondSession);
    QCOMPARE(controller.acceptsProviderSessionResult(secondSerial), false);
}

void ViewportControllerProviderTest::metadataAndFrameEventsRejectStaleTokens()
{
    ImageSequenceFactory factory;
    ProviderControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence
        = makeProviderSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller(context);

    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    controller.assignSequence(assignment);

    StubProviderSession session;
    controller.installProviderSession(&session);
    const ViewportProviderSessionOpenResult opened = controller.handleProviderSessionOpened();
    const ImageSequenceProviderRequestToken metadataToken = opened.providerMetadataTransport.token;
    QVERIFY(metadataToken.isValid());

    const ViewportProviderMetadataEventAcceptance staleMetadata
        = controller.acceptProviderMetadataEvent(
            { ImageSequenceProviderRequestToken(metadataToken.id() + 1) });
    QCOMPARE(staleMetadata.accepted, false);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Loading);

    const ViewportProviderMetadataEventAcceptance activeMetadata
        = controller.acceptProviderMetadataEvent({ metadataToken });
    QCOMPARE(activeMetadata.accepted, true);
    const ViewportProviderMetadataAdmissionResult metadataAdmission
        = controller.handleProviderMetadataAdmission(
            ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QCOMPARE(metadataAdmission.accepted, true);
    controller.handleProviderAcceptedMetadataFacts(metadataAdmission.facts);
    const ViewportProviderMetadataTargetPolicyResult targetPolicy
        = controller.handleProviderMetadataTargetPolicy(metadataAdmission.facts);
    QCOMPARE(targetPolicy.providerFrameTransport.sendCommand, true);
    const ImageSequenceProviderRequestToken frameToken
        = targetPolicy.providerFrameTransport.command.token;
    QVERIFY(frameToken.isValid());

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    const ImageViewportInternal::ViewportChangeSet staleFrame
        = controller.handleProviderFrameEvent(
            { ImageSequenceProviderRequestToken(frameToken.id() + 1) }, &frame,
            ImageSequenceProviderFrameMetadata::stillFrame());
    QCOMPARE(staleFrame.requestState, false);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Loading);

    const ImageViewportInternal::ViewportChangeSet activeFrame
        = controller.handleProviderFrameEvent(
            { frameToken }, &frame, ImageSequenceProviderFrameMetadata::stillFrame());
    QCOMPARE(activeFrame.requestState, true);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::UploadPending);
}

void ViewportControllerProviderTest::
    cancellationTerminalEventClosesActiveMetadataGeneration()
{
    ImageSequenceFactory factory;
    ProviderControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence
        = makeProviderSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller(context);

    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    controller.assignSequence(assignment);

    StubProviderSession session;
    controller.installProviderSession(&session);
    const ViewportProviderSessionOpenResult opened = controller.handleProviderSessionOpened();
    const ImageSequenceProviderRequestToken metadataToken = opened.providerMetadataTransport.token;
    QVERIFY(metadataToken.isValid());

    const ViewportProviderTerminalEventResult cancellation
        = controller.handleProviderTerminalEvent(
            { metadataToken, ViewportProviderTerminalEvent::Kind::Cancellation,
                ImageSequenceProviderSession::UnsupportedCause::PayloadRejection,
                QStringLiteral("provider cancelled metadata") });
    QCOMPARE(cancellation.changes.requestState, true);
    QCOMPARE(cancellation.providerFrameTransport.closeSession, true);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Error);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::ProviderFailure);
    QVERIFY(controller.requestState().errorString.contains(QStringLiteral("cancelled")));
}

QTEST_MAIN(ViewportControllerProviderTest)

#include "tst_viewportcontroller_provider.moc"
