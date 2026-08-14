// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "rendering/heifdisplaysource.h"

#include "decoding/imagedecodeworkspace.h"

#include <QFile>
#include <QObject>
#include <QString>
#include <QTest>
#include <memory>

class TestHeifDisplaySource : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void staticOpenIncludesRetainedInputInOperationAdmission();
};

void TestHeifDisplaySource::staticOpenIncludesRetainedInputInOperationAdmission()
{
    QFile fixture(
        QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR "/../fixtures/heif-sequence-alpha.heics"));
    QVERIFY(fixture.open(QIODevice::ReadOnly));
    const QByteArray data = fixture.readAll();
    QVERIFY(!data.isEmpty());

    constexpr qsizetype generousByteCount = qsizetype { 256 } * 1024 * 1024;
    auto generousBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        generousByteCount, generousByteCount);
    QString errorString;
    bool resourceExhausted = false;
    const std::shared_ptr<kiriview::HeifDisplaySource> opened = kiriview::openHeifDisplaySource(
        data, &errorString, generousBudget, 0, &resourceExhausted);
    QVERIFY2(opened != nullptr, qPrintable(errorString));
    QVERIFY(!resourceExhausted);
    QCOMPARE(generousBudget->reservedByteCount(), qsizetype(0));

    constexpr qsizetype retainedInputByteCount = generousByteCount - 1;
    auto limitedBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        generousByteCount * 2, generousByteCount);
    kiriview::ImageDecodeWorkspaceLease retainedInput
        = kiriview::ImageDecodeWorkspaceDetail::startLease(*limitedBudget);
    QVERIFY(
        kiriview::ImageDecodeWorkspaceDetail::tryReserve(retainedInput, retainedInputByteCount));
    resourceExhausted = false;
    errorString.clear();

    const std::shared_ptr<kiriview::HeifDisplaySource> rejected = kiriview::openHeifDisplaySource(
        data, &errorString, limitedBudget, retainedInputByteCount, &resourceExhausted);

    QVERIFY(rejected == nullptr);
    QVERIFY(resourceExhausted);
    QVERIFY(!errorString.isEmpty());
    QCOMPARE(limitedBudget->reservedByteCount(), retainedInputByteCount);
    retainedInput = {};
    QCOMPARE(limitedBudget->reservedByteCount(), qsizetype(0));
}

QTEST_GUILESS_MAIN(TestHeifDisplaySource)

#include "tst_heifdisplaysource.moc"
