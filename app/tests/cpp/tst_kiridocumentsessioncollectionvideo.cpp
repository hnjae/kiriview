// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "kiridocumentsession_test_support.h"

#include <cstddef>
#include <functional>
#include <utility>

namespace {
class PendingMediaOpenWithProvider
{
public:
    kiriview::MediaOpenWithProvider provider()
    {
        return [this](QObject* receiver, kiriview::MediaOpenWithRequest request,
                   kiriview::MediaOpenWithCallback) {
            ++m_startCount;
            m_targetUrl = std::move(request.targetUrl);
            auto* token = new QObject(receiver);
            return kiriview::ImageIoJob(token, [this](QObject* object) {
                ++m_cancelCount;
                std::function<void()> cancelHook = std::exchange(m_cancelHook, {});
                if (cancelHook) {
                    cancelHook();
                }
                if (object != nullptr) {
                    object->deleteLater();
                }
            });
        };
    }

    void setCancelHook(std::function<void()> cancelHook) { m_cancelHook = std::move(cancelHook); }

    [[nodiscard]] std::size_t startCount() const { return m_startCount; }
    [[nodiscard]] std::size_t cancelCount() const { return m_cancelCount; }
    [[nodiscard]] const QUrl& targetUrl() const { return m_targetUrl; }

private:
    std::function<void()> m_cancelHook;
    QUrl m_targetUrl;
    std::size_t m_startCount = 0;
    std::size_t m_cancelCount = 0;
};
}

class TestKiriDocumentSessionCollectionVideo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void playableCollectionVideoMediaInformationUsesCollectionEntryWithoutMetadata();
    void playableDirectoryCollectionVideoUsesOpenedCollectionNavigation();
    void openedCollectionVideoHandoffsDoNotPublishIntermediateEmptySession();
    void unsupportedOpenedCollectionVideoProjectsImagePlaceholderNavigation();
    void playableCollectionVideoDeletionTargetsOpenedCollectionContainerAndClearsPlayback();
    void collectionVideoHandoffCannotOverwriteSourceSelectedDuringOpenWithCancellation();
};

#include "kiridocumentsession_collection_video_deletion.inc"
#include "kiridocumentsession_collection_video_information.inc"
#include "kiridocumentsession_collection_video_placeholder.inc"
#include "kiridocumentsession_collection_video_routing.inc"

void TestKiriDocumentSessionCollectionVideo::
    collectionVideoHandoffCannotOverwriteSourceSelectedDuringOpenWithCancellation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    PendingMediaOpenWithProvider openWithProvider;
    const QUrl directoryUrl = localUrl(directory.path());
    const std::optional<kiriview::OpenedCollectionScopeLocation> directoryCollection
        = kiriview::openedCollectionScopeLocationForResolvedExternalSource(
            kiriview::NavigationSourceResolver().resolveExternalSource(directoryUrl));
    QVERIFY(directoryCollection.has_value());
    const QUrl firstPage = kiriview::TestSupport::archivePageUrl(
        directoryCollection->rootUrl(), QStringLiteral("chapter/01.png"));
    const QUrl videoPage = kiriview::TestSupport::archivePageUrl(
        directoryCollection->rootUrl(), QStringLiteral("chapter/clip.mp4"));
    const QUrl latestSource = localUrl(QStringLiteral("/latest/selected.mp4"));
    directMediaNavigationProvider.setMedia(
        localUrl(QStringLiteral("/latest/")), { directMediaNavigationCandidate(latestSource) });
    kiriview::MediaEntrySourceFactory mediaEntrySourceFactory
        = mediaEntrySourceFactoryForCandidates(
            { kiriview::TestSupport::imageDocumentPageCandidate(firstPage),
                kiriview::TestSupport::videoCandidate(videoPage) });
    std::unique_ptr<KiriDocumentSession> session
        = createSessionWithProvider(directMediaNavigationProvider.provider(), nullptr, nullptr, {},
            kiriview::TestSupport::staticImageDataDecoder(), openWithProvider.provider(), {}, {},
            {}, std::move(mediaEntrySourceFactory));

    session->setSourceUrl(directoryUrl);
    QTRY_VERIFY2(session->imageDocument()->status() == KiriImageDocument::Status::Ready,
        qPrintable(session->imageDocument()->errorString()));
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Image);
    QCOMPARE(session->imageDocument()->displayedUrl(), firstPage);
    QVERIFY(session->displayedMediaOpenWithAvailable());

    bool latestSourceSelected = false;
    openWithProvider.setCancelHook([&]() {
        latestSourceSelected = true;
        session->setSourceUrl(latestSource);
    });
    session->openCurrentMediaWith();
    QCOMPARE(openWithProvider.startCount(), std::size_t(1));
    QCOMPARE(openWithProvider.targetUrl(), firstPage);

    session->openActiveNavigationAtNumber(2);

    QVERIFY(latestSourceSelected);
    QCOMPARE(openWithProvider.cancelCount(), std::size_t(1));
    QCOMPARE(session->sourceUrl(), latestSource);
    QCOMPARE(session->documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(session->videoDocument()->sourceUrl(), latestSource);
}

QTEST_MAIN(TestKiriDocumentSessionCollectionVideo)

#include "tst_kiridocumentsessioncollectionvideo.moc"
