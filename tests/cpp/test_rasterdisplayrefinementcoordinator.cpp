// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/rasterdisplayrefinementcoordinator.h"

#include "async/imageworkerscheduler.h"
#include "image_test_support.h"
#include "rendering/imagerendering.h"
#include "rendering/qimagereaderdisplaysource.h"

#include <QBuffer>
#include <QByteArray>
#include <QObject>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QTest>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

class TestRasterDisplayRefinementCoordinator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cacheHitAvoidsWorkerReschedule();
    void inFlightDuplicateIsShared();
    void cancelRejectsStaleCompletion();
};

namespace {
constexpr qsizetype testByteBudget = 1024 * 1024;

struct ManualImageWorkerSchedule
{
    kiriview::ImageWorkerOperation work;
    kiriview::ImageWorkerCompletion completion;
};

class ManualImageWorkerScheduler
{
public:
    kiriview::ImageWorkerScheduler scheduler()
    {
        return kiriview::ImageWorkerScheduler([this](QObject*, kiriview::ImageWorkerOperation work,
                                                  kiriview::ImageWorkerCompletion completion) {
            schedules.push_back(
                ManualImageWorkerSchedule { std::move(work), std::move(completion) });
        });
    }

    std::size_t scheduleCount() const { return schedules.size(); }

    void runAndFinish(std::size_t index)
    {
        schedules.at(index).work();
        schedules.at(index).completion();
    }

private:
    std::vector<ManualImageWorkerSchedule> schedules;
};

QByteArray encodedPng(const QImage& image)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return data;
}

kiriview::StaticDisplayImagePayload qtRasterPayload(
    const QString& sourceIdentity, const QString& displayScopeIdentity = QStringLiteral("scope"))
{
    const QSize originalSize(16, 12);
    const QSize rasterSize(4, 3);
    QString errorString;
    std::shared_ptr<kiriview::QImageReaderDisplaySource> source
        = kiriview::QImageReaderDisplaySource::open(
            encodedPng(kiriview::TestSupport::testImage(originalSize)), QByteArrayLiteral("png"),
            &errorString);
    Q_ASSERT(source != nullptr);

    kiriview::StaticDisplayImagePayload payload {
        sourceIdentity,
        source->imageReaderTransform(),
        originalSize,
        kiriview::TestSupport::testImage(rasterSize),
        kiriview::DisplayImageQuality::FirstDisplay,
        kiriview::imagePixelsPerSourcePixel(originalSize, rasterSize),
        {},
        std::move(source),
    };
    payload.displayScopeIdentity = displayScopeIdentity;
    return payload;
}

kiriview::ImagePresentationRenderProjection visibleProjection()
{
    kiriview::ImagePresentationRenderProjection projection;
    projection.visible = true;
    projection.pageRole = kiriview::DisplayedPageRole::Primary;
    projection.displaySize = QSizeF(8.0, 6.0);
    projection.visibleItemRect = QRectF(0.0, 0.0, 8.0, 6.0);
    projection.maximumTextureSize = kiriview::fallbackTextureSizeMax;
    return projection;
}
}

void TestRasterDisplayRefinementCoordinator::cacheHitAvoidsWorkerReschedule()
{
    ManualImageWorkerScheduler worker;
    std::vector<kiriview::StaticDisplayImagePayload> accepted;
    kiriview::RasterDisplayRefinementCoordinator coordinator(this, testByteBudget,
        worker.scheduler(),
        [&accepted](kiriview::StaticDisplayImagePayload displayImage,
            const kiriview::ImageDocumentRenderContext&) {
            accepted.push_back(std::move(displayImage));
        });

    coordinator.request(qtRasterPayload(QStringLiteral("source-a")), visibleProjection(), 1);
    QCOMPARE(worker.scheduleCount(), std::size_t(1));
    worker.runAndFinish(0);
    QCOMPARE(accepted.size(), std::size_t(1));
    QCOMPARE(accepted.back().image.size(), QSize(8, 6));

    coordinator.cancel();
    coordinator.request(qtRasterPayload(QStringLiteral("source-a")), visibleProjection(), 2);

    QCOMPARE(worker.scheduleCount(), std::size_t(1));
    QCOMPARE(accepted.size(), std::size_t(2));
    QCOMPARE(accepted.back().image.size(), QSize(8, 6));
}

void TestRasterDisplayRefinementCoordinator::inFlightDuplicateIsShared()
{
    ManualImageWorkerScheduler worker;
    std::vector<kiriview::StaticDisplayImagePayload> accepted;
    kiriview::RasterDisplayRefinementCoordinator coordinator(this, testByteBudget,
        worker.scheduler(),
        [&accepted](kiriview::StaticDisplayImagePayload displayImage,
            const kiriview::ImageDocumentRenderContext&) {
            accepted.push_back(std::move(displayImage));
        });

    coordinator.request(qtRasterPayload(QStringLiteral("source-a")), visibleProjection(), 1);
    coordinator.cancel();
    coordinator.request(qtRasterPayload(QStringLiteral("source-a")), visibleProjection(), 2);

    QCOMPARE(worker.scheduleCount(), std::size_t(1));
    worker.runAndFinish(0);
    QCOMPARE(accepted.size(), std::size_t(1));
    QCOMPARE(accepted.back().sourceIdentity, QStringLiteral("source-a"));
}

void TestRasterDisplayRefinementCoordinator::cancelRejectsStaleCompletion()
{
    ManualImageWorkerScheduler worker;
    std::vector<kiriview::StaticDisplayImagePayload> accepted;
    kiriview::RasterDisplayRefinementCoordinator coordinator(this, testByteBudget,
        worker.scheduler(),
        [&accepted](kiriview::StaticDisplayImagePayload displayImage,
            const kiriview::ImageDocumentRenderContext&) {
            accepted.push_back(std::move(displayImage));
        });

    coordinator.request(qtRasterPayload(QStringLiteral("source-a")), visibleProjection(), 1);
    QCOMPARE(worker.scheduleCount(), std::size_t(1));

    coordinator.cancel();
    worker.runAndFinish(0);

    QVERIFY(accepted.empty());
}

QTEST_GUILESS_MAIN(TestRasterDisplayRefinementCoordinator)

#include "test_rasterdisplayrefinementcoordinator.moc"
