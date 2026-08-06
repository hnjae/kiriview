// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imagedataloading.h"

#include "decoding/imagesourcedata.h"
#include "location/imagelocation.h"

#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <memory>

class TestImageDataLoading : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void directSourceStopsAtSourceDataBudget();
};

void TestImageDataLoading::directSourceStopsAtSourceDataBudget()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("oversized.png"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(QByteArray(32, 'x')), qint64(32));
    file.close();

    auto budget = std::make_shared<kiriview::ImageSourceDataBudget>(16, 16);
    QObject receiver;
    bool deliveredData = false;
    QString errorString;
    kiriview::ImageIoJob job = kiriview::startStoredImageDataLoad(
        &receiver, kiriview::ImageDecodeRequest::fromUrl(1, QUrl::fromLocalFile(path)), {}, budget,
        [&deliveredData](kiriview::ImageSourceData) { deliveredData = true; },
        [&errorString](const kiriview::ImageDataLoadError& error) {
            if (const QString* message = std::get_if<QString>(&error)) {
                errorString = *message;
            }
        });

    QTRY_VERIFY_WITH_TIMEOUT(!job.isActive(), 5000);
    QVERIFY(!deliveredData);
    QVERIFY(!errorString.isEmpty());
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

QTEST_GUILESS_MAIN(TestImageDataLoading)

#include "tst_imagedataloading.moc"
