// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "navigation/imagedocumentpagecandidateitems.h"

#include <KFileItem>
#include <QObject>
#include <QTest>
#include <QUrl>
#include <sys/stat.h>
#include <vector>

class TestImageDocumentPageCandidateItems : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void imageDocumentPageCandidatesOnlyIncludeSupportedMediaFiles();
    void candidateAdmissionAcceptsExactEntryAndIdentityLimits();
    void candidateAdmissionRejectsEntryCountWithoutPublishingPartialRows();
    void candidateAdmissionRejectsIdentityCostWithoutPublishingPartialRows();
    void candidateAdmissionCountsUnsupportedEntries();
    void candidateAdmissionAcceptsNormalizedSiblingScope();
    void candidateAdmissionRejectsForeignScopeWithoutPublishingPartialRows_data();
    void candidateAdmissionRejectsForeignScopeWithoutPublishingPartialRows();
    void containerCandidatesOnlyIncludeComicBookArchives();
    void containerCandidateAdmissionEnforcesExactSharedLimits();
    void containerCandidateAdmissionCountsUnsupportedEntries();
    void containerCandidateAdmissionRejectsForeignScope();
};

void TestImageDocumentPageCandidateItems::
    imageDocumentPageCandidatesOnlyIncludeSupportedMediaFiles()
{
    KFileItemList items;
    items.append(KFileItem(QUrl::fromLocalFile(QStringLiteral("/images/a/")), QString(), S_IFDIR));
    items.append(
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/images/02.txt")), QString(), S_IFREG));
    items.append(
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/images/02.png")), QString(), S_IFREG));
    items.append(
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/images/03.mp4")), QString(), S_IFREG));
    items.append(
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/images/01.jpg")), QString(), S_IFREG));

    const kiriview::ImageDocumentPageCandidateAdmissionResult admitted
        = kiriview::imageDocumentPageNavigationCandidates(
            QUrl::fromLocalFile(QStringLiteral("/images/")), items);
    QVERIFY(admitted.has_value());
    const std::vector<kiriview::ImageDocumentPageCandidate>& candidates = *admitted;

    QCOMPARE(candidates.size(), std::size_t(3));
    QCOMPARE(candidates.front().url, QUrl::fromLocalFile(QStringLiteral("/images/01.jpg")));
    QCOMPARE(candidates.front().name, QStringLiteral("01.jpg"));
    QCOMPARE(candidates.front().kind, kiriview::ImageDocumentPageKind::Image);
    QCOMPARE(candidates.at(1).url, QUrl::fromLocalFile(QStringLiteral("/images/02.png")));
    QCOMPARE(candidates.at(1).name, QStringLiteral("02.png"));
    QCOMPARE(candidates.at(1).kind, kiriview::ImageDocumentPageKind::Image);
    QCOMPARE(candidates.back().url, QUrl::fromLocalFile(QStringLiteral("/images/03.mp4")));
    QCOMPARE(candidates.back().name, QStringLiteral("03.mp4"));
    QCOMPARE(candidates.back().kind, kiriview::ImageDocumentPageKind::Video);

    const kiriview::DirectMediaNavigationCandidateAdmissionResult directMediaAdmission
        = kiriview::directMediaNavigationCandidates(
            QUrl::fromLocalFile(QStringLiteral("/images/")), items);
    QVERIFY(directMediaAdmission.has_value());
    const std::vector<kiriview::DirectMediaNavigationCandidate>& directMediaNavigationCandidates
        = *directMediaAdmission;

    QCOMPARE(directMediaNavigationCandidates.size(), candidates.size());
    QCOMPARE(directMediaNavigationCandidates.front().url, candidates.front().url);
    QCOMPARE(directMediaNavigationCandidates.front().name, candidates.front().name);
    QCOMPARE(directMediaNavigationCandidates.at(1).url, candidates.at(1).url);
    QCOMPARE(directMediaNavigationCandidates.at(1).name, candidates.at(1).name);
    QCOMPARE(directMediaNavigationCandidates.back().url, candidates.back().url);
    QCOMPARE(directMediaNavigationCandidates.back().name, candidates.back().name);
}

void TestImageDocumentPageCandidateItems::candidateAdmissionAcceptsExactEntryAndIdentityLimits()
{
    const KFileItem item(QUrl::fromLocalFile(QStringLiteral("/images/01.png")), QString(), S_IFREG);
    KFileItemList items { item };
    const qsizetype identityCost
        = item.name().size() + item.url().toString(QUrl::FullyEncoded).size();

    const kiriview::ImageDocumentPageCandidateAdmissionResult admitted
        = kiriview::imageDocumentPageNavigationCandidates(
            QUrl::fromLocalFile(QStringLiteral("/images/")), items,
            kiriview::SiblingCandidateAdmissionLimits { 1, identityCost });

    QVERIFY(admitted.has_value());
    QCOMPARE(admitted->size(), std::size_t(1));
}

void TestImageDocumentPageCandidateItems::
    candidateAdmissionRejectsEntryCountWithoutPublishingPartialRows()
{
    KFileItemList items;
    items.append(
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/images/01.png")), QString(), S_IFREG));
    items.append(
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/images/02.png")), QString(), S_IFREG));

    const kiriview::ImageDocumentPageCandidateAdmissionResult admitted
        = kiriview::imageDocumentPageNavigationCandidates(
            QUrl::fromLocalFile(QStringLiteral("/images/")), items,
            kiriview::SiblingCandidateAdmissionLimits { 1, 1'024 });

    QVERIFY(!admitted.has_value());
    QCOMPARE(admitted.error(),
        kiriview::ImageDocumentPageCandidateAdmissionFailure::ResourceLimitExceeded);
}

void TestImageDocumentPageCandidateItems::
    candidateAdmissionRejectsIdentityCostWithoutPublishingPartialRows()
{
    const KFileItem item(QUrl::fromLocalFile(QStringLiteral("/images/01.png")), QString(), S_IFREG);
    KFileItemList items { item };
    const qsizetype identityCost
        = item.name().size() + item.url().toString(QUrl::FullyEncoded).size();

    const kiriview::ImageDocumentPageCandidateAdmissionResult admitted
        = kiriview::imageDocumentPageNavigationCandidates(
            QUrl::fromLocalFile(QStringLiteral("/images/")), items,
            kiriview::SiblingCandidateAdmissionLimits { 1, identityCost - 1 });

    QVERIFY(!admitted.has_value());
    QCOMPARE(admitted.error(),
        kiriview::ImageDocumentPageCandidateAdmissionFailure::ResourceLimitExceeded);
}

void TestImageDocumentPageCandidateItems::candidateAdmissionCountsUnsupportedEntries()
{
    KFileItemList items;
    items.append(
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/images/01.txt")), QString(), S_IFREG));
    items.append(
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/images/02.txt")), QString(), S_IFREG));

    const kiriview::ImageDocumentPageCandidateAdmissionResult admitted
        = kiriview::imageDocumentPageNavigationCandidates(
            QUrl::fromLocalFile(QStringLiteral("/images/")), items,
            kiriview::SiblingCandidateAdmissionLimits { 1, 1'024 });

    QVERIFY(!admitted.has_value());
    QCOMPARE(admitted.error(),
        kiriview::ImageDocumentPageCandidateAdmissionFailure::ResourceLimitExceeded);
}

void TestImageDocumentPageCandidateItems::candidateAdmissionAcceptsNormalizedSiblingScope()
{
    const QUrl requestedDirectory(
        QStringLiteral("smb://user@example.test/images/?listing=recent#selection"));
    KFileItemList items {
        KFileItem(QUrl(QStringLiteral(
                      "smb://user@example.test/images/chapter/../01.png?version=2#preview")),
            QString(), S_IFREG),
    };

    const kiriview::ImageDocumentPageCandidateAdmissionResult admitted
        = kiriview::imageDocumentPageNavigationCandidates(requestedDirectory, items);

    QVERIFY(admitted.has_value());
    QCOMPARE(admitted->size(), std::size_t(1));
}

void TestImageDocumentPageCandidateItems::
    candidateAdmissionRejectsForeignScopeWithoutPublishingPartialRows_data()
{
    QTest::addColumn<QUrl>("requestedDirectory");
    QTest::addColumn<QUrl>("foreignCandidate");

    QTest::newRow("foreign-parent") << QUrl::fromLocalFile(QStringLiteral("/images/"))
                                    << QUrl::fromLocalFile(QStringLiteral("/other/02.png"));
    QTest::newRow("changed-scheme") << QUrl(QStringLiteral("smb://example.test/images/"))
                                    << QUrl(QStringLiteral("https://example.test/images/02.png"));
    QTest::newRow("changed-authority") << QUrl(QStringLiteral("smb://example.test/images/"))
                                       << QUrl(QStringLiteral("smb://other.test/images/02.png"));
}

void TestImageDocumentPageCandidateItems::
    candidateAdmissionRejectsForeignScopeWithoutPublishingPartialRows()
{
    QFETCH(QUrl, requestedDirectory);
    QFETCH(QUrl, foreignCandidate);
    KFileItemList items {
        KFileItem(requestedDirectory.resolved(QUrl(QStringLiteral("01.png"))), QString(), S_IFREG),
        KFileItem(foreignCandidate, QString(), S_IFREG),
    };

    const kiriview::ImageDocumentPageCandidateAdmissionResult admitted
        = kiriview::imageDocumentPageNavigationCandidates(requestedDirectory, items);

    QVERIFY(!admitted.has_value());
    QCOMPARE(
        admitted.error(), kiriview::ImageDocumentPageCandidateAdmissionFailure::ScopeViolation);
}

void TestImageDocumentPageCandidateItems::containerCandidatesOnlyIncludeComicBookArchives()
{
    KFileItemList items;
    items.append(KFileItem(QUrl::fromLocalFile(QStringLiteral("/books/a/")), QString(), S_IFDIR));
    items.append(
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/books/a.cbz")), QString(), S_IFREG));
    items.append(
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/books/b.cbr")), QString(), S_IFREG));
    items.append(
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/books/book.zip")), QString(), S_IFREG));
    items.append(
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/books/book.rar")), QString(), S_IFREG));

    const kiriview::ContainerNavigationCandidateAdmissionResult admitted
        = kiriview::containerNavigationCandidates(
            QUrl::fromLocalFile(QStringLiteral("/books/")), items);
    QVERIFY(admitted.has_value());
    const std::vector<kiriview::ContainerNavigationCandidate>& candidates = *admitted;
    QCOMPARE(candidates.size(), std::size_t(2));
    QCOMPARE(candidates.front().url, QUrl::fromLocalFile(QStringLiteral("/books/a.cbz")));
    QCOMPARE(candidates.front().type, kiriview::ContainerNavigationCandidateType::ComicBookArchive);
    QCOMPARE(candidates.back().url, QUrl::fromLocalFile(QStringLiteral("/books/b.cbr")));
    QCOMPARE(candidates.back().type, kiriview::ContainerNavigationCandidateType::ComicBookArchive);
}

void TestImageDocumentPageCandidateItems::containerCandidateAdmissionEnforcesExactSharedLimits()
{
    const KFileItem supported(
        QUrl::fromLocalFile(QStringLiteral("/books/a.cbz")), QString(), S_IFREG);
    const KFileItem unsupported(
        QUrl::fromLocalFile(QStringLiteral("/books/notes.txt")), QString(), S_IFREG);
    const KFileItemList items { supported, unsupported };
    const qsizetype identityCost = supported.name().size()
        + supported.url().toString(QUrl::FullyEncoded).size() + unsupported.name().size()
        + unsupported.url().toString(QUrl::FullyEncoded).size();

    const kiriview::ContainerNavigationCandidateAdmissionResult admitted
        = kiriview::containerNavigationCandidates(QUrl::fromLocalFile(QStringLiteral("/books/")),
            items, kiriview::SiblingCandidateAdmissionLimits { items.size(), identityCost });

    QVERIFY(admitted.has_value());
    QCOMPARE(admitted->size(), std::size_t(1));
    QCOMPARE(admitted->front().url, supported.url());

    const kiriview::ContainerNavigationCandidateAdmissionResult overLimit
        = kiriview::containerNavigationCandidates(QUrl::fromLocalFile(QStringLiteral("/books/")),
            items, kiriview::SiblingCandidateAdmissionLimits { items.size(), identityCost - 1 });
    QVERIFY(!overLimit.has_value());
    QCOMPARE(overLimit.error(),
        kiriview::ImageDocumentPageCandidateAdmissionFailure::ResourceLimitExceeded);
}

void TestImageDocumentPageCandidateItems::containerCandidateAdmissionCountsUnsupportedEntries()
{
    const kiriview::SiblingCandidateAdmissionLimits limits
        = kiriview::defaultSiblingCandidateAdmissionLimits();
    KFileItemList items;
    items.reserve(limits.maximumEntryCount + 1);
    for (qsizetype index = 0; index <= limits.maximumEntryCount; ++index) {
        items.append(
            KFileItem(QUrl::fromLocalFile(QStringLiteral("/books/unsupported-%1.txt").arg(index)),
                QString(), S_IFREG));
    }

    const kiriview::ContainerNavigationCandidateAdmissionResult admitted
        = kiriview::containerNavigationCandidates(
            QUrl::fromLocalFile(QStringLiteral("/books/")), items);

    QVERIFY(!admitted.has_value());
    QCOMPARE(admitted.error(),
        kiriview::ImageDocumentPageCandidateAdmissionFailure::ResourceLimitExceeded);
}

void TestImageDocumentPageCandidateItems::containerCandidateAdmissionRejectsForeignScope()
{
    const QUrl requestedDirectory = QUrl::fromLocalFile(QStringLiteral("/books/"));
    KFileItemList items {
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/books/a.cbz")), QString(), S_IFREG),
        KFileItem(QUrl::fromLocalFile(QStringLiteral("/other/b.cbr")), QString(), S_IFREG),
    };

    const kiriview::ContainerNavigationCandidateAdmissionResult admitted
        = kiriview::containerNavigationCandidates(requestedDirectory, items);

    QVERIFY(!admitted.has_value());
    QCOMPARE(
        admitted.error(), kiriview::ImageDocumentPageCandidateAdmissionFailure::ScopeViolation);
}

QTEST_GUILESS_MAIN(TestImageDocumentPageCandidateItems)

#include "tst_imagedocumentpagecandidateitems.moc"
