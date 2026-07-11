#include "imageviewport.h"
#include "viewportcontroller_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollerrendercontract_p.h"
#include "viewportplaybackcontract_p.h"

#include <QtTest/QTest>

#include <memory>
#include <utility>

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
    explicit StubProviderAdapter(QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
    {
    }

    ImageSequenceProviderDescriptor descriptor() const override
    {
        ImageSequenceProviderDescriptor descriptor;
        descriptor.setSessionFactory(std::make_shared<StubProviderSessionFactory>());
        return descriptor;
    }
};

enum class RoleCommandAdmissionCase {
    MalformedRole,
    AbsentSecondaryRole,
    NegativeTarget,
    OutOfRangeTarget,
    UnsupportedCapability,
    GenerationTerminalFailure,
    DisplayRequestTerminalFailure,
    AcceptedValidTarget,
};

enum class RoleCommandKind {
    Play,
    SeekFrame,
    SeekPosition,
};

class PlaybackControllerContext final
{
public:
    ImageSequence* sequence = nullptr;
    ImageSequence* secondarySequence = nullptr;
    bool providerSequence = false;
    bool timed = true;
    QSizeF size { 100.0, 100.0 };
    QSizeF logicalSize { 16.0, 8.0 };
    QVector<int> durations { 100, 250 };
    QVector<int> secondaryDurations { 100, 250 };
    QVector<QImage> images;
    QVector<QImage> secondaryImages;

    QRectF contentRect() const
    {
        return itemBounds().isEmpty() ? QRectF() : QRectF(0.0, 25.0, size.width(), 50.0);
    }

    QRectF visibleImageRect() const
    {
        return itemBounds().isEmpty() ? QRectF() : QRectF(0.0, 0.0, 16.0, 8.0);
    }

    QRectF itemBounds() const
    {
        if (size.width() <= 0.0 || size.height() <= 0.0) {
            return {};
        }
        return QRectF(0.0, 0.0, size.width(), size.height());
    }

    bool hasActiveRequest() const { return sequence != nullptr; }
    bool hasDisplayableSequence() const { return sequence != nullptr; }
    bool hasProviderSequence() const { return sequence != nullptr && providerSequence; }
    bool hasTimedSequence() const { return sequence != nullptr && timed; }
    int sequenceFrameCount() const { return durations.size(); }
    QSizeF sequenceLogicalSize() const { return logicalSize; }
    double width() const { return size.width(); }
    double height() const { return size.height(); }
    bool hasSecondaryTimedSequence() const { return secondarySequence != nullptr; }
    int secondarySequenceFrameCount() const { return secondaryDurations.size(); }
    int secondarySequenceTotalDuration() const { return totalDurationFor(secondaryDurations); }
    QSizeF secondarySequenceLogicalSize() const { return logicalSize; }

    int sequenceFrameStartPosition(int frame) const
    {
        return frameStartPositionFor(durations, frame);
    }

    int sequenceFrameIndexForPosition(int position) const
    {
        return frameIndexForPositionIn(durations, position);
    }

    int secondarySequenceFrameStartPosition(int frame) const
    {
        return frameStartPositionFor(secondaryDurations, frame);
    }

    int secondarySequenceFrameIndexForPosition(int position) const
    {
        return frameIndexForPositionIn(secondaryDurations, position);
    }

private:
    static int totalDurationFor(const QVector<int>& frameDurations)
    {
        int total = 0;
        for (int duration : frameDurations) {
            total += duration;
        }
        return total;
    }

    static int frameStartPositionFor(const QVector<int>& frameDurations, int frame)
    {
        if (frame < 0 || frame >= frameDurations.size()) {
            return -1;
        }
        int position = 0;
        for (int index = 0; index < frame; ++index) {
            position += frameDurations.at(index);
        }
        return position;
    }

    static int frameIndexForPositionIn(const QVector<int>& frameDurations, int position)
    {
        if (position < 0) {
            return -1;
        }
        int start = 0;
        for (int index = 0; index < frameDurations.size(); ++index) {
            const int end = start + frameDurations.at(index);
            if (position >= start && position < end) {
                return index;
            }
            start = end;
        }
        return position == start && !frameDurations.isEmpty() ? frameDurations.size() - 1 : -1;
    }

    int sequenceTotalDuration() const { return totalDurationFor(durations); }
};

std::unique_ptr<ImageSequenceFactoryResult> makeTimedSequenceFor(
    ImageSequenceFactory& factory, QVector<QImage>& retainedImages, const QVector<int>& durations)
{
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    if (!list.appendFrame(&firstFrame, durations.at(0))
        || !list.appendFrame(&secondFrame, durations.at(1))) {
        return {};
    }
    retainedImages = { firstImage, secondImage };
    auto result = std::unique_ptr<ImageSequenceFactoryResult>(factory.fromTimedFrameList(&list));
    if (!result || !result->sequence()) {
        return {};
    }
    return result;
}

std::unique_ptr<ImageSequenceFactoryResult> makeTimedSequence(
    ImageSequenceFactory& factory, PlaybackControllerContext& context)
{
    auto result = makeTimedSequenceFor(factory, context.images, context.durations);
    if (!result || !result->sequence()) {
        return {};
    }
    context.sequence = result->sequence();
    return result;
}

std::unique_ptr<ImageSequenceFactoryResult> makeStillSequence(
    ImageSequenceFactory& factory, PlaybackControllerContext& context)
{
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    auto result = std::unique_ptr<ImageSequenceFactoryResult>(factory.fromFrame(&frame));
    if (!result || !result->sequence()) {
        return {};
    }
    context.sequence = result->sequence();
    context.timed = false;
    context.durations = { 100 };
    context.images = { image };
    return result;
}

std::unique_ptr<ImageSequenceFactoryResult> makeSecondaryTimedSequence(
    ImageSequenceFactory& factory, PlaybackControllerContext& context)
{
    auto result
        = makeTimedSequenceFor(factory, context.secondaryImages, context.secondaryDurations);
    if (!result || !result->sequence()) {
        return {};
    }
    context.secondarySequence = result->sequence();
    return result;
}

std::unique_ptr<ImageSequenceFactoryResult> makeProviderSequence(
    ImageSequenceFactory& factory, PlaybackControllerContext& context)
{
    StubProviderAdapter adapter;
    auto result = std::unique_ptr<ImageSequenceFactoryResult>(factory.fromProvider(&adapter));
    if (!result || !result->sequence()) {
        return {};
    }
    context.sequence = result->sequence();
    context.providerSequence = true;
    return result;
}

void acknowledgePendingRenderCommit(ViewportController& controller)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    if (!synchronization.pendingTargetCommit) {
        return;
    }
    const ImageViewportInternal::PreparedPayloadIdentity primaryPayload
        = controller.displayState().roles[0].pendingRenderPayload.identity().isValid()
        ? controller.displayState().roles[0].pendingRenderPayload.identity()
        : synchronization.preparedPayload.identity();
    QVector<ViewportRenderRolePayload> rolePayloads { { ImageViewport::PageRole::Primary,
        primaryPayload } };
    if (controller.requestState().roles[1].sequence
        && controller.requestState().roles[1].activeRequest.target.frame >= 0) {
        const ImageViewportInternal::PreparedPayloadIdentity secondaryPayload
            = controller.displayState().roles[1].pendingRenderPayload.identity().isValid()
            ? controller.displayState().roles[1].pendingRenderPayload.identity()
            : primaryPayload;
        rolePayloads.append({ ImageViewport::PageRole::Secondary, secondaryPayload });
    }
    controller.acknowledgeRenderCommit({ primaryPayload, rolePayloads }, true, synchronization);
}

void failPendingRenderCommit(ViewportController& controller)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    QVERIFY(synchronization.pendingTargetCommit);
    const ImageViewportInternal::PreparedPayloadIdentity primaryPayload
        = controller.displayState().roles[0].pendingRenderPayload.identity().isValid()
        ? controller.displayState().roles[0].pendingRenderPayload.identity()
        : synchronization.preparedPayload.identity();
    QVERIFY(primaryPayload.isValid());
    controller.acknowledgeRenderFailure({ primaryPayload, {}, ImageViewport::PageRole::Primary,
        RenderFailureCause::TextureCreationFailure });
}

ViewportCommandResult invokeRoleCommand(
    ViewportController& controller, RoleCommandKind kind, ImageViewport::PageRole role, int value)
{
    switch (kind) {
    case RoleCommandKind::Play:
        return controller.play(role);
    case RoleCommandKind::SeekFrame:
        return controller.seek(role, value);
    case RoleCommandKind::SeekPosition:
        return controller.seekToPosition(role, value);
    }
    return {};
}

} // namespace

class ViewportControllerPlaybackTest : public QObject
{
    Q_OBJECT

public:
    explicit ViewportControllerPlaybackTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void roleCommandAdmissionOrder_data();
    void roleCommandAdmissionOrder();
    void builtInPlaybackAdvanceUsesExplicitElapsedWithoutTimer();
    void pauseWhileRenderWaitingCommitsWithoutResumingPlayback();
    void explicitSeekWhilePlayingWaitsForRenderCommit();
    void loopingPlaybackWrapsToStart();
    void unsupportedPlayForUntimedSequencePreservesStoppedPhase();
    void invalidSeekWhilePlayingPreservesPlaybackPhase();
};

void ViewportControllerPlaybackTest::roleCommandAdmissionOrder_data()
{
    QTest::addColumn<int>("admissionCase");
    QTest::addColumn<int>("commandKind");
    QTest::addColumn<int>("role");
    QTest::addColumn<int>("value");
    QTest::addColumn<int>("expectedOutcome");
    QTest::addColumn<int>("expectedCommandReason");

    const auto addRow = [](const char* name, RoleCommandAdmissionCase admissionCase,
                            RoleCommandKind commandKind, ImageViewport::PageRole role, int value,
                            ImageViewport::CommandOutcome expectedOutcome,
                            ImageViewport::CommandReason expectedCommandReason) {
        QTest::newRow(name) << static_cast<int>(admissionCase) << static_cast<int>(commandKind)
                            << static_cast<int>(role) << value << static_cast<int>(expectedOutcome)
                            << static_cast<int>(expectedCommandReason);
    };

    addRow("malformed-role-before-no-request", RoleCommandAdmissionCase::MalformedRole,
        RoleCommandKind::SeekFrame,
        static_cast<ImageViewport::PageRole>(
            99), // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
        0, ImageViewport::CommandOutcome::Invalid, ImageViewport::CommandReason::InvalidRequest);
    addRow("absent-secondary-role", RoleCommandAdmissionCase::AbsentSecondaryRole,
        RoleCommandKind::SeekFrame, ImageViewport::PageRole::Secondary, 0,
        ImageViewport::CommandOutcome::IgnoredNoRequest,
        ImageViewport::CommandReason::IgnoredNoRequest);
    addRow("negative-target-before-failure-scope", RoleCommandAdmissionCase::NegativeTarget,
        RoleCommandKind::SeekFrame, ImageViewport::PageRole::Primary, -1,
        ImageViewport::CommandOutcome::Invalid, ImageViewport::CommandReason::InvalidRequest);
    addRow("known-out-of-range-before-failure-scope", RoleCommandAdmissionCase::OutOfRangeTarget,
        RoleCommandKind::SeekFrame, ImageViewport::PageRole::Primary, 2,
        ImageViewport::CommandOutcome::Invalid, ImageViewport::CommandReason::InvalidRequest);
    addRow("unsupported-capability-after-valid-input",
        RoleCommandAdmissionCase::UnsupportedCapability, RoleCommandKind::SeekPosition,
        ImageViewport::PageRole::Primary, 0, ImageViewport::CommandOutcome::Unsupported,
        ImageViewport::CommandReason::UnsupportedRequest);
    addRow("generation-terminal-after-valid-input",
        RoleCommandAdmissionCase::GenerationTerminalFailure, RoleCommandKind::SeekFrame,
        ImageViewport::PageRole::Primary, 0, ImageViewport::CommandOutcome::Unsupported,
        ImageViewport::CommandReason::UnsupportedRequest);
    addRow("display-request-terminal-allows-valid-seek",
        RoleCommandAdmissionCase::DisplayRequestTerminalFailure, RoleCommandKind::SeekFrame,
        ImageViewport::PageRole::Primary, 0, ImageViewport::CommandOutcome::Accepted,
        ImageViewport::CommandReason::NoCommand);
    addRow("accepted-secondary-valid-target", RoleCommandAdmissionCase::AcceptedValidTarget,
        RoleCommandKind::SeekFrame, ImageViewport::PageRole::Secondary, 1,
        ImageViewport::CommandOutcome::Accepted, ImageViewport::CommandReason::NoCommand);
}

void ViewportControllerPlaybackTest::roleCommandAdmissionOrder()
{
    QFETCH(int, admissionCase);
    QFETCH(int, commandKind);
    QFETCH(int, role);
    QFETCH(int, value);
    QFETCH(int, expectedOutcome);
    QFETCH(int, expectedCommandReason);

    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    ViewportController controller([&context] { return context.itemBounds(); });
    std::unique_ptr<ImageSequenceFactoryResult> primarySequence;
    std::unique_ptr<ImageSequenceFactoryResult> secondarySequence;

    switch (static_cast<RoleCommandAdmissionCase>(admissionCase)) {
    case RoleCommandAdmissionCase::MalformedRole:
        break;
    case RoleCommandAdmissionCase::UnsupportedCapability:
        primarySequence = makeStillSequence(factory, context);
        QVERIFY(primarySequence);
        controller.assignSequence({ primarySequence->sequence() });
        break;
    case RoleCommandAdmissionCase::GenerationTerminalFailure: {
        primarySequence = makeProviderSequence(factory, context);
        QVERIFY(primarySequence);
        controller.assignSequence({ primarySequence->sequence() });
        ViewportProviderHostEvent failure;
        failure.kind = ViewportProviderHostEvent::Kind::SessionOpenFailed;
        failure.role = ImageViewport::PageRole::Primary;
        failure.diagnostic = QStringLiteral("session failed");
        controller.handleProviderHostEvent(failure);
        QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Error);
        break;
    }
    case RoleCommandAdmissionCase::DisplayRequestTerminalFailure:
        primarySequence = makeTimedSequence(factory, context);
        QVERIFY(primarySequence);
        controller.assignSequence({ primarySequence->sequence() });
        failPendingRenderCommit(controller);
        QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Error);
        break;
    case RoleCommandAdmissionCase::AcceptedValidTarget: {
        primarySequence = makeTimedSequence(factory, context);
        secondarySequence = makeSecondaryTimedSequence(factory, context);
        QVERIFY(primarySequence);
        QVERIFY(secondarySequence);
        ViewportSequenceAssignment assignment;
        assignment.sequence = primarySequence->sequence();
        assignment.secondarySequence = secondarySequence->sequence();
        controller.assignSequence(assignment);
        QCOMPARE(controller.requestState().roles[1].activeRequest.target.frame, 0);
        QCOMPARE(controller.requestState().roles[1].activeRequest.target.position, 0);
        break;
    }
    case RoleCommandAdmissionCase::AbsentSecondaryRole:
    case RoleCommandAdmissionCase::NegativeTarget:
    case RoleCommandAdmissionCase::OutOfRangeTarget:
        primarySequence = makeTimedSequence(factory, context);
        QVERIFY(primarySequence);
        controller.assignSequence({ primarySequence->sequence() });
        break;
    }

    const ViewportCommandResult result
        = invokeRoleCommand(controller, static_cast<RoleCommandKind>(commandKind),
            static_cast<ImageViewport::PageRole>(role), value);
    QCOMPARE(result.outcome, static_cast<ImageViewport::CommandOutcome>(expectedOutcome));
    QCOMPARE(controller.requestState().commandReason,
        static_cast<ImageViewport::CommandReason>(expectedCommandReason));
}

void ViewportControllerPlaybackTest::builtInPlaybackAdvanceUsesExplicitElapsedWithoutTimer()
{
    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeTimedSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller([&context] { return context.itemBounds(); });

    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Ready);
    QCOMPARE(controller.requestState().roles[0].activeRequest.target.frame, 0);
    QCOMPARE(controller.requestState().playbackPosition, 0);

    const ViewportCommandResult play = controller.play(ImageViewport::PageRole::Primary);
    QCOMPARE(play.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Playing);

    controller.advancePlayback(99);
    QCOMPARE(controller.requestState().roles[0].activeRequest.target.frame, 0);
    QCOMPARE(controller.requestState().playbackPosition, 99);

    context.size = QSizeF(0.0, 100.0);
    controller.advancePlayback(1);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Waiting);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Loading);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::RenderWaiting);
    QCOMPARE(controller.requestState().roles[0].activeRequest.target.frame, 1);
    QCOMPARE(controller.requestState().playbackPosition, 100);

    controller.advancePlayback(1000);
    QCOMPARE(controller.requestState().roles[0].activeRequest.target.frame, 1);
    QCOMPARE(controller.requestState().playbackPosition, 100);

    context.size = QSizeF(100.0, 100.0);
    controller.handleGeometryChanged({}, {});
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Playing);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Ready);
    QCOMPARE(controller.displayState().roles[0].displayedRequest.request.target.frame, 1);

    controller.advancePlayback(249);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Playing);
    QCOMPARE(controller.requestState().playbackPosition, 349);
    controller.advancePlayback(1);
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Stopped);
    QCOMPARE(controller.requestState().roles[0].activeRequest.target.frame, 1);
    QCOMPARE(controller.requestState().playbackPosition, 350);
}

void ViewportControllerPlaybackTest::pauseWhileRenderWaitingCommitsWithoutResumingPlayback()
{
    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeTimedSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller([&context] { return context.itemBounds(); });

    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);
    controller.play(ImageViewport::PageRole::Primary);
    context.size = QSizeF(0.0, 100.0);
    controller.advancePlayback(100);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Waiting);

    const ViewportCommandResult pause = controller.pause(ImageViewport::PageRole::Primary);
    QCOMPARE(pause.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Paused);

    context.size = QSizeF(100.0, 100.0);
    controller.handleGeometryChanged({}, {});
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Ready);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Paused);
    QCOMPARE(controller.displayState().roles[0].displayedRequest.request.target.frame, 1);
}

void ViewportControllerPlaybackTest::explicitSeekWhilePlayingWaitsForRenderCommit()
{
    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeTimedSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller([&context] { return context.itemBounds(); });

    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);
    controller.play(ImageViewport::PageRole::Primary);
    context.size = QSizeF(0.0, 100.0);

    const ViewportCommandResult seek = controller.seek(ImageViewport::PageRole::Primary, 1);
    QCOMPARE(seek.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Waiting);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Loading);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::RenderWaiting);
    QCOMPARE(controller.requestState().roles[0].activeRequest.target.frame, 1);
    QCOMPARE(controller.requestState().roles[0].activeRequest.target.position, 100);
}

void ViewportControllerPlaybackTest::loopingPlaybackWrapsToStart()
{
    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeTimedSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller([&context] { return context.itemBounds(); });

    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);
    ImageViewportPresentationCommand loopingCommand;
    loopingCommand.setLooping(true);
    QCOMPARE(controller.setPresentation({ loopingCommand, {}, 1.0 }).outcome,
        ImageViewport::CommandOutcome::Accepted);
    controller.play(ImageViewport::PageRole::Primary);

    controller.advancePlayback(350);
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Playing);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Ready);
    QCOMPARE(controller.requestState().roles[0].activeRequest.target.frame, 0);
    QCOMPARE(controller.requestState().roles[0].activeRequest.target.position, 0);
    QCOMPARE(controller.requestState().playbackPosition, 0);
}

void ViewportControllerPlaybackTest::unsupportedPlayForUntimedSequencePreservesStoppedPhase()
{
    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeStillSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller([&context] { return context.itemBounds(); });

    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);

    const ViewportCommandResult play = controller.play(ImageViewport::PageRole::Primary);
    QCOMPARE(play.outcome, ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Stopped);
    QCOMPARE(
        controller.requestState().commandReason, ImageViewport::CommandReason::UnsupportedRequest);
}

void ViewportControllerPlaybackTest::invalidSeekWhilePlayingPreservesPlaybackPhase()
{
    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeTimedSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller([&context] { return context.itemBounds(); });

    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);
    controller.play(ImageViewport::PageRole::Primary);

    const ViewportCommandResult invalidSeek = controller.seek(ImageViewport::PageRole::Primary, -1);
    QCOMPARE(invalidSeek.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Playing);
    QCOMPARE(controller.requestState().roles[0].activeRequest.target.frame, 0);
    QCOMPARE(controller.requestState().playbackPosition, 0);
}

QTEST_MAIN(ViewportControllerPlaybackTest)

#include "tst_viewportcontroller_playback.moc"
