// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <KProtocolInfo>
#include <KProtocolManager>

#include <QObject>
#include <QTest>
#include <QUrl>

class TestKioRemoteSourceCapabilities : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void smbImageSourceOperationsAreAvailable();
};

void TestKioRemoteSourceCapabilities::smbImageSourceOperationsAreAvailable()
{
    const QUrl imageUrl(QStringLiteral("smb://example.invalid/share/image.png"));
    const QUrl parentUrl(QStringLiteral("smb://example.invalid/share/"));

    QVERIFY(KProtocolInfo::isKnownProtocol(imageUrl));
    QVERIFY(KProtocolManager::supportsReading(imageUrl));
    QVERIFY(KProtocolManager::supportsListing(parentUrl));
}

QTEST_GUILESS_MAIN(TestKioRemoteSourceCapabilities)

#include "tst_kioremotesourcecapabilities.moc"
