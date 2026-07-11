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
}

class TestActiveNavigationThumbnailRowStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void identityReplacementAdvancesGenerationAndReleasesEntry();
    void currentOnlyChangePreservesGenerationAndReadyResult();
    void readyReplacementReleasesPreviousEntryAndPublishesRoles();
    void staleSourceKeyCannotMutateCurrentRows();
};

void TestActiveNavigationThumbnailRowStore::identityReplacementAdvancesGenerationAndReleasesEntry()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore store(this, images);
    store.setRows({ row(1, QStringLiteral("/media/one.png"), true) });
    const kiriview::ThumbnailSourceRevisionKey firstKey = store.sourceKeyAt(0);
    QVERIFY(store.installReadyImage(
        firstKey, image(Qt::red), kiriview::ThumbnailImageRetentionPriority::Visible, false));
    QCOMPARE(images->size(), qsizetype(1));

    store.setRows({ row(1, QStringLiteral("/media/two.png"), true) });

    QCOMPARE(store.navigationGeneration(), quint64(2));
    QCOMPARE(images->size(), qsizetype(0));
    QCOMPARE(store.resultAt(0).status, kiriview::ActiveNavigationThumbnailResultStatus::NoResult);
    QVERIFY(!store.rowIndexForSourceKey(firstKey).has_value());
}

void TestActiveNavigationThumbnailRowStore::currentOnlyChangePreservesGenerationAndReadyResult()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore store(this, images);
    store.setRows({ row(1, QStringLiteral("/media/one.png"), true),
        row(2, QStringLiteral("/media/two.png")) });
    const kiriview::ThumbnailSourceRevisionKey firstKey = store.sourceKeyAt(0);
    QVERIFY(store.installReadyImage(
        firstKey, image(Qt::green), kiriview::ThumbnailImageRetentionPriority::Visible, false));
    const QUrl readySource = store.resultAt(0).imageSource;

    store.setRows({ row(1, QStringLiteral("/media/one.png")),
        row(2, QStringLiteral("/media/two.png"), true) });

    QCOMPARE(store.navigationGeneration(), quint64(1));
    QCOMPARE(store.resultAt(0).imageSource, readySource);
    QCOMPARE(images->size(), qsizetype(1));
    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("current")).toBool(), false);
    QCOMPARE(modelData(*store.model(), 1, QByteArrayLiteral("current")).toBool(), true);
}

void TestActiveNavigationThumbnailRowStore::readyReplacementReleasesPreviousEntryAndPublishesRoles()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore store(this, images);
    store.setRows({ row(1, QStringLiteral("/media/one.png"), true) });
    const kiriview::ThumbnailSourceRevisionKey key = store.sourceKeyAt(0);
    QVERIFY(store.installReadyImage(
        key, image(Qt::red), kiriview::ThumbnailImageRetentionPriority::Visible, false));
    const QString firstId = imageId(store.resultAt(0).imageSource);

    QVERIFY(store.installReadyImage(
        key, image(Qt::blue), kiriview::ThumbnailImageRetentionPriority::Nearby, false));

    QCOMPARE(images->size(), qsizetype(1));
    QVERIFY(images->image(firstId).isNull());
    QCOMPARE(
        images->image(imageId(store.resultAt(0).imageSource)).pixelColor(0, 0), QColor(Qt::blue));
    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailStatus")).toInt(),
        static_cast<int>(kiriview::ActiveNavigationThumbnailResultStatus::Ready));
    QCOMPARE(modelData(*store.model(), 0, QByteArrayLiteral("thumbnailImageSource")).toUrl(),
        store.resultAt(0).imageSource);
}

void TestActiveNavigationThumbnailRowStore::staleSourceKeyCannotMutateCurrentRows()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore store(this, images);
    store.setRows({ row(1, QStringLiteral("/media/one.png"), true) });
    kiriview::ThumbnailSourceRevisionKey staleKey = store.sourceKeyAt(0);
    ++staleKey.navigationGeneration;

    store.applyPending(staleKey);
    store.applyFailed(staleKey);
    QVERIFY(!store.installReadyImage(
        staleKey, image(Qt::red), kiriview::ThumbnailImageRetentionPriority::Visible, false));

    QCOMPARE(store.resultAt(0).status, kiriview::ActiveNavigationThumbnailResultStatus::NoResult);
    QCOMPARE(images->size(), qsizetype(0));
}

QTEST_GUILESS_MAIN(TestActiveNavigationThumbnailRowStore)

#include "test_activenavigationthumbnailrowstore.moc"
