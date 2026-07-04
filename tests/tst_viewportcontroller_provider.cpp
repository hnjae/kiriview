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

enum class TerminalScopeCase {
    SessionOpenFailure,
    MetadataProductionFailure,
    MalformedMetadata,
    ContradictoryMetadata,
    MetadataTargetUnsupported,
    FrameUnsupported,
    FrameProviderFailure,
    FrameAdmissionFailure,
    MetadataEndOfSequenceProtocolViolation,
    FrameEndOfSequenceProtocolViolation,
    RenderFailure,
};

class ProviderControllerContext final : public ViewportControllerContext
{
public:
    ImageSequence* sequence = nullptr;
    bool providerSequence = false;
    bool completeKnownMetadata = false;
    ImageSequenceProviderKnownFacts knownFacts;
    ImageSequenceProviderKnownFacts secondaryKnownFacts;
    ImageSequenceProviderCapabilitySupport secondaryTimedPlaybackCapability
        = ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageSequenceProviderCapabilitySupport secondaryFrameSeekCapability
        = ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageSequenceProviderCapabilitySupport secondaryPositionSeekCapability
        = ImageSequenceProviderCapabilitySupport::Unavailable;

    QRectF itemBounds() const override { return QRectF(0.0, 0.0, 100.0, 100.0); }
    bool hasActiveRequest() const override { return sequence != nullptr; }
    bool hasDisplayableSequence() const override { return sequence != nullptr; }
    bool hasProviderSequence() const override { return sequence && providerSequence; }
    bool providerHasCompleteKnownMetadata() const override { return completeKnownMetadata; }
    ImageSequenceProviderKnownFacts providerKnownFacts() const override { return knownFacts; }
    QSizeF providerKnownLogicalSize() const override { return knownFacts.logicalSize(); }
    ImageSequenceProviderKnownFacts secondaryProviderKnownFacts() const override
    {
        return secondaryKnownFacts;
    }
    QSizeF secondaryProviderKnownLogicalSize() const override
    {
        return secondaryKnownFacts.logicalSize();
    }
    ImageSequenceProviderCapabilitySupport secondaryProviderTimedPlaybackCapability() const override
    {
        return secondaryTimedPlaybackCapability;
    }
    ImageSequenceProviderCapabilitySupport secondaryProviderFrameSeekCapability() const override
    {
        return secondaryFrameSeekCapability;
    }
    ImageSequenceProviderCapabilitySupport secondaryProviderPositionSeekCapability() const override
    {
        return secondaryPositionSeekCapability;
    }
    double width() const override { return 100.0; }
    double height() const override { return 100.0; }
};

std::unique_ptr<ImageSequenceFactoryResult> makeDetachedProviderSequence(
    ImageSequenceFactory& factory, ImageSequenceProviderMetadata knownMetadata = {})
{
    StubProviderAdapter adapter(knownMetadata);
    return std::unique_ptr<ImageSequenceFactoryResult>(factory.fromProvider(&adapter));
}

std::unique_ptr<ImageSequenceFactoryResult> makeProviderSequence(
    ImageSequenceFactory& factory, ProviderControllerContext& context,
    ImageSequenceProviderMetadata knownMetadata = {})
{
    auto result = makeDetachedProviderSequence(factory, knownMetadata);
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
    void providerFrameRenderAcknowledgementCommitsFromControllerSnapshot();
    void cancellationTerminalEventClosesActiveMetadataGeneration();
    void requiredRoleWaitPriorityAggregatesBeforeProjection_data();
    void requiredRoleWaitPriorityAggregatesBeforeProjection();
    void failureScopeTableClassifiesTerminalInputs_data();
    void failureScopeTableClassifiesTerminalInputs();
    void secondaryMetadataAdmissionRejectsKnownFactContradictionAndClosesGeneration();
    void secondaryMetadataTargetPolicyIgnoresSupersededInitialRequest();
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
    providerFrameRenderAcknowledgementCommitsFromControllerSnapshot()
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

    StubProviderSession session;
    QVERIFY(controller.installProviderSession(&session) != 0);
    const ViewportProviderSessionOpenResult opened = controller.handleProviderSessionOpened();
    QCOMPARE(opened.providerFrameTransport.sendCommand, true);
    const ImageSequenceProviderRequestToken frameToken
        = opened.providerFrameTransport.command.token;
    QVERIFY(frameToken.isValid());

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    const ImageViewportInternal::ViewportChangeSet frameChanges
        = controller.handleProviderFrameEvent(
            { frameToken }, &frame, ImageSequenceProviderFrameMetadata::stillFrame());
    QCOMPARE(frameChanges.requestState, true);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Loading);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::UploadPending);
    QVERIFY(controller.displayState().pendingRenderPayload.commitPending);

    const ImageViewportInternal::PreparedPayloadIdentity payload
        = controller.displayState().pendingRenderPayload.identity();
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    const ImageViewportInternal::ViewportChangeSet commitChanges
        = controller.acknowledgeRenderCommit({ payload }, true, synchronization);

    QCOMPARE(commitChanges.requestState, true);
    QCOMPARE(commitChanges.displayState, true);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Ready);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::Ready);
    QCOMPARE(controller.displayState().status, ImageViewport::DisplayStatus::Ready);
    QCOMPARE(controller.displayState().displayedRequest.request.preparedPayloadId,
        payload.payloadId);
    QCOMPARE(controller.displayState().pendingRenderPayload.commitPending, false);
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

void ViewportControllerProviderTest::requiredRoleWaitPriorityAggregatesBeforeProjection_data()
{
    QTest::addColumn<bool>("requiresSecondary");
    QTest::addColumn<bool>("primaryProviderWaiting");
    QTest::addColumn<bool>("primaryRequestQueued");
    QTest::addColumn<bool>("primaryUploadPending");
    QTest::addColumn<bool>("primaryRenderWaiting");
    QTest::addColumn<bool>("secondaryProviderWaiting");
    QTest::addColumn<bool>("secondaryRequestQueued");
    QTest::addColumn<bool>("secondaryUploadPending");
    QTest::addColumn<bool>("secondaryRenderWaiting");
    QTest::addColumn<int>("expectedReason");

    QTest::newRow("provider-waiting-over-upload")
        << true << false << false << true << false << true << false << false << false
        << static_cast<int>(ImageViewport::RequestReason::ProviderWaiting);
    QTest::newRow("queued-over-upload-and-render")
        << true << false << false << true << false << false << true << false << true
        << static_cast<int>(ImageViewport::RequestReason::RequestQueued);
    QTest::newRow("upload-over-render")
        << true << false << false << false << true << false << false << true << false
        << static_cast<int>(ImageViewport::RequestReason::UploadPending);
    QTest::newRow("render-only")
        << true << false << false << false << true << false << false << false << true
        << static_cast<int>(ImageViewport::RequestReason::RenderWaiting);
    QTest::newRow("non-required-secondary-ignored")
        << false << false << false << true << false << true << true << false << false
        << static_cast<int>(ImageViewport::RequestReason::UploadPending);
}

void ViewportControllerProviderTest::requiredRoleWaitPriorityAggregatesBeforeProjection()
{
    QFETCH(bool, requiresSecondary);
    QFETCH(bool, primaryProviderWaiting);
    QFETCH(bool, primaryRequestQueued);
    QFETCH(bool, primaryUploadPending);
    QFETCH(bool, primaryRenderWaiting);
    QFETCH(bool, secondaryProviderWaiting);
    QFETCH(bool, secondaryRequestQueued);
    QFETCH(bool, secondaryUploadPending);
    QFETCH(bool, secondaryRenderWaiting);
    QFETCH(int, expectedReason);

    ImageViewportInternal::TargetSpreadWaitState waitState;
    waitState.requiresSecondary = requiresSecondary;
    waitState.primary.providerWaiting = primaryProviderWaiting;
    waitState.primary.requestQueued = primaryRequestQueued;
    waitState.primary.uploadPending = primaryUploadPending;
    waitState.primary.renderWaiting = primaryRenderWaiting;
    waitState.secondary.providerWaiting = secondaryProviderWaiting;
    waitState.secondary.requestQueued = secondaryRequestQueued;
    waitState.secondary.uploadPending = secondaryUploadPending;
    waitState.secondary.renderWaiting = secondaryRenderWaiting;

    QCOMPARE(ImageViewportInternal::projectWaitReason(waitState),
        static_cast<ImageViewport::RequestReason>(expectedReason));
}

void ViewportControllerProviderTest::failureScopeTableClassifiesTerminalInputs_data()
{
    QTest::addColumn<int>("terminalCase");
    QTest::addColumn<bool>("generationTerminal");

    QTest::newRow("session-open-failure")
        << static_cast<int>(TerminalScopeCase::SessionOpenFailure) << true;
    QTest::newRow("metadata-production-failure")
        << static_cast<int>(TerminalScopeCase::MetadataProductionFailure) << true;
    QTest::newRow("malformed-metadata")
        << static_cast<int>(TerminalScopeCase::MalformedMetadata) << true;
    QTest::newRow("contradictory-metadata")
        << static_cast<int>(TerminalScopeCase::ContradictoryMetadata) << true;
    QTest::newRow("metadata-target-unsupported")
        << static_cast<int>(TerminalScopeCase::MetadataTargetUnsupported) << false;
    QTest::newRow("frame-unsupported")
        << static_cast<int>(TerminalScopeCase::FrameUnsupported) << false;
    QTest::newRow("frame-provider-failure")
        << static_cast<int>(TerminalScopeCase::FrameProviderFailure) << false;
    QTest::newRow("frame-admission-failure")
        << static_cast<int>(TerminalScopeCase::FrameAdmissionFailure) << false;
    QTest::newRow("metadata-end-of-sequence-protocol-violation")
        << static_cast<int>(TerminalScopeCase::MetadataEndOfSequenceProtocolViolation) << true;
    QTest::newRow("frame-end-of-sequence-protocol-violation")
        << static_cast<int>(TerminalScopeCase::FrameEndOfSequenceProtocolViolation) << false;
    QTest::newRow("render-failure")
        << static_cast<int>(TerminalScopeCase::RenderFailure) << false;
}

void ViewportControllerProviderTest::failureScopeTableClassifiesTerminalInputs()
{
    QFETCH(int, terminalCase);
    QFETCH(bool, generationTerminal);

    const auto scopeCase = static_cast<TerminalScopeCase>(terminalCase);
    using UnsupportedCause = ImageSequenceProviderSession::UnsupportedCause;
    ImageSequenceFactory factory;
    ProviderControllerContext context;
    if (scopeCase == TerminalScopeCase::ContradictoryMetadata) {
        context.knownFacts = ImageSequenceProviderKnownFacts::logicalSize(QSizeF(16.0, 8.0));
    }
    const ImageSequenceProviderMetadata knownMetadata
        = scopeCase == TerminalScopeCase::RenderFailure
        ? ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0))
        : ImageSequenceProviderMetadata {};
    std::unique_ptr<ImageSequenceFactoryResult> sequence
        = makeProviderSequence(factory, context, knownMetadata);
    QVERIFY(sequence);
    ViewportController controller(context);

    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    const ViewportSequenceAssignmentResult assigned = controller.assignSequence(assignment);
    QCOMPARE(assigned.openProviderSession, true);

    if (scopeCase == TerminalScopeCase::SessionOpenFailure) {
        const ImageViewportInternal::ViewportChangeSet changes
            = controller.handleProviderSessionOpenFailure(QStringLiteral("session failed"));
        QCOMPARE(changes.requestState, true);
    } else {
        StubProviderSession session;
        QVERIFY(controller.installProviderSession(&session) != 0);
        const ViewportProviderSessionOpenResult opened = controller.handleProviderSessionOpened();
        if (scopeCase == TerminalScopeCase::RenderFailure) {
            QCOMPARE(opened.providerFrameTransport.sendCommand, true);
            const ImageSequenceProviderRequestToken frameToken
                = opened.providerFrameTransport.command.token;
            QVERIFY(frameToken.isValid());
            QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            ImageFrame frame(image);
            const ImageViewportInternal::ViewportChangeSet frameChanges
                = controller.handleProviderFrameEvent(
                    { frameToken }, &frame, ImageSequenceProviderFrameMetadata::stillFrame());
            QCOMPARE(frameChanges.requestState, true);
            QVERIFY(controller.displayState().pendingRenderPayload.commitPending);
            const ImageViewportInternal::PreparedPayloadIdentity payload
                = controller.displayState().pendingRenderPayload.identity();
            const ImageViewportInternal::ViewportChangeSet renderFailure
                = controller.acknowledgeRenderFailure(
                    { payload, {}, ImageViewport::PageRole::Primary,
                        RenderFailureCause::TextureCreationFailure });
            QCOMPARE(renderFailure.requestState, true);
        } else {
            const ImageSequenceProviderRequestToken metadataToken
                = opened.providerMetadataTransport.token;
            QVERIFY(metadataToken.isValid());

            if (scopeCase == TerminalScopeCase::MetadataProductionFailure) {
                const ViewportProviderTerminalEventResult terminal
                    = controller.handleProviderTerminalEvent(
                        { metadataToken, ViewportProviderTerminalEvent::Kind::Failure,
                            UnsupportedCause::PayloadRejection,
                            QStringLiteral("metadata failed") });
                QCOMPARE(terminal.changes.requestState, true);
            } else if (scopeCase == TerminalScopeCase::MetadataEndOfSequenceProtocolViolation) {
                const ViewportProviderEndOfSequenceResult endOfSequence
                    = controller.handleProviderEndOfSequenceEvent({ metadataToken });
                QCOMPARE(endOfSequence.changes.requestState, true);
            } else {
                QCOMPARE(controller.acceptProviderMetadataEvent({ metadataToken }).accepted, true);
                ImageSequenceProviderMetadata metadata
                    = ImageSequenceProviderMetadata::timedFrameList(
                        QSizeF(16.0, 8.0), { 100, 250 });
                if (scopeCase == TerminalScopeCase::MalformedMetadata) {
                    metadata = {};
                } else if (scopeCase == TerminalScopeCase::ContradictoryMetadata) {
                    metadata = ImageSequenceProviderMetadata::still(QSizeF(8.0, 16.0));
                } else if (scopeCase == TerminalScopeCase::MetadataTargetUnsupported) {
                    const ViewportCommandResult play = controller.play();
                    QCOMPARE(play.outcome, ImageViewport::CommandOutcome::Accepted);
                    metadata.setTimedPlaybackSupport(false);
                    metadata.setPositionSeekSupport(false);
                }

                const ViewportProviderMetadataAdmissionResult admission
                    = controller.handleProviderMetadataAdmission(metadata);
                if (!admission.accepted) {
                    QCOMPARE(admission.changes.requestState, true);
                } else {
                    controller.handleProviderAcceptedMetadataFacts(admission.facts);
                    const ViewportProviderMetadataTargetPolicyResult targetPolicy
                        = controller.handleProviderMetadataTargetPolicy(admission.facts);
                    if (scopeCase == TerminalScopeCase::MetadataTargetUnsupported) {
                        QCOMPARE(targetPolicy.changes.requestState, true);
                    } else {
                        QCOMPARE(targetPolicy.providerFrameTransport.sendCommand, true);
                        const ImageSequenceProviderRequestToken frameToken
                            = targetPolicy.providerFrameTransport.command.token;
                        QVERIFY(frameToken.isValid());

                        if (scopeCase == TerminalScopeCase::FrameUnsupported) {
                            const ViewportProviderTerminalEventResult terminal
                                = controller.handleProviderTerminalEvent(
                                    { frameToken,
                                        ViewportProviderTerminalEvent::Kind::Unsupported,
                                        UnsupportedCause::UnsupportedRequest,
                                        QStringLiteral("frame unsupported") });
                            QCOMPARE(terminal.changes.requestState, true);
                        } else if (scopeCase == TerminalScopeCase::FrameProviderFailure) {
                            const ViewportProviderTerminalEventResult terminal
                                = controller.handleProviderTerminalEvent(
                                    { frameToken, ViewportProviderTerminalEvent::Kind::Failure,
                                        UnsupportedCause::PayloadRejection,
                                        QStringLiteral("frame failed") });
                            QCOMPARE(terminal.changes.requestState, true);
                        } else if (scopeCase == TerminalScopeCase::FrameAdmissionFailure) {
                            const ImageViewportInternal::ViewportChangeSet changes
                                = controller.handleProviderFrameEvent({ frameToken }, nullptr,
                                    ImageSequenceProviderFrameMetadata::timedFrame(0, 0, 100));
                            QCOMPARE(changes.requestState, true);
                        } else if (scopeCase
                            == TerminalScopeCase::FrameEndOfSequenceProtocolViolation) {
                            const ViewportProviderEndOfSequenceResult endOfSequence
                                = controller.handleProviderEndOfSequenceEvent({ frameToken });
                            QCOMPARE(endOfSequence.changes.requestState, true);
                        }
                    }
                }
            }
        }
    }

    QVERIFY(controller.requestState().status == ImageViewport::RequestStatus::Error
        || controller.requestState().status == ImageViewport::RequestStatus::Unsupported);
    if (scopeCase == TerminalScopeCase::RenderFailure) {
        QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::RenderFailure);
    }

    const ViewportCommandResult seek = controller.seek(0);
    QCOMPARE(seek.outcome, generationTerminal ? ImageViewport::CommandOutcome::Unsupported
                                              : ImageViewport::CommandOutcome::Accepted);
}

void ViewportControllerProviderTest::
    secondaryMetadataAdmissionRejectsKnownFactContradictionAndClosesGeneration()
{
    ImageSequenceFactory factory;
    ProviderControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> primary = makeProviderSequence(factory, context);
    QVERIFY(primary);
    std::unique_ptr<ImageSequenceFactoryResult> secondary = makeDetachedProviderSequence(factory);
    QVERIFY(secondary);
    context.secondaryKnownFacts = ImageSequenceProviderKnownFacts::logicalSize(QSizeF(16.0, 8.0));
    ViewportController controller(context);

    ViewportSequenceAssignment assignment;
    assignment.sequence = primary->sequence();
    assignment.secondarySequence = secondary->sequence();
    assignment.secondarySource.present = true;
    assignment.secondarySource.provider = true;
    const ViewportSequenceAssignmentResult assigned = controller.assignSequence(assignment);
    QCOMPARE(assigned.openSecondaryProviderSession, true);

    StubProviderSession session;
    QVERIFY(controller.installProviderSession(ImageViewport::PageRole::Secondary, &session) != 0);
    const ViewportProviderSessionOpenResult opened
        = controller.handleProviderSessionOpened(ImageViewport::PageRole::Secondary);
    const ImageSequenceProviderRequestToken metadataToken = opened.providerMetadataTransport.token;
    QVERIFY(metadataToken.isValid());
    QCOMPARE(controller.acceptSecondaryProviderMetadataEvent({ metadataToken }).accepted, true);

    const ViewportProviderMetadataAdmissionResult admission
        = controller.handleSecondaryProviderMetadataAdmission(
            ImageSequenceProviderMetadata::still(QSizeF(8.0, 16.0)));
    QCOMPARE(admission.accepted, false);
    QCOMPARE(admission.providerFrameTransport.closeSession, true);
    QCOMPARE(controller.secondaryProviderMetadataReady(), false);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Error);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::PayloadRejection);
    QVERIFY(controller.requestState().errorString.contains(QStringLiteral("construction-time")));
}

void ViewportControllerProviderTest::
    secondaryMetadataTargetPolicyIgnoresSupersededInitialRequest()
{
    ImageSequenceFactory factory;
    ProviderControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> primary = makeProviderSequence(factory, context);
    QVERIFY(primary);
    std::unique_ptr<ImageSequenceFactoryResult> secondary = makeDetachedProviderSequence(factory);
    QVERIFY(secondary);
    ViewportController controller(context);

    ViewportSequenceAssignment assignment;
    assignment.sequence = primary->sequence();
    assignment.secondarySequence = secondary->sequence();
    assignment.secondarySource.present = true;
    assignment.secondarySource.provider = true;
    controller.assignSequence(assignment);

    StubProviderSession session;
    controller.installProviderSession(ImageViewport::PageRole::Secondary, &session);
    const ViewportProviderSessionOpenResult opened
        = controller.handleProviderSessionOpened(ImageViewport::PageRole::Secondary);
    const ImageSequenceProviderRequestToken metadataToken = opened.providerMetadataTransport.token;
    QVERIFY(metadataToken.isValid());

    const ViewportCommandResult seek = controller.seek(0);
    QCOMPARE(seek.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.requestState().activeRequest.identity.origin,
        ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek);
    QCOMPARE(controller.requestState().secondaryActiveRequest.target.frame, -1);

    QCOMPARE(controller.acceptSecondaryProviderMetadataEvent({ metadataToken }).accepted, true);
    const ViewportProviderMetadataAdmissionResult admission
        = controller.handleSecondaryProviderMetadataAdmission(
            ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QCOMPARE(admission.accepted, true);
    controller.handleSecondaryProviderAcceptedMetadataFacts(admission.facts);
    const ViewportProviderMetadataTargetPolicyResult targetPolicy
        = controller.handleSecondaryProviderMetadataTargetPolicy(admission.facts);
    QCOMPARE(targetPolicy.providerFrameTransport.sendCommand, false);
    QCOMPARE(controller.secondaryProviderMetadataReady(), true);
    QCOMPARE(controller.requestState().secondaryActiveRequest.target.frame, -1);
}

QTEST_MAIN(ViewportControllerProviderTest)

#include "tst_viewportcontroller_provider.moc"
