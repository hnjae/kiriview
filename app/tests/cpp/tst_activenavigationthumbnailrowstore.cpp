// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailrowstore.h"

#include <QAbstractItemModel>
#include <QColor>
#include <QImage>
#include <QObject>
#include <QTest>
#include <QUrl>
#include <memory>

namespace {
kiriview::ActiveNavigationThumbnailRow row(int number, const QString& path, bool current = false)
{
    return kiriview::ActiveNavigationThumbnailRow {
        number,
        QUrl::fromLocalFile(path),
        path.section(QLatin1Char('/'), -1),
        kiriview::ActiveNavigationThumbnailKind::Image,
        kiriview::ActiveNavigationThumbnailSourceKind::DirectImage,
        current,
    };
}

QImage image(QColor color)
{
    QImage result(QSize(2, 1), QImage::Format_RGBA8888);
    result.fill(color);
    return result;
}

int roleForName(const QAbstractItemModel& model, const QByteArray& name)
{
    const QHash<int, QByteArray> roles = model.roleNames();
    for (auto role = roles.cbegin(); role != roles.cend(); ++role) {
        if (role.value() == name) {
            return role.key();
        }
    }
    return -1;
}

QVariant modelData(const QAbstractItemModel& model, int row, const QByteArray& roleName)
{
    return model.data(model.index(row, 0), roleForName(model, roleName));
}

QString imageId(const QUrl& source) { return source.path().mid(1); }

kiriview::ActiveNavigationThumbnailRowCommit setRows(
    kiriview::ActiveNavigationThumbnailRowStore& store,
    std::vector<kiriview::ActiveNavigationThumbnailRow> rows)
{
    return store.commitRows(store.prepareRows(std::move(rows)));
}
}

class TestActiveNavigationThumbnailRowStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void identityReplacementAdvancesGenerationAndReleasesEntry();
    void currentOnlyChangePreservesGenerationAndReadyResult();
    void readyReplacementReleasesPreviousEntryAndPublishesRoles();
    void pressureEvictionInvalidatesReadyResult();
    void rebudgetingInvalidatesReadyResult();
    void terminalProjectionClearsQueuedResidencyLoss();
    void sourceRefreshMigratesQueuedResidencyLoss();
    void staleSourceKeyCannotMutateCurrentRows();
    void preparedIdentityReplacementDoesNotMutateBeforeCommit();
    void normalizedEquivalentUrlRefreshPreservesGenerationAndEntry();
};

void TestActiveNavigationThumbnailRowStore::identityReplacementAdvancesGenerationAndReleasesEntry()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore store(images);
    setRows(store, { row(1, QStringLiteral("/media/one.png"), true) });
    const kiriview::ThumbnailSourceRevisionKey firstKey = store.schedulingSnapshot().rows.front();
    QVERIFY(store.installReadyImage(
        firstKey, image(Qt::red), kiriview::ThumbnailImageRetentionPriority::Visible, false));
    QCOMPARE(images->size(), qsizetype(1));

    setRows(store, { row(1, QStringLiteral("/media/two.png"), true) });

    QCOMPARE(store.navigationGeneration(), quint64(2));
    QCOMPARE(images->size(), qsizetype(0));
    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(kiriview::ActiveNavigationThumbnailResultStatus::NoResult));
    store.applyPending(firstKey);
    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(kiriview::ActiveNavigationThumbnailResultStatus::NoResult));
}

void TestActiveNavigationThumbnailRowStore::currentOnlyChangePreservesGenerationAndReadyResult()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore store(images);
    setRows(store,
        { row(1, QStringLiteral("/media/one.png"), true),
            row(2, QStringLiteral("/media/two.png")) });
    const kiriview::ThumbnailSourceRevisionKey firstKey = store.schedulingSnapshot().rows.front();
    QVERIFY(store.installReadyImage(
        firstKey, image(Qt::green), kiriview::ThumbnailImageRetentionPriority::Visible, false));
    const QUrl readySource
        = modelData(*store.model(), 0, QByteArrayLiteral("thumbnailImageSource")).toUrl();

    setRows(store,
        { row(1, QStringLiteral("/media/one.png")),
            row(2, QStringLiteral("/media/two.png"), true) });

    QCOMPARE(store.navigationGeneration(), quint64(1));
    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailImageSource")).toUrl(),
        readySource);
    QCOMPARE(images->size(), qsizetype(1));
    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("current")).toBool(), false);
    QCOMPARE(modelData(*store.model(), 1, QByteArrayLiteral("current")).toBool(), true);
}

void TestActiveNavigationThumbnailRowStore::readyReplacementReleasesPreviousEntryAndPublishesRoles()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore store(images);
    setRows(store, { row(1, QStringLiteral("/media/one.png"), true) });
    const kiriview::ThumbnailSourceRevisionKey key = store.schedulingSnapshot().rows.front();
    QVERIFY(store.installReadyImage(
        key, image(Qt::red), kiriview::ThumbnailImageRetentionPriority::Visible, false));
    const QString firstId
        = imageId(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailImageSource")).toUrl());

    QVERIFY(store.installReadyImage(
        key, image(Qt::blue), kiriview::ThumbnailImageRetentionPriority::Nearby, false));

    QCOMPARE(images->size(), qsizetype(1));
    QVERIFY(images->image(firstId).isNull());
    const QUrl currentSource
        = modelData(*store.model(), 0, QByteArrayLiteral("thumbnailImageSource")).toUrl();
    QCOMPARE(images->image(imageId(currentSource)).pixelColor(0, 0), QColor(Qt::blue));
    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(kiriview::ActiveNavigationThumbnailResultStatus::Ready));
    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailImageSource")).toUrl(),
        currentSource);
}

void TestActiveNavigationThumbnailRowStore::pressureEvictionInvalidatesReadyResult()
{
    const QImage firstImage = image(Qt::red);
    auto images = std::make_shared<kiriview::ThumbnailImageStore>(firstImage.sizeInBytes());
    kiriview::ActiveNavigationThumbnailRowStore store(images);
    setRows(store,
        { row(1, QStringLiteral("/media/one.png"), true),
            row(2, QStringLiteral("/media/two.png")) });
    const auto sourceKeys = store.schedulingSnapshot().rows;
    QVERIFY(store.installReadyImage(
        sourceKeys.at(0), firstImage, kiriview::ThumbnailImageRetentionPriority::Visible, false));
    const QString firstId
        = imageId(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailImageSource")).toUrl());

    QVERIFY(store.installReadyImage(sourceKeys.at(1), image(Qt::blue),
        kiriview::ThumbnailImageRetentionPriority::Visible, false));

    QVERIFY(images->image(firstId).isNull());
    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(kiriview::ActiveNavigationThumbnailResultStatus::Pending));
    QVERIFY(
        modelData(*store.model(), 0, QByteArrayLiteral("thumbnailImageSource")).toUrl().isEmpty());
    const auto change = store.takeResidencyChange();
    QCOMPARE(change.losses.size(), std::size_t(1));
    QCOMPARE(change.losses.front(), sourceKeys.at(0));
    QVERIFY(!change.admissionOpportunity);
    QVERIFY(store.takeResidencyChange().empty());
}

void TestActiveNavigationThumbnailRowStore::rebudgetingInvalidatesReadyResult()
{
    const QImage readyImage = image(Qt::green);
    auto images = std::make_shared<kiriview::ThumbnailImageStore>(readyImage.sizeInBytes());
    kiriview::ActiveNavigationThumbnailRowStore store(images);
    setRows(store, { row(1, QStringLiteral("/media/one.png"), true) });
    const auto sourceKey = store.schedulingSnapshot().rows.front();
    QVERIFY(store.installReadyImage(
        sourceKey, readyImage, kiriview::ThumbnailImageRetentionPriority::Visible, false));

    images->setByteBudget(1);

    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(kiriview::ActiveNavigationThumbnailResultStatus::Pending));
    QVERIFY(
        modelData(*store.model(), 0, QByteArrayLiteral("thumbnailImageSource")).toUrl().isEmpty());
    const auto change = store.takeResidencyChange();
    QCOMPARE(change.losses.size(), std::size_t(1));
    QCOMPARE(change.losses.front(), sourceKey);
    QVERIFY(!change.admissionOpportunity);
}

void TestActiveNavigationThumbnailRowStore::terminalProjectionClearsQueuedResidencyLoss()
{
    const QImage readyImage = image(Qt::green);
    auto images = std::make_shared<kiriview::ThumbnailImageStore>(readyImage.sizeInBytes());
    kiriview::ActiveNavigationThumbnailRowStore store(images);
    setRows(store, { row(1, QStringLiteral("/media/one.png"), true) });
    const auto sourceKey = store.schedulingSnapshot().rows.front();
    QVERIFY(store.installReadyImage(
        sourceKey, readyImage, kiriview::ThumbnailImageRetentionPriority::Visible, false));
    images->setByteBudget(1);

    store.applyUnsupported(sourceKey);

    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(kiriview::ActiveNavigationThumbnailResultStatus::Unsupported));
    QVERIFY(store.takeResidencyChange().losses.empty());

    images->setByteBudget(readyImage.sizeInBytes());
    QVERIFY(store.installReadyImage(
        sourceKey, readyImage, kiriview::ThumbnailImageRetentionPriority::Visible, false));
    images->setByteBudget(1);
    store.applyFailed(sourceKey);

    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(kiriview::ActiveNavigationThumbnailResultStatus::Failed));
    QVERIFY(store.takeResidencyChange().losses.empty());
}

void TestActiveNavigationThumbnailRowStore::sourceRefreshMigratesQueuedResidencyLoss()
{
    const QImage readyImage = image(Qt::green);
    auto images = std::make_shared<kiriview::ThumbnailImageStore>(readyImage.sizeInBytes());
    kiriview::ActiveNavigationThumbnailRowStore store(images);
    setRows(store, { row(1, QStringLiteral("/media/chapter/../one.png"), true) });
    const auto originalKey = store.schedulingSnapshot().rows.front();
    QVERIFY(store.installReadyImage(
        originalKey, readyImage, kiriview::ThumbnailImageRetentionPriority::Visible, false));
    images->setByteBudget(1);

    const auto commit = setRows(store, { row(1, QStringLiteral("/media/one.png"), true) });

    QCOMPARE(commit.kind, kiriview::ActiveNavigationThumbnailRowUpdateKind::SourceRefresh);
    const auto change = store.takeResidencyChange();
    QCOMPARE(change.losses.size(), std::size_t(1));
    QCOMPARE(
        change.losses.front().sourceUrl, QUrl::fromLocalFile(QStringLiteral("/media/one.png")));
}

void TestActiveNavigationThumbnailRowStore::staleSourceKeyCannotMutateCurrentRows()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore store(images);
    setRows(store, { row(1, QStringLiteral("/media/one.png"), true) });
    kiriview::ThumbnailSourceRevisionKey staleKey = store.schedulingSnapshot().rows.front();
    ++staleKey.navigationGeneration;

    store.applyPending(staleKey);
    store.applyFailed(staleKey);
    QVERIFY(!store.installReadyImage(
        staleKey, image(Qt::red), kiriview::ThumbnailImageRetentionPriority::Visible, false));

    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(kiriview::ActiveNavigationThumbnailResultStatus::NoResult));
    QCOMPARE(images->size(), qsizetype(0));
}

void TestActiveNavigationThumbnailRowStore::preparedIdentityReplacementDoesNotMutateBeforeCommit()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore store(images);
    setRows(store, { row(1, QStringLiteral("/media/one.png"), true) });
    const quint64 generation = store.navigationGeneration();
    const int modelRows = store.model()->rowCount();

    auto plan = store.prepareRows({ row(1, QStringLiteral("/media/two.png"), true) });
    QCOMPARE(plan.kind(), kiriview::ActiveNavigationThumbnailRowUpdateKind::IdentityReplacement);
    QCOMPARE(store.navigationGeneration(), generation);
    QCOMPARE(store.model()->rowCount(), modelRows);

    const auto commit = store.commitRows(std::move(plan));
    QCOMPARE(commit.kind, kiriview::ActiveNavigationThumbnailRowUpdateKind::IdentityReplacement);
    QVERIFY(commit.schedulingSnapshot.has_value());
    QCOMPARE(commit.schedulingSnapshot->navigationGeneration, generation + 1);
    QCOMPARE(commit.schedulingSnapshot->rows.size(), std::size_t(1));
}

void TestActiveNavigationThumbnailRowStore::
    normalizedEquivalentUrlRefreshPreservesGenerationAndEntry()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore store(images);
    setRows(store, { row(1, QStringLiteral("/media/chapter/../one.png"), true) });
    const auto firstSnapshot = store.schedulingSnapshot();
    QVERIFY(store.installReadyImage(firstSnapshot.rows.front(), image(Qt::green),
        kiriview::ThumbnailImageRetentionPriority::Visible, false));
    const quint64 generation = store.navigationGeneration();

    auto plan = store.prepareRows({ row(1, QStringLiteral("/media/one.png"), true) });
    QCOMPARE(plan.kind(), kiriview::ActiveNavigationThumbnailRowUpdateKind::SourceRefresh);
    const auto commit = store.commitRows(std::move(plan));

    QCOMPARE(store.navigationGeneration(), generation);
    QCOMPARE(images->size(), qsizetype(1));
    QVERIFY(commit.schedulingSnapshot.has_value());
    QCOMPARE(commit.schedulingSnapshot->rows.front().sourceUrl,
        QUrl::fromLocalFile(QStringLiteral("/media/one.png")));
    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(kiriview::ActiveNavigationThumbnailResultStatus::Ready));
}

QTEST_GUILESS_MAIN(TestActiveNavigationThumbnailRowStore)

#include "tst_activenavigationthumbnailrowstore.moc"
