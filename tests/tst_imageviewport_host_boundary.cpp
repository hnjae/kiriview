#include "imageviewport_p.h"
#include "imageviewportplaybackscheduler_p.h"
#include "imageviewportproviderhost_p.h"
#include "imageviewportrenderhost_p.h"

#include <QtCore/QObject>
#include <QtTest/QTest>

#include <functional>
#include <type_traits>

class ImageViewportHostBoundaryTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportHostBoundaryTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void hostsExposeNarrowFactBoundaries();
    void providerDiagnosticsRequireItemBoundaryAdmission();
};

void ImageViewportHostBoundaryTest::hostsExposeNarrowFactBoundaries()
{
    using ProviderEventSink = std::function<void(ViewportProviderHostEvent)>;
    using ProviderDiagnosticSink
        = std::function<void(ImageViewportInternal::ProviderTransportDiagnostic)>;
    using PlaybackElapsedSink = std::function<void(int)>;

    QVERIFY((std::is_constructible_v<ImageViewportProviderHost, QObject&, ProviderEventSink,
        ProviderDiagnosticSink>));
    QVERIFY((!std::is_constructible_v<ImageViewportProviderHost, ImageViewportPrivate&>));
    QVERIFY(
        (std::is_constructible_v<ImageViewportPlaybackScheduler, QObject&, PlaybackElapsedSink>));
    QVERIFY((!std::is_constructible_v<ImageViewportPlaybackScheduler, ImageViewportPrivate&>));
    QVERIFY(std::is_default_constructible_v<ImageViewportRenderHost>);
    QVERIFY((!std::is_constructible_v<ImageViewportRenderHost, ImageViewportPrivate&>));

    using RenderSynchronize = ImageViewportRenderHostResult (ImageViewportRenderHost::*)(
        QSGNode*, QQuickWindow*, const ViewportRenderAttempt&);
    QVERIFY((std::is_same_v<decltype(&ImageViewportRenderHost::synchronize), RenderSynchronize>));
    QVERIFY(
        (std::is_same_v<decltype(ImageViewportRenderHostResult::fact), ViewportRenderHostFact>));
}

void ImageViewportHostBoundaryTest::providerDiagnosticsRequireItemBoundaryAdmission()
{
    using ImageViewportInternal::PublicDiagnosticText;

    QVERIFY((!std::is_constructible_v<PublicDiagnosticText, QString>));
    QVERIFY(!std::is_default_constructible_v<ViewportEngineProviderHostEventRequest>);
    QVERIFY((!std::is_constructible_v<ViewportEngineProviderHostEventRequest,
        ViewportProviderHostEvent>));

    ViewportProviderHostEvent event;
    event.kind = ViewportProviderHostEvent::Kind::ProviderEvent;
    event.providerEvent.kind = ImageSequenceProviderEventKind::Failed;
    event.providerEvent.diagnostic
        = QStringLiteral("failed for https://user:secret@example.test/image.png token=abc123");

    const auto admitted = ViewportEngineProviderHostEventRequest::admit(std::move(event));
    QCOMPARE(admitted.event().providerEvent.diagnostic, QString());
    QVERIFY(!admitted.diagnostic().text().isEmpty());
    QVERIFY(!admitted.diagnostic().text().contains(QStringLiteral("https://")));
    QVERIFY(!admitted.diagnostic().text().contains(QStringLiteral("token=abc123")));
}

QTEST_MAIN(ImageViewportHostBoundaryTest)

#include "tst_imageviewport_host_boundary.moc"
