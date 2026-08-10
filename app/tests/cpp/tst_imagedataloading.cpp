// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imagedataloading.h"

#include "decoding/imagesourcedata.h"
#include "location/imagelocation.h"
#include "system/kiooperationfailure.h"

#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <memory>
#include <optional>
#include <utility>

class TestImageDataLoading : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void directSourceFailurePreservesTypedKioFields();
    void directSourceStopsAtSourceDataBudget();
};

void TestImageDataLoading::directSourceFailurePreservesTypedKioFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QUrl missingUrl = QUrl::fromLocalFile(directory.filePath(QStringLiteral("missing.png")));
    QObject receiver;
    bool deliveredData = false;
    std::optional<kiriview::ImageDataLoadError> loadError;
    kiriview::ImageIoJob job = kiriview::startStoredImageDataLoad(
        &receiver, kiriview::ImageDecodeRequest::fromUrl(1, missingUrl),
        [&deliveredData](kiriview::ImageSourceData) { deliveredData = true; },
        [&loadError](kiriview::ImageDataLoadError error) { loadError = std::move(error); });

    QTRY_VERIFY_WITH_TIMEOUT(!job.isActive(), 5000);
    QVERIFY(!deliveredData);
    QVERIFY(loadError.has_value());
    const auto* failure = std::get_if<kiriview::KioOperationFailure>(&*loadError);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->operationKind, kiriview::KioOperationKind::ImageDataRead);
    QCOMPARE(failure->targetUrl, missingUrl);
    QCOMPARE(failure->cause, kiriview::KioOperationFailureCause::Backend);
    QVERIFY(failure->rawErrorCode.has_value());
    QVERIFY(!failure->canceled);
    QVERIFY(!failure->diagnosticDetail.isEmpty());
    QVERIFY(!failure->retryable);
}

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
    std::optional<kiriview::ImageDataLoadError> loadError;
    const QUrl imageUrl = QUrl::fromLocalFile(path);
    kiriview::ImageIoJob job = kiriview::startStoredImageDataLoad(
        &receiver, kiriview::ImageDecodeRequest::fromUrl(1, imageUrl), {}, budget,
        [&deliveredData](kiriview::ImageSourceData) { deliveredData = true; },
        [&loadError](kiriview::ImageDataLoadError error) { loadError = std::move(error); });

    QTRY_VERIFY_WITH_TIMEOUT(!job.isActive(), 5000);
    QVERIFY(!deliveredData);
    QVERIFY(loadError.has_value());
    const auto* failure = std::get_if<kiriview::KioOperationFailure>(&*loadError);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->operationKind, kiriview::KioOperationKind::ImageDataRead);
    QCOMPARE(failure->targetUrl, imageUrl);
    QCOMPARE(failure->cause, kiriview::KioOperationFailureCause::ResourceLimitExceeded);
    QVERIFY(!failure->rawErrorCode.has_value());
    QVERIFY(!failure->canceled);
    QVERIFY(!failure->diagnosticDetail.isEmpty());
    QVERIFY(!failure->retryable);
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

QTEST_GUILESS_MAIN(TestImageDataLoading)

#include "tst_imagedataloading.moc"
