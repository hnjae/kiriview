// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/applicationstartupsource.h"

#include <QDir>
#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

namespace {
kiriview::ApplicationStartupParseResult parse(const QStringList& arguments)
{
    return kiriview::parseApplicationStartupSource(
        QStringList { QStringLiteral("kiriview") } + arguments);
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

    QVERIFY(result.has_value());
    QCOMPARE(result->kind, kiriview::ApplicationStartupSourceKind::None);
    QVERIFY(!result->verbose);
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

    QVERIFY(result.has_value());
    QVERIFY(result->verbose);
    QCOMPARE(result->kind, kiriview::ApplicationStartupSourceKind::LocalFilePath);
    QCOMPARE(result->text, directory.filePath(QStringLiteral("image.png")));
}

void TestApplicationStartupSource::rejectsUnknownOption()
{
    const auto result = parse({ QStringLiteral("--unknown") });

    QVERIFY(!result.has_value());
    QVERIFY(result.error().contains(QStringLiteral("--unknown")));
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

    QVERIFY(result.has_value());
    QVERIFY(!result->verbose);
    QCOMPARE(result->text, directory.filePath(QStringLiteral("-v")));
}

void TestApplicationStartupSource::rejectsMissingFileUrl()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("missing.png"));

    const auto result = parse({ QUrl::fromLocalFile(path).toString() });

    QVERIFY(!result.has_value());
    QVERIFY(result.error().contains(path));
}

QTEST_GUILESS_MAIN(TestApplicationStartupSource)

#include "tst_applicationstartupsource.moc"
