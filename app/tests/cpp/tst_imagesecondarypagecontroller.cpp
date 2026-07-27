// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagesecondarypagecontroller.h"

#include <QTest>

#include <optional>
#include <vector>

namespace {
kiriview::OpenedCollectionScopeLocation openedCollectionScope()
{
    return kiriview::OpenedCollectionScopeLocation::fromUrls(
        QUrl(QStringLiteral("file:///books/book.cbz")), QUrl(QStringLiteral("file:///books/")),
        kiriview::OpenedCollectionScopeKind::ComicBookArchive);
}

struct ObservedCompletion
{
    kiriview::ImageSecondaryPageLoadResult result = kiriview::ImageSecondaryPageLoadResult::Failed;
    kiriview::DisplayedImageLocation location;
    QSize imageSize;
};
}

class TestImageSecondaryPageController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void providerTerminalClaimsExactSameUrlSessionBeforeCallback();
};

void TestImageSecondaryPageController::providerTerminalClaimsExactSameUrlSessionBeforeCallback()
{
    const QUrl pageUrl(QStringLiteral("zip:///books/book.cbz!/002.png"));
    const kiriview::OpenedCollectionScopeLocation scope = openedCollectionScope();
    std::vector<kiriview::ImageLoadSession> preparedSessions;
    std::vector<ObservedCompletion> completions;
    kiriview::ImageSecondaryPageController* controllerPointer = nullptr;
    bool replacementStarted = false;

    kiriview::ImageSecondaryPageController controller(
        kiriview::ImageSecondaryPageController::Callbacks {
            [&](kiriview::ImageSecondaryPageLoadResult result,
                const kiriview::DisplayedImageLocation& location, const QSize& imageSize) {
                completions.push_back({ result, location, imageSize });
                if (replacementStarted) {
                    return;
                }

                replacementStarted = true;
                controllerPointer->startLoad(pageUrl, scope);
                if (preparedSessions.size() == 2) {
                    controllerPointer->finishProviderLoad(
                        preparedSessions.front(), QSize(800, 1200));
                }
            },
            {},
            [&](kiriview::ImageLoadSession session, std::optional<kiriview::PredecodedImage>) {
                preparedSessions.push_back(std::move(session));
            },
        });
    controllerPointer = &controller;

    controller.startLoad(pageUrl, scope);
    QCOMPARE(preparedSessions.size(), std::size_t(1));
    const kiriview::ImageLoadSession firstSession = preparedSessions.front();

    controller.finishProviderLoadWithError(firstSession);
    QCOMPARE(preparedSessions.size(), std::size_t(2));
    const kiriview::ImageLoadSession secondSession = preparedSessions.back();
    QCOMPARE(firstSession.imageUrl(), secondSession.imageUrl());
    QVERIFY(!firstSession.sameSession(secondSession));

    const QSize secondImageSize(600, 900);
    controller.finishProviderLoad(secondSession, secondImageSize);
    controller.finishProviderLoadWithError(secondSession);

    QCOMPARE(completions.size(), std::size_t(2));
    QCOMPARE(completions.at(0).result, kiriview::ImageSecondaryPageLoadResult::Failed);
    QCOMPARE(completions.at(1).result, kiriview::ImageSecondaryPageLoadResult::Visible);
    QCOMPARE(completions.at(1).location, secondSession.location());
    QCOMPARE(completions.at(1).imageSize, secondImageSize);
    QVERIFY(controller.visible());
    QCOMPARE(controller.displayedImageLocation(), secondSession.location());
    QCOMPARE(controller.imageSize(), secondImageSize);
}

QTEST_GUILESS_MAIN(TestImageSecondaryPageController)

#include "tst_imagesecondarypagecontroller.moc"
