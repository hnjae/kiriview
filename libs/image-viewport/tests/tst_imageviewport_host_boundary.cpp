// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_p.h"
#include "imageviewportplaybackscheduler_p.h"
#include "imageviewportproviderhost_p.h"
#include "imageviewportrenderhost_p.h"

#include <QtCore/QObject>
#include <QtTest/QTest>

#include <functional>
#include <type_traits>
#include <utility>

template <typename Event, typename = void> struct HasFreeFormDiagnostic : std::false_type
{
};

template <typename Event>
struct HasFreeFormDiagnostic<Event, std::void_t<decltype(std::declval<Event>().diagnostic)>>
    : std::true_type
{
};

template <typename Request, typename = void> struct ExposesAdmittedDiagnostic : std::false_type
{
};

template <typename Request>
struct ExposesAdmittedDiagnostic<Request,
    std::void_t<decltype(std::declval<const Request>().diagnostic())>> : std::true_type
{
};

static_assert(!HasFreeFormDiagnostic<ViewportProviderEvent>::value);
static_assert(!HasFreeFormDiagnostic<ViewportProviderHostEvent>::value);
static_assert(!ExposesAdmittedDiagnostic<ViewportEngineProviderHostEventRequest>::value);

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
    void providerHostEventsCarryTypedFactsOnly();
    void trustedDiagnosticsUseUnicodeScalarLimit();
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

void ImageViewportHostBoundaryTest::providerHostEventsCarryTypedFactsOnly()
{
    using ImageViewportInternal::PublicDiagnosticText;

    QVERIFY((!std::is_constructible_v<PublicDiagnosticText, QString>));
    QVERIFY(!std::is_default_constructible_v<ViewportEngineProviderHostEventRequest>);
    QVERIFY((!std::is_constructible_v<ViewportEngineProviderHostEventRequest,
        ViewportProviderHostEvent>));

    ViewportProviderHostEvent event;
    event.kind = ViewportProviderHostEvent::Kind::ProviderEvent;
    event.providerEvent.kind = ImageSequenceProviderEventKind::Failed;

    const auto admitted = ViewportEngineProviderHostEventRequest::admit(std::move(event));
    QCOMPARE(admitted.event().providerEvent.kind, ImageSequenceProviderEventKind::Failed);
}

void ImageViewportHostBoundaryTest::trustedDiagnosticsUseUnicodeScalarLimit()
{
    using ImageViewportInternal::PublicDiagnosticText;

    const int limit = ImageSequenceLimits::maximumDiagnosticCharacters();
    const char32_t codePoint[] = { 0x1F642 };
    const QString scalar = QString::fromUcs4(codePoint, 1);
    const QString diagnostic = scalar.repeated(limit + 1);

    const QString text = PublicDiagnosticText::fromTrusted(diagnostic).text();
    QCOMPARE(text.toUcs4().size(), limit);
    QCOMPARE(text, scalar.repeated(limit));
}

QTEST_MAIN(ImageViewportHostBoundaryTest)

#include "tst_imageviewport_host_boundary.moc"
