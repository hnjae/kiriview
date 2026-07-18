// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "bridge/imageopenworkflowconversion.h"

#include <QObject>
#include <QTest>

class TestImageOpenWorkflowConversion : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sourceLoadPolicyInputMapsPlainFields();
    void loadFailureRouteMapsToBridgeEvent();
};

void TestImageOpenWorkflowConversion::sourceLoadPolicyInputMapsPlainFields()
{
    kiriview::Bridge::ImageDocumentSourceLoadPolicyInput input;
    input.loadKind = kiriview::Bridge::ImageDocumentSourceLoadKind::ReplacementSource;
    input.preserveTwoPageSpreadTransition = true;
    input.rightToLeftReadingEnabled = true;
    input.sourceWithinDisplayedComicBookArchive = true;
    input.hasRequestedContainerNavigationUrl = true;

    const kiriview::RustImageDocumentSourceLoadPolicyInput converted
        = kiriview::rustImageDocumentSourceLoadPolicyInput(input);

    QCOMPARE(converted.load_kind, kiriview::RustImageDocumentSourceLoadKind::ReplacementSource);
    QVERIFY(converted.preserve_two_page_spread_transition);
    QVERIFY(converted.right_to_left_reading_enabled);
    QVERIFY(converted.source_within_displayed_comic_book_archive);
    QVERIFY(converted.has_requested_container_navigation_url);
}

void TestImageOpenWorkflowConversion::loadFailureRouteMapsToBridgeEvent()
{
    const kiriview::RustImageOpenWorkflowEvent source = kiriview::rustSourceLoadErrorEvent(false);
    QCOMPARE(source.kind, kiriview::RustImageOpenWorkflowEventKind::FinishSourceLoadWithError);
    QCOMPARE(source.load_failure_route, kiriview::RustImageOpenLoadFailureRoute::Source);

    const kiriview::RustImageOpenWorkflowEvent container = kiriview::rustSourceLoadErrorEvent(true);
    QCOMPARE(container.kind, kiriview::RustImageOpenWorkflowEventKind::FinishSourceLoadWithError);
    QCOMPARE(
        container.load_failure_route, kiriview::RustImageOpenLoadFailureRoute::ContainerNavigation);
}

QTEST_GUILESS_MAIN(TestImageOpenWorkflowConversion)

#include "tst_imageopenworkflowconversion.moc"
