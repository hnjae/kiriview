// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "archive/mediaentrysourceerror.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "system/kiooperationfailure.h"

#include <QDebug>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QUrl>

namespace {
QString renderProjection(const kiriview::DiagnosticLogProjection& projection)
{
    QString rendered;
    {
        QDebug debug(&rendered);
        debug << projection;
    }
    return rendered;
}

QString renderMediaEntrySourceError(const kiriview::MediaEntrySourceError& error)
{
    QString rendered;
    {
        QDebug debug(&rendered);
        debug << error;
    }
    return rendered;
}

QString renderKioOperationFailure(const kiriview::KioOperationFailure& failure)
{
    QString rendered;
    {
        QDebug debug(&rendered);
        debug << failure;
    }
    return rendered;
}

bool hasOnlySafeAscii(const QString& value)
{
    for (const QChar character : value) {
        if (character.unicode() < 0x20 || character.unicode() > 0x7e) {
            return false;
        }
    }
    return true;
}

bool isBoundedSafeAscii(const QString& value)
{
    return value.size() <= kiriview::maximumDiagnosticLogProjectionCharacters
        && hasOnlySafeAscii(value);
}
}

class TestDiagnosticLogProjection : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void projectionsAreOpaqueBoundedAndCorrelatable();
    void mediaEntrySourceErrorStreamExcludesRawPayloads();
    void kioOperationFailureStreamPreservesTypedSafeFields();
};

void TestDiagnosticLogProjection::projectionsAreOpaqueBoundedAndCorrelatable()
{
    const QUrl sourceUrl(QStringLiteral(
        "smb://alice:password@example.invalid/private/secret.png?token=sekret#fragment"));
    const QUrl otherSourceUrl(QStringLiteral("file:///different/private/image.png"));
    const QString sensitiveText
        = QStringLiteral("backend\npassword=/private/secret.png\ttoken=sekret");

    const QString source = renderProjection(kiriview::diagnosticSourceReference(sourceUrl));
    const QString repeatedSource = renderProjection(kiriview::diagnosticSourceReference(sourceUrl));
    const QString otherSource
        = renderProjection(kiriview::diagnosticSourceReference(otherSourceUrl));
    const QString path = renderProjection(kiriview::diagnosticPathReference(sensitiveText));
    const QString detail = renderProjection(kiriview::diagnosticDetailReference(sensitiveText));

    QCOMPARE(source, repeatedSource);
    QVERIFY(source != otherSource);
    QVERIFY(path != detail);
    QVERIFY(isBoundedSafeAscii(source));
    QVERIFY(isBoundedSafeAscii(otherSource));
    QVERIFY(isBoundedSafeAscii(path));
    QVERIFY(isBoundedSafeAscii(detail));
    const QStringList rawFragments {
        QStringLiteral("alice"),
        QStringLiteral("password"),
        QStringLiteral("example.invalid"),
        QStringLiteral("private"),
        QStringLiteral("secret.png"),
        QStringLiteral("token"),
        QStringLiteral("sekret"),
    };
    for (const QString& fragment : rawFragments) {
        QVERIFY(!source.contains(fragment));
        QVERIFY(!path.contains(fragment));
        QVERIFY(!detail.contains(fragment));
    }
}

void TestDiagnosticLogProjection::mediaEntrySourceErrorStreamExcludesRawPayloads()
{
    const QString longDiagnostic = QStringLiteral("backend-password-secret\n").repeated(8'192);
    const kiriview::MediaEntrySourceError error {
        kiriview::MediaEntrySourceErrorCause::ResourceLimitExceeded,
        kiriview::MediaEntrySourceBackendKind::KArchive,
        kiriview::MediaEntrySourceOperation::ReadImageData,
        QUrl(QStringLiteral(
            "smb://alice:password@example.invalid/private/archive.zip?token=sekret")),
        QStringLiteral("private/entry-secret.png"),
        longDiagnostic,
    };

    const QString rendered = renderMediaEntrySourceError(error);

    kiriview::MediaEntrySourceError differentCause = error;
    differentCause.cause = kiriview::MediaEntrySourceErrorCause::EntryReadFailed;
    kiriview::MediaEntrySourceError differentBackend = error;
    differentBackend.backend = kiriview::MediaEntrySourceBackendKind::Directory;
    kiriview::MediaEntrySourceError differentOperation = error;
    differentOperation.operation = kiriview::MediaEntrySourceOperation::ListEntries;

    QVERIFY(!rendered.isEmpty());
    QVERIFY(rendered.size() <= kiriview::maximumDiagnosticLogProjectionCharacters * 3 + 128);
    QVERIFY(hasOnlySafeAscii(rendered));
    QVERIFY(rendered != renderMediaEntrySourceError(differentCause));
    QVERIFY(rendered != renderMediaEntrySourceError(differentBackend));
    QVERIFY(rendered != renderMediaEntrySourceError(differentOperation));
    QVERIFY(!rendered.contains(QStringLiteral("alice")));
    QVERIFY(!rendered.contains(QStringLiteral("password")));
    QVERIFY(!rendered.contains(QStringLiteral("example.invalid")));
    QVERIFY(!rendered.contains(QStringLiteral("private/entry-secret.png")));
    QVERIFY(!rendered.contains(QStringLiteral("backend-password-secret")));
    QVERIFY(!rendered.contains(QLatin1Char('\n')));
}

void TestDiagnosticLogProjection::kioOperationFailureStreamPreservesTypedSafeFields()
{
    const QUrl targetUrl(QStringLiteral(
        "smb://alice:password@example.invalid/private/secret.png?token=sekret#fragment"));
    const QString backendDetail
        = QStringLiteral("backend password=/private/secret.png\ntoken=sekret");
    const kiriview::KioOperationFailure failure {
        kiriview::KioOperationKind::ImageDataRead,
        targetUrl,
        73,
        false,
        backendDetail,
        backendDetail,
        true,
        kiriview::KioOperationFailureCause::Backend,
    };

    const QString rendered = renderKioOperationFailure(failure);
    kiriview::KioOperationFailure differentCause = failure;
    differentCause.cause = kiriview::KioOperationFailureCause::ResourceLimitExceeded;
    kiriview::KioOperationFailure differentOperation = failure;
    differentOperation.operationKind = kiriview::KioOperationKind::DirectoryListing;
    kiriview::KioOperationFailure differentCode = failure;
    differentCode.rawErrorCode = 74;
    kiriview::KioOperationFailure canceled = failure;
    canceled.canceled = true;
    kiriview::KioOperationFailure permanent = failure;
    permanent.retryable = false;

    QVERIFY(!rendered.isEmpty());
    QVERIFY(rendered.size() <= kiriview::maximumDiagnosticLogProjectionCharacters * 2 + 256);
    QVERIFY(hasOnlySafeAscii(rendered));
    QVERIFY(rendered != renderKioOperationFailure(differentCause));
    QVERIFY(rendered != renderKioOperationFailure(differentOperation));
    QVERIFY(rendered != renderKioOperationFailure(differentCode));
    QVERIFY(rendered != renderKioOperationFailure(canceled));
    QVERIFY(rendered != renderKioOperationFailure(permanent));
    QVERIFY(!rendered.contains(QStringLiteral("alice")));
    QVERIFY(!rendered.contains(QStringLiteral("password")));
    QVERIFY(!rendered.contains(QStringLiteral("example.invalid")));
    QVERIFY(!rendered.contains(QStringLiteral("private/secret.png")));
    QVERIFY(!rendered.contains(QStringLiteral("token")));
    QVERIFY(!rendered.contains(QStringLiteral("sekret")));
    QVERIFY(!rendered.contains(QLatin1Char('\n')));
}

QTEST_GUILESS_MAIN(TestDiagnosticLogProjection)

#include "tst_diagnosticlogprojection.moc"
