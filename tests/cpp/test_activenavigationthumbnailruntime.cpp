// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailmodel.h"
#include "session/activenavigationthumbnailruntime.h"

#include <QAbstractItemModel>
#include <QColor>
#include <QImage>
#include <QModelIndex>
#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

namespace {
kiriview::ActiveNavigationThumbnailRow row(int number, const QString& path, bool current = false)
{
    return {
        number,
        QUrl::fromLocalFile(path),
        path.section(QLatin1Char('/'), -1),
        kiriview::ActiveNavigationThumbnailKind::Image,
        kiriview::ActiveNavigationThumbnailSourceKind::DirectImage,
        current,
    };
}
}

class TestActiveNavigationThumbnailRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesStableModelRolesAndCurrentProjection();
    void rowIdentityChangesGenerationButCurrentOnlyChangesDoNot();
    void defaultSourceAdapterAcceptsOnlySupportedLocalRows();
    void normalizedSourceRefreshUsesLatestRequestedUrl();
    void identityReplacementCancelsWorkBeforeReleasingImage();
};

void TestActiveNavigationThumbnailRuntime::publishesStableModelRolesAndCurrentProjection()
{
    kiriview::ActiveNavigationThumbnailRuntime runtime(
        this, kiriview::ActiveNavigationThumbnailRuntimeDependencies {});
    runtime.setRows({ row(1, QStringLiteral("/media/one.png"), true),
        row(2, QStringLiteral("/media/two.png")) });
    QAbstractItemModel* model = runtime.model();

    QCOMPARE(model->rowCount(), 2);
    const QModelIndex first = model->index(0, 0);
    QCOMPARE(model->data(first, kiriview::ActiveNavigationThumbnailModel::NumberRole).toInt(), 1);
    QCOMPARE(model->data(first, kiriview::ActiveNavigationThumbnailModel::LabelRole).toString(),
        QStringLiteral("one.png"));
    QVERIFY(model->data(first, kiriview::ActiveNavigationThumbnailModel::CurrentRole).toBool());

    runtime.setCurrentNumber(2);
    QVERIFY(!model->data(first, kiriview::ActiveNavigationThumbnailModel::CurrentRole).toBool());
    QVERIFY(model->data(model->index(1, 0), kiriview::ActiveNavigationThumbnailModel::CurrentRole)
            .toBool());
}

void TestActiveNavigationThumbnailRuntime::rowIdentityChangesGenerationButCurrentOnlyChangesDoNot()
{
    kiriview::ActiveNavigationThumbnailRuntime runtime(
        this, kiriview::ActiveNavigationThumbnailRuntimeDependencies {});
    runtime.setRows({ row(1, QStringLiteral("/media/one.png"), true) });
    const quint64 generation = runtime.navigationGeneration();

    runtime.setRows({ row(1, QStringLiteral("/media/one.png"), false) });
    QCOMPARE(runtime.navigationGeneration(), generation);

    runtime.setRows({ row(1, QStringLiteral("/media/renamed.png"), true) });
    QVERIFY(runtime.navigationGeneration() > generation);
}

void TestActiveNavigationThumbnailRuntime::defaultSourceAdapterAcceptsOnlySupportedLocalRows()
{
    const auto adapter = kiriview::defaultThumbnailSourceAdapter();
    auto localKey = kiriview::thumbnailSourceRevisionKey(1,
        QUrl::fromLocalFile(QStringLiteral("/media/one.png")), QStringLiteral("one.png"),
        QStringLiteral("image"),
        kiriview::activeNavigationThumbnailSourceKindIdentity(
            kiriview::ActiveNavigationThumbnailSourceKind::DirectImage),
        1);
    QCOMPARE(
        adapter({ localKey }).kind, kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile);

    localKey.sourceUrl = QUrl(QStringLiteral("https://example.invalid/one.png"));
    QCOMPARE(adapter({ localKey }).kind, kiriview::ThumbnailSourceAdapterPlanKind::Unsupported);
    localKey.sourceUrl = QUrl::fromLocalFile(QStringLiteral("/media/archive.zip"));
    localKey.row.sourceKind = QStringLiteral("collection");
    QCOMPARE(adapter({ localKey }).kind, kiriview::ThumbnailSourceAdapterPlanKind::Unsupported);
}

void TestActiveNavigationThumbnailRuntime::normalizedSourceRefreshUsesLatestRequestedUrl()
{
    QUrl adaptedUrl;
    kiriview::ActiveNavigationThumbnailRuntimeDependencies dependencies;
    dependencies.sourceAdapter = [&adaptedUrl](kiriview::ThumbnailSourceAdapterRequest request) {
        adaptedUrl = request.sourceKey.sourceUrl;
        return kiriview::ThumbnailSourceAdapterPlan {};
    };
    kiriview::ActiveNavigationThumbnailRuntime runtime(this, std::move(dependencies));
    runtime.setRows({ row(1, QStringLiteral("/media/chapter/../one.png"), true) });
    const quint64 generation = runtime.navigationGeneration();

    runtime.setRows({ row(1, QStringLiteral("/media/one.png"), true) });
    QCOMPARE(runtime.navigationGeneration(), generation);
    QVERIFY(runtime.replaceDemandSnapshot({ generation,
        { { 1, QUrl::fromLocalFile(QStringLiteral("/media/one.png")),
            kiriview::ActiveNavigationThumbnailDemandBucket::Normal,
            kiriview::ActiveNavigationThumbnailDemandPriority::Visible } } }));
    QCOMPARE(adaptedUrl, QUrl::fromLocalFile(QStringLiteral("/media/one.png")));
}

void TestActiveNavigationThumbnailRuntime::identityReplacementCancelsWorkBeforeReleasingImage()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    bool canceledBeforeRelease = false;
    kiriview::ActiveNavigationThumbnailRuntimeDependencies dependencies;
    dependencies.imageStore = images;
    dependencies.sourceAdapter = kiriview::defaultThumbnailSourceAdapter();
    dependencies.lookupProvider = [images, &canceledBeforeRelease](QObject*,
                                      kiriview::ThumbnailCacheLookupRequest request,
                                      kiriview::ThumbnailCacheLookupCallback callback) {
        if (request.requestedBucket == kiriview::ActiveNavigationThumbnailDemandBucket::Normal) {
            QImage ready(QSize(2, 1), QImage::Format_RGBA8888);
            ready.fill(Qt::green);
            callback({ kiriview::ThumbnailCacheLookupStatus::Ready, std::move(ready),
                request.requestedBucket, request.requestedBucket, {}, {} });
            return kiriview::ImageIoJob {};
        }
        return kiriview::ImageIoJob(new QObject, [images, &canceledBeforeRelease](QObject* object) {
            canceledBeforeRelease = images->size() == 1;
            delete object;
        });
    };
    kiriview::ActiveNavigationThumbnailRuntime runtime(this, std::move(dependencies));
    runtime.setRows({ row(1, QStringLiteral("/media/one.png"), true) });
    const quint64 generation = runtime.navigationGeneration();
    QVERIFY(runtime.replaceDemandSnapshot({ generation,
        { { 1, QUrl::fromLocalFile(QStringLiteral("/media/one.png")),
            kiriview::ActiveNavigationThumbnailDemandBucket::Normal,
            kiriview::ActiveNavigationThumbnailDemandPriority::Visible } } }));
    QCOMPARE(images->size(), qsizetype(1));

    runtime.setRows({ row(1, QStringLiteral("/media/two.png"), true) });
    QVERIFY(canceledBeforeRelease);
    QCOMPARE(images->size(), qsizetype(0));
}

QTEST_GUILESS_MAIN(TestActiveNavigationThumbnailRuntime)

#include "test_activenavigationthumbnailruntime.moc"
