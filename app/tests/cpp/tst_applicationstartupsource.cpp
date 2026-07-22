// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/applicationstartupsource.h"

#include <QDir>
#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QVector>

namespace {
kiriview::ApplicationStartupParseResult parse(const QStringList& arguments)
{
    QVector<QByteArray> encoded;
    encoded.reserve(arguments.size() + 1);
    encoded.push_back(QByteArrayLiteral("kiriview"));
    for (const QString& argument : arguments) {
        encoded.push_back(argument.toLocal8Bit());
    }

    QVector<char*> pointers;
    pointers.reserve(encoded.size());
    for (QByteArray& argument : encoded) {
        pointers.push_back(argument.data());
    }
    return kiriview::parseApplicationStartupSource(pointers.size(), pointers.data());
}

class CurrentDirectoryGuard
{
public:
    CurrentDirectoryGuard()
        : m_path(QDir::currentPath())
    {
    }

    ~CurrentDirectoryGuard() { QDir::setCurrent(m_path); }

private:
    QString m_path;
};
}

class TestApplicationStartupSource : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void acceptsDefaultOptions();
    void acceptsVerboseAndFirstRelativeSource();
    void rejectsUnknownOption();
    void treatsOptionAfterSeparatorAsSource();
    void rejectsMissingFileUrl();
};

void TestApplicationStartupSource::acceptsDefaultOptions()
{
    const auto result = parse({});

    QVERIFY(result.accepted());
    QCOMPARE(result.source.kind, kiriview::ApplicationStartupSourceKind::None);
    QVERIFY(!result.source.verbose);
}

void TestApplicationStartupSource::acceptsVerboseAndFirstRelativeSource()
{
    CurrentDirectoryGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir::setCurrent(directory.path()));
    QFile image(QStringLiteral("image.png"));
    QVERIFY(image.open(QIODevice::WriteOnly));
    image.close();

    const auto result = parse(
        { QStringLiteral("image.png"), QStringLiteral("-v"), QStringLiteral("ignored.png") });

    QVERIFY(result.accepted());
    QVERIFY(result.source.verbose);
    QCOMPARE(result.source.kind, kiriview::ApplicationStartupSourceKind::LocalFilePath);
    QCOMPARE(result.source.text, directory.filePath(QStringLiteral("image.png")));
}

void TestApplicationStartupSource::rejectsUnknownOption()
{
    const auto result = parse({ QStringLiteral("--unknown") });

    QVERIFY(!result.accepted());
    QVERIFY(result.errorString.contains(QStringLiteral("--unknown")));
}

void TestApplicationStartupSource::treatsOptionAfterSeparatorAsSource()
{
    CurrentDirectoryGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir::setCurrent(directory.path()));
    QFile source(QStringLiteral("-v"));
    QVERIFY(source.open(QIODevice::WriteOnly));
    source.close();

    const auto result = parse({ QStringLiteral("--"), QStringLiteral("-v") });

    QVERIFY(result.accepted());
    QVERIFY(!result.source.verbose);
    QCOMPARE(result.source.text, directory.filePath(QStringLiteral("-v")));
}

void TestApplicationStartupSource::rejectsMissingFileUrl()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("missing.png"));

    const auto result = parse({ QUrl::fromLocalFile(path).toString() });

    QVERIFY(!result.accepted());
    QVERIFY(result.errorString.contains(path));
}

QTEST_GUILESS_MAIN(TestApplicationStartupSource)

#include "tst_applicationstartupsource.moc"
