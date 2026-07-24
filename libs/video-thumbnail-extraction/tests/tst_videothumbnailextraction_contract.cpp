// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video_thumbnail_extraction_test_support.h"

#include <QColor>
#include <QObject>
#include <QTest>

#include <array>
#include <limits>
#include <optional>

namespace {

using kiriview::VideoThumbnailExtractionFailureCause;
using kiriview::VideoThumbnailExtractionResult;
using kiriview::VideoThumbnailExtractionStatus;
using kiriview::detail::VideoThumbnailBackendError;
using kiriview::detail::VideoThumbnailBackendMediaFacts;
using kiriview::detail::VideoThumbnailBackendMediaStatus;
using kiriview::detail::VideoThumbnailEmbeddedImages;
using kiriview::test::ExtractionHarness;

auto scalarCount(const QString& value) -> qsizetype { return value.toUcs4().size(); }

class VideoThumbnailExtractionContractTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void invalidRequestsDoNotStartResources_data();
    void invalidRequestsDoNotStartResources();
    void admittedRequestStartsBoundedExtraction();
    void coverPrecedesThumbnailAndOutputIsBounded();
    void unusableCoverFallsBackToThumbnail();
    void frameOutputIsDetachedFromBackendStorage();
    void nonSeekableMediaUsesFirstUsableFrame();
    void oversizedFrameDoesNotMaterialize();
    void admissibleFrameMaterializesOnce();
    void frameConversionFailureIsTyped();
    void resourceLimitIsTyped();
    void backendFailuresAreTyped_data();
    void backendFailuresAreTyped();
    void deadlineAndExhaustionAreTyped();
};

void VideoThumbnailExtractionContractTest::invalidRequestsDoNotStartResources_data()
{
    QTest::addColumn<QUrl>("sourceUrl");
    QTest::addColumn<int>("maximumLongEdge");

    QTest::newRow("empty-source") << QUrl {} << 128;
    QTest::newRow("invalid-source") << QUrl(QStringLiteral("http://[")) << 128;
    QTest::newRow("zero-edge") << QUrl::fromLocalFile(QStringLiteral("/tmp/video.mp4")) << 0;
    QTest::newRow("negative-edge") << QUrl::fromLocalFile(QStringLiteral("/tmp/video.mp4")) << -1;
    QTest::newRow("oversized-edge")
        << QUrl::fromLocalFile(QStringLiteral("/tmp/video.mp4")) << 4097;
}

void VideoThumbnailExtractionContractTest::invalidRequestsDoNotStartResources()
{
    QFETCH(QUrl, sourceUrl);
    QFETCH(int, maximumLongEdge);

    QObject receiver;
    ExtractionHarness harness;
    std::optional<VideoThumbnailExtractionResult> result;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, { sourceUrl, maximumLongEdge },
        [&result](VideoThumbnailExtractionResult value) { result = std::move(value); },
        harness.dependencies());

    QVERIFY(job.isActive());
    QVERIFY(!result.has_value());
    QCOMPARE(harness.backend->creations, 0);
    QCOMPARE(harness.deadline->creations, 0);

    kiriview::test::drainQueuedCalls();

    QVERIFY(!job.isActive());
    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Failed);
    QVERIFY(result->image.isNull());
    QVERIFY(result->failure.has_value());
    QCOMPARE(result->failure->cause, VideoThumbnailExtractionFailureCause::InvalidRequest);
}

void VideoThumbnailExtractionContractTest::admittedRequestStartsBoundedExtraction()
{
    QObject receiver;
    ExtractionHarness harness;
    std::optional<VideoThumbnailExtractionResult> result;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(),
        [&result](VideoThumbnailExtractionResult value) { result = std::move(value); },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    QVERIFY(job.isActive());
    QCOMPARE(harness.backend->creations, 1);
    QCOMPARE(harness.deadline->creations, 1);
    QCOMPARE(harness.backend->setSourceCalls, 1);
    QCOMPARE(harness.deadline->startCalls, 1);
    QCOMPARE(harness.deadline->interval, std::chrono::seconds(10));

    harness.backend->instance->emitMediaFacts(
        { VideoThumbnailBackendMediaStatus::Ready, 900, true, true });
    QCOMPARE(harness.backend->positions, std::vector<qint64> { 300 });

    QImage frame(32, 18, QImage::Format_RGBA8888);
    frame.fill(Qt::red);
    harness.backend->instance->emitFrame(frame);
    QVERIFY(!result.has_value());

    kiriview::test::drainQueuedCalls();

    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Ready);
    QVERIFY(!result->image.isNull());
    QVERIFY(!result->failure.has_value());
}

void VideoThumbnailExtractionContractTest::coverPrecedesThumbnailAndOutputIsBounded()
{
    QObject receiver;
    ExtractionHarness harness;
    std::optional<VideoThumbnailExtractionResult> result;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(8),
        [&result](VideoThumbnailExtractionResult value) { result = std::move(value); },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    QImage cover(16, 8, QImage::Format_ARGB32_Premultiplied);
    cover.fill(Qt::red);
    QImage thumbnail(8, 8, QImage::Format_RGB888);
    thumbnail.fill(Qt::blue);
    harness.backend->instance->emitMetadata(VideoThumbnailEmbeddedImages { cover, thumbnail });
    kiriview::test::drainQueuedCalls();

    QVERIFY(!job.isActive());
    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Ready);
    QCOMPARE(result->image.size(), QSize(8, 4));
    QCOMPARE(result->image.format(), QImage::Format_RGBA8888);
    QCOMPARE(result->image.pixelColor(0, 0), QColor(Qt::red));
    QVERIFY(result->image.sizeInBytes()
        <= kiriview::VideoThumbnailExtractionLimits::maximumOutputBytes);
}

void VideoThumbnailExtractionContractTest::unusableCoverFallsBackToThumbnail()
{
    QObject receiver;
    ExtractionHarness harness;
    std::optional<VideoThumbnailExtractionResult> result;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(8),
        [&result](VideoThumbnailExtractionResult value) { result = std::move(value); },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    alignas(4) std::array<uchar, 4> storage {};
    QImage oversizedCover(storage.data(), 16'385, 1, 16'385 * 4, QImage::Format_RGBA8888);
    QImage thumbnail(8, 4, QImage::Format_RGBA8888);
    thumbnail.fill(Qt::blue);
    harness.backend->instance->emitMetadata(
        VideoThumbnailEmbeddedImages { oversizedCover, thumbnail });
    kiriview::test::drainQueuedCalls();

    QVERIFY(!job.isActive());
    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Ready);
    QCOMPARE(result->image.size(), QSize(8, 4));
    QCOMPARE(result->image.pixelColor(0, 0), QColor(Qt::blue));
}

void VideoThumbnailExtractionContractTest::frameOutputIsDetachedFromBackendStorage()
{
    QObject receiver;
    ExtractionHarness harness;
    std::optional<VideoThumbnailExtractionResult> result;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(4),
        [&result](VideoThumbnailExtractionResult value) { result = std::move(value); },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    harness.backend->instance->emitMediaFacts(
        { VideoThumbnailBackendMediaStatus::Ready, 0, false, true });
    QImage frame(4, 2, QImage::Format_RGBA8888);
    frame.fill(Qt::red);
    harness.backend->instance->emitFrame(frame);
    frame.fill(Qt::blue);
    kiriview::test::drainQueuedCalls();

    QVERIFY(!job.isActive());
    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Ready);
    QCOMPARE(result->image.size(), QSize(4, 2));
    QCOMPARE(result->image.pixelColor(0, 0), QColor(Qt::red));
}

void VideoThumbnailExtractionContractTest::nonSeekableMediaUsesFirstUsableFrame()
{
    QObject receiver;
    ExtractionHarness harness;
    std::optional<VideoThumbnailExtractionResult> result;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(64),
        [&result](VideoThumbnailExtractionResult value) { result = std::move(value); },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    harness.backend->instance->emitMediaFacts(
        { VideoThumbnailBackendMediaStatus::Ready, 0, false, true });
    QCOMPARE(harness.backend->playCalls, 1);
    QVERIFY(harness.backend->positions.empty());

    QImage frame(12, 20, QImage::Format_RGB32);
    frame.fill(Qt::green);
    harness.backend->instance->emitFrame(frame);
    kiriview::test::drainQueuedCalls();

    QVERIFY(!job.isActive());
    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Ready);
    QCOMPARE(result->image.size(), QSize(12, 20));
}

void VideoThumbnailExtractionContractTest::oversizedFrameDoesNotMaterialize()
{
    QObject receiver;
    ExtractionHarness harness;
    std::optional<VideoThumbnailExtractionResult> result;
    bool materialized = false;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(),
        [&result](VideoThumbnailExtractionResult value) { result = std::move(value); },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    harness.backend->instance->emitMediaFacts(
        { VideoThumbnailBackendMediaStatus::Ready, 0, false, true });
    harness.backend->instance->emitFrame(kiriview::detail::VideoThumbnailBackendFrame(
        QSize(std::numeric_limits<int>::max(), std::numeric_limits<int>::max()), [&materialized]() {
            materialized = true;
            return QImage(1, 1, QImage::Format_RGBA8888);
        }));
    kiriview::test::drainQueuedCalls();

    QVERIFY(!materialized);
    QVERIFY(!job.isActive());
    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Failed);
    QVERIFY(result->failure.has_value());
    QCOMPARE(result->failure->cause, VideoThumbnailExtractionFailureCause::ResourceLimit);
}

void VideoThumbnailExtractionContractTest::admissibleFrameMaterializesOnce()
{
    QObject receiver;
    ExtractionHarness harness;
    std::optional<VideoThumbnailExtractionResult> result;
    int materializationCount = 0;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(),
        [&result](VideoThumbnailExtractionResult value) { result = std::move(value); },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    harness.backend->instance->emitMediaFacts(
        { VideoThumbnailBackendMediaStatus::Ready, 0, false, true });
    harness.backend->instance->emitFrame(
        kiriview::detail::VideoThumbnailBackendFrame(QSize(32, 18), [&materializationCount]() {
            ++materializationCount;
            QImage image(32, 18, QImage::Format_RGBA8888);
            image.fill(Qt::cyan);
            return image;
        }));
    kiriview::test::drainQueuedCalls();

    QCOMPARE(materializationCount, 1);
    QVERIFY(!job.isActive());
    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Ready);
    QCOMPARE(result->image.size(), QSize(32, 18));
    QCOMPARE(result->image.pixelColor(0, 0), QColor(Qt::cyan));
}

void VideoThumbnailExtractionContractTest::frameConversionFailureIsTyped()
{
    QObject receiver;
    ExtractionHarness harness;
    std::optional<VideoThumbnailExtractionResult> result;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(),
        [&result](VideoThumbnailExtractionResult value) { result = std::move(value); },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    harness.backend->instance->emitMediaFacts(
        { VideoThumbnailBackendMediaStatus::Ready, 0, false, true });
    harness.backend->instance->emitFrame(
        kiriview::detail::VideoThumbnailBackendFrame(QSize(32, 18), []() { return QImage {}; }));
    kiriview::test::drainQueuedCalls();

    QVERIFY(!job.isActive());
    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Failed);
    QVERIFY(result->failure.has_value());
    QCOMPARE(result->failure->cause, VideoThumbnailExtractionFailureCause::BackendFailure);
}

void VideoThumbnailExtractionContractTest::resourceLimitIsTyped()
{
    QObject receiver;
    ExtractionHarness harness;
    std::optional<VideoThumbnailExtractionResult> result;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(),
        [&result](VideoThumbnailExtractionResult value) { result = std::move(value); },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    alignas(4) std::array<uchar, 4> storage {};
    QImage oversized(storage.data(), 16'385, 1, 16'385 * 4, QImage::Format_RGBA8888);
    harness.backend->instance->emitMetadata(VideoThumbnailEmbeddedImages { oversized, {} });
    kiriview::test::drainQueuedCalls();

    QVERIFY(!job.isActive());
    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Failed);
    QVERIFY(result->image.isNull());
    QVERIFY(result->failure.has_value());
    QCOMPARE(result->failure->cause, VideoThumbnailExtractionFailureCause::ResourceLimit);
}

void VideoThumbnailExtractionContractTest::backendFailuresAreTyped_data()
{
    QTest::addColumn<VideoThumbnailBackendError>("backendError");
    QTest::addColumn<VideoThumbnailExtractionFailureCause>("expectedCause");

    QTest::newRow("resource") << VideoThumbnailBackendError::Resource
                              << VideoThumbnailExtractionFailureCause::SourceUnavailable;
    QTest::newRow("network") << VideoThumbnailBackendError::Network
                             << VideoThumbnailExtractionFailureCause::SourceUnavailable;
    QTest::newRow("access-denied") << VideoThumbnailBackendError::AccessDenied
                                   << VideoThumbnailExtractionFailureCause::SourceUnavailable;
    QTest::newRow("format") << VideoThumbnailBackendError::Format
                            << VideoThumbnailExtractionFailureCause::UnsupportedMedia;
    QTest::newRow("other") << VideoThumbnailBackendError::Other
                           << VideoThumbnailExtractionFailureCause::BackendFailure;
}

void VideoThumbnailExtractionContractTest::backendFailuresAreTyped()
{
    QFETCH(VideoThumbnailBackendError, backendError);
    QFETCH(VideoThumbnailExtractionFailureCause, expectedCause);

    QObject receiver;
    ExtractionHarness harness;
    std::optional<VideoThumbnailExtractionResult> result;

    auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
        &receiver, kiriview::test::validRequest(),
        [&result](VideoThumbnailExtractionResult value) { result = std::move(value); },
        harness.dependencies());
    kiriview::test::drainQueuedCalls();

    const QString emoji = QString::fromUcs4(U"\U0001F642");
    const QString untrusted
        = QStringLiteral("https://user:secret@example.invalid/video\nbackend=0x1234 ")
        + emoji.repeated(2'000);
    harness.backend->instance->emitError(backendError, untrusted);
    kiriview::test::drainQueuedCalls();

    QVERIFY(!job.isActive());
    QVERIFY(result.has_value());
    QCOMPARE(result->status, VideoThumbnailExtractionStatus::Failed);
    QVERIFY(result->failure.has_value());
    QCOMPARE(result->failure->cause, expectedCause);
    QVERIFY(!result->failure->diagnostic.contains(QStringLiteral("secret")));
    QVERIFY(!result->failure->diagnostic.contains(QStringLiteral("0x1234")));
    QVERIFY(scalarCount(result->failure->diagnostic)
        <= kiriview::VideoThumbnailExtractionLimits::maximumDiagnosticCharacters);
}

void VideoThumbnailExtractionContractTest::deadlineAndExhaustionAreTyped()
{
    const auto run = [](bool timeout) {
        QObject receiver;
        ExtractionHarness harness;
        std::optional<VideoThumbnailExtractionResult> result;
        auto job = kiriview::detail::startVideoThumbnailExtractionWithDependencies(
            &receiver, kiriview::test::validRequest(),
            [&result](VideoThumbnailExtractionResult value) { result = std::move(value); },
            harness.dependencies());
        kiriview::test::drainQueuedCalls();

        if (timeout) {
            harness.deadline->fire();
        } else {
            harness.backend->instance->emitMediaFacts(
                { VideoThumbnailBackendMediaStatus::EndOfMedia, 0, false, true });
        }
        kiriview::test::drainQueuedCalls();
        Q_ASSERT(!job.isActive());
        Q_ASSERT(result.has_value());
        return result->failure->cause;
    };

    QCOMPARE(run(true), VideoThumbnailExtractionFailureCause::TimedOut);
    QCOMPARE(run(false), VideoThumbnailExtractionFailureCause::NoRepresentativeImage);
}

} // namespace

QTEST_GUILESS_MAIN(VideoThumbnailExtractionContractTest)

#include "tst_videothumbnailextraction_contract.moc"
