// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videooutputruntime.h"
#include "video/videozoomstate.h"

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRectF>
#include <QTest>
#include <memory>

class TestVideoOutputRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void outputAttachDetachAndSameOutputAreCanonical();
    void outputDestructionClearsCanonicalOutputAndBackend();
    void contextDestructionDoesNotOwnTheRuntimeObserver();
    void geometryAndRenderContextProjectZoom();
    void backendAttachmentReentryKeepsNewestOutput();
    void renderContextPublicationReentryKeepsNewestOutput();
    void backendAttachmentCanDestroyRuntime();
    void zoomPublicationCanDestroyRuntime();
    void outputPublicationCanDestroyRuntime();
    void renderContextEventCanDestroyRuntime();
    void sameOutputGeometryReentryPreservesPendingOutputPublication();
    void outputPublicationKeepsGetterCoherentDuringReentry();
    void destructorDetachRejectsReentrantAttachment();
};

namespace {
struct RuntimeFixture
{
    QObject context;
    QPointer<QObject> backendOutput;
    int backendOutputSetCount = 0;
    int outputChangedCount = 0;
    int zoomProjectionChangedCount = 0;
    kiriview::VideoOutputRuntime runtime;

    RuntimeFixture()
        : runtime(&context,
              kiriview::VideoOutputRuntimeCallbacks {
                  [this](QObject* output) {
                      backendOutput = output;
                      ++backendOutputSetCount;
                  },
                  [this](bool outputChanged) {
                      if (outputChanged) {
                          ++outputChangedCount;
                      }
                      ++zoomProjectionChangedCount;
                  },
              })
    {
    }
};
}

void TestVideoOutputRuntime::outputAttachDetachAndSameOutputAreCanonical()
{
    RuntimeFixture fixture;
    QObject output;

    fixture.runtime.setVideoOutputAttachment(&output, {}, {});

    QCOMPARE(fixture.runtime.videoOutput(), &output);
    QCOMPARE(fixture.backendOutput.data(), &output);
    QCOMPARE(fixture.backendOutputSetCount, 1);
    QCOMPARE(fixture.outputChangedCount, 1);

    fixture.runtime.setVideoOutputAttachment(&output, {}, {});

    QCOMPARE(fixture.runtime.videoOutput(), &output);
    QCOMPARE(fixture.backendOutput.data(), &output);
    QCOMPARE(fixture.backendOutputSetCount, 1);
    QCOMPARE(fixture.outputChangedCount, 1);

    fixture.runtime.setVideoOutputAttachment(nullptr, {}, {});

    QCOMPARE(fixture.runtime.videoOutput(), nullptr);
    QCOMPARE(fixture.backendOutput.data(), nullptr);
    QCOMPARE(fixture.backendOutputSetCount, 2);
    QCOMPARE(fixture.outputChangedCount, 2);
}

void TestVideoOutputRuntime::outputDestructionClearsCanonicalOutputAndBackend()
{
    RuntimeFixture fixture;
    auto* output = new QObject();

    fixture.runtime.setVideoOutputAttachment(output, {}, {});
    delete output;

    QCOMPARE(fixture.runtime.videoOutput(), nullptr);
    QCOMPARE(fixture.backendOutput.data(), nullptr);
    QCOMPARE(fixture.outputChangedCount, 2);
}

void TestVideoOutputRuntime::contextDestructionDoesNotOwnTheRuntimeObserver()
{
    auto context = std::make_unique<QObject>();
    auto runtime = std::make_unique<kiriview::VideoOutputRuntime>(
        context.get(), kiriview::VideoOutputRuntimeCallbacks {});

    context.reset();
    runtime.reset();
}

void TestVideoOutputRuntime::geometryAndRenderContextProjectZoom()
{
    RuntimeFixture fixture;
    QQuickWindow window;
    QQuickItem output;
    output.setParentItem(window.contentItem());

    fixture.runtime.setVideoOutputAttachment(&output, {}, {});
    const int changedAfterOutput = fixture.zoomProjectionChangedCount;

    fixture.runtime.setVideoOutputAttachment(
        &output, QRectF(0.0, 0.0, 1280.0, 720.0), QRectF(0.0, 0.0, 1280.0, 720.0));

    QVERIFY(fixture.runtime.zoomPercent().has_value());
    QCOMPARE(fixture.runtime.zoomPercent().value(), 100);
    QVERIFY(fixture.zoomProjectionChangedCount > changedAfterOutput);

    const int changedAfterGeometry = fixture.zoomProjectionChangedCount;
    fixture.runtime.setVideoOutputAttachment(
        &output, QRectF(0.0, 0.0, 1280.0, 720.0), QRectF(0.0, 0.0, 1280.0, 720.0));

    QCOMPARE(fixture.zoomProjectionChangedCount, changedAfterGeometry);
}

void TestVideoOutputRuntime::backendAttachmentReentryKeepsNewestOutput()
{
    QObject context;
    auto firstOutput = std::make_unique<QObject>();
    QObject replacementOutput;
    QPointer<QObject> backendOutput;
    QPointer<QObject> lastPublishedOutput;
    std::unique_ptr<kiriview::VideoOutputRuntime> runtime;
    bool reentered = false;
    int outputPublicationCount = 0;

    runtime = std::make_unique<kiriview::VideoOutputRuntime>(&context,
        kiriview::VideoOutputRuntimeCallbacks {
            [&](QObject* output) {
                backendOutput = output;
                if (output != firstOutput.get() || reentered) {
                    return;
                }
                reentered = true;
                runtime->setVideoOutputAttachment(&replacementOutput, {}, {});
            },
            [&](bool outputChanged) {
                if (outputChanged) {
                    lastPublishedOutput = runtime->videoOutput();
                    ++outputPublicationCount;
                }
            },
        });

    runtime->setVideoOutputAttachment(firstOutput.get(), {}, {});
    QVERIFY(reentered);

    firstOutput.reset();

    QCOMPARE(runtime->videoOutput(), &replacementOutput);
    QCOMPARE(backendOutput.data(), &replacementOutput);
    QVERIFY(outputPublicationCount > 0);
    QCOMPARE(lastPublishedOutput.data(), &replacementOutput);
}

void TestVideoOutputRuntime::renderContextPublicationReentryKeepsNewestOutput()
{
    QObject context;
    QQuickWindow window;
    auto firstOutput = std::make_unique<QQuickItem>();
    QQuickItem replacementOutput;
    firstOutput->setParentItem(window.contentItem());
    replacementOutput.setParentItem(window.contentItem());
    QPointer<QObject> backendOutput;
    QPointer<QObject> lastPublishedOutput;
    std::unique_ptr<kiriview::VideoOutputRuntime> runtime;
    bool reentered = false;
    int outputPublicationCount = 0;

    runtime = std::make_unique<kiriview::VideoOutputRuntime>(&context,
        kiriview::VideoOutputRuntimeCallbacks {
            [&](QObject* output) { backendOutput = output; },
            [&](bool outputChanged) {
                if (outputChanged) {
                    lastPublishedOutput = runtime->videoOutput();
                    ++outputPublicationCount;
                }
                if (backendOutput != firstOutput.get() || reentered) {
                    return;
                }
                reentered = true;
                runtime->setVideoOutputAttachment(&replacementOutput, {}, {});
            },
        });

    runtime->setVideoOutputAttachment(firstOutput.get(), {}, {});
    QVERIFY(reentered);

    firstOutput.reset();

    QCOMPARE(runtime->videoOutput(), &replacementOutput);
    QCOMPARE(backendOutput.data(), &replacementOutput);
    QVERIFY(outputPublicationCount > 0);
    QCOMPARE(lastPublishedOutput.data(), &replacementOutput);
}

void TestVideoOutputRuntime::backendAttachmentCanDestroyRuntime()
{
    QObject context;
    QObject output;
    std::unique_ptr<kiriview::VideoOutputRuntime> runtime;
    bool destroyedDuringAttachment = false;
    int publicationAfterDestructionCount = 0;

    runtime = std::make_unique<kiriview::VideoOutputRuntime>(&context,
        kiriview::VideoOutputRuntimeCallbacks {
            [&](QObject* attachedOutput) {
                if (attachedOutput != &output || destroyedDuringAttachment) {
                    return;
                }
                destroyedDuringAttachment = true;
                runtime.reset();
            },
            [&](bool) {
                if (runtime == nullptr) {
                    ++publicationAfterDestructionCount;
                }
            },
        });

    auto* runtimePointer = runtime.get();
    runtimePointer->setVideoOutputAttachment(&output, {}, {});

    QVERIFY(destroyedDuringAttachment);
    QVERIFY(runtime == nullptr);
    QCOMPARE(publicationAfterDestructionCount, 0);
}

void TestVideoOutputRuntime::zoomPublicationCanDestroyRuntime()
{
    QObject context;
    QObject output;
    std::unique_ptr<kiriview::VideoOutputRuntime> runtime;
    bool armed = false;
    bool destroyedDuringPublication = false;

    runtime = std::make_unique<kiriview::VideoOutputRuntime>(&context,
        kiriview::VideoOutputRuntimeCallbacks {
            {},
            [&](bool outputChanged) {
                if (!armed) {
                    return;
                }
                QVERIFY(!outputChanged);
                destroyedDuringPublication = true;
                runtime.reset();
            },
        });
    runtime->setVideoOutputAttachment(&output, {}, {});
    armed = true;

    auto* runtimePointer = runtime.get();
    runtimePointer->setVideoOutputAttachment(
        &output, QRectF(0.0, 0.0, 320.0, 180.0), QRectF(0.0, 0.0, 640.0, 360.0));

    QVERIFY(destroyedDuringPublication);
    QVERIFY(runtime == nullptr);
}

void TestVideoOutputRuntime::outputPublicationCanDestroyRuntime()
{
    QObject context;
    QObject output;
    std::unique_ptr<kiriview::VideoOutputRuntime> runtime;
    bool destroyedDuringPublication = false;

    runtime = std::make_unique<kiriview::VideoOutputRuntime>(&context,
        kiriview::VideoOutputRuntimeCallbacks {
            {},
            [&](bool outputChanged) {
                QVERIFY(outputChanged);
                destroyedDuringPublication = true;
                runtime.reset();
            },
        });

    auto* runtimePointer = runtime.get();
    runtimePointer->setVideoOutputAttachment(&output, {}, {});

    QVERIFY(destroyedDuringPublication);
    QVERIFY(runtime == nullptr);
}

void TestVideoOutputRuntime::renderContextEventCanDestroyRuntime()
{
    QObject context;
    QQuickWindow window;
    QQuickItem output;
    output.setParentItem(window.contentItem());
    std::unique_ptr<kiriview::VideoOutputRuntime> runtime;
    bool armed = false;
    bool destroyedDuringEvent = false;

    runtime = std::make_unique<kiriview::VideoOutputRuntime>(&context,
        kiriview::VideoOutputRuntimeCallbacks {
            {},
            [&](bool outputChanged) {
                if (!armed || destroyedDuringEvent) {
                    return;
                }
                QVERIFY(!outputChanged);
                destroyedDuringEvent = true;
                runtime.reset();
            },
        });
    runtime->setVideoOutputAttachment(&output, {}, {});
    armed = true;

    QEvent event(QEvent::DevicePixelRatioChange);
    QCoreApplication::sendEvent(&window, &event);

    QVERIFY(destroyedDuringEvent);
    QVERIFY(runtime == nullptr);
}

void TestVideoOutputRuntime::sameOutputGeometryReentryPreservesPendingOutputPublication()
{
    QObject context;
    QQuickWindow window;
    QQuickItem output;
    output.setParentItem(window.contentItem());
    const QRectF firstContentRect(0.0, 0.0, 1280.0, 720.0);
    const QRectF firstSourceRect(0.0, 0.0, 1280.0, 720.0);
    const QRectF replacementContentRect(0.0, 0.0, 640.0, 360.0);
    const QRectF replacementSourceRect(0.0, 0.0, 1280.0, 720.0);
    QPointer<QObject> backendOutput;
    QPointer<QObject> lastPublishedOutput;
    std::optional<int> lastPublishedZoom;
    std::unique_ptr<kiriview::VideoOutputRuntime> runtime;
    bool reentered = false;
    int backendSetCount = 0;
    int outputPublicationCount = 0;

    runtime = std::make_unique<kiriview::VideoOutputRuntime>(&context,
        kiriview::VideoOutputRuntimeCallbacks {
            [&](QObject* nextOutput) {
                backendOutput = nextOutput;
                ++backendSetCount;
                if (nextOutput != &output || reentered) {
                    return;
                }
                reentered = true;
                runtime->setVideoOutputAttachment(
                    &output, replacementContentRect, replacementSourceRect);
            },
            [&](bool outputChanged) {
                if (outputChanged) {
                    lastPublishedOutput = runtime->videoOutput();
                    ++outputPublicationCount;
                }
                lastPublishedZoom = runtime->zoomPercent();
            },
        });

    runtime->setVideoOutputAttachment(&output, firstContentRect, firstSourceRect);

    const std::optional<int> expectedZoom = kiriview::videoZoomPercentForRects(
        replacementContentRect, replacementSourceRect, window.effectiveDevicePixelRatio());
    QVERIFY(reentered);
    QCOMPARE(runtime->videoOutput(), &output);
    QCOMPARE(backendOutput.data(), &output);
    QCOMPARE(backendSetCount, 1);
    QCOMPARE(outputPublicationCount, 1);
    QCOMPARE(lastPublishedOutput.data(), &output);
    QCOMPARE(runtime->zoomPercent(), expectedZoom);
    QCOMPARE(lastPublishedZoom, expectedZoom);
}

void TestVideoOutputRuntime::outputPublicationKeepsGetterCoherentDuringReentry()
{
    QObject context;
    QQuickWindow window;
    QQuickItem firstOutput;
    QQuickItem replacementOutput;
    firstOutput.setParentItem(window.contentItem());
    replacementOutput.setParentItem(window.contentItem());
    const QRectF firstContentRect(0.0, 0.0, 1280.0, 720.0);
    const QRectF sourceRect(0.0, 0.0, 1280.0, 720.0);
    const QRectF replacementContentRect(0.0, 0.0, 640.0, 360.0);
    std::unique_ptr<kiriview::VideoOutputRuntime> runtime;
    QObject* getterAfterReentry = nullptr;
    std::optional<int> zoomBeforeReentry;
    std::optional<int> zoomAfterReentry;
    bool reentered = false;
    int outputPublicationCount = 0;

    runtime = std::make_unique<kiriview::VideoOutputRuntime>(&context,
        kiriview::VideoOutputRuntimeCallbacks {
            {},
            [&](bool outputChanged) {
                if (!outputChanged) {
                    return;
                }
                ++outputPublicationCount;
                if (reentered) {
                    return;
                }
                QCOMPARE(runtime->videoOutput(), &firstOutput);
                zoomBeforeReentry = runtime->zoomPercent();
                reentered = true;
                runtime->setVideoOutputAttachment(
                    &replacementOutput, replacementContentRect, sourceRect);
                getterAfterReentry = runtime->videoOutput();
                zoomAfterReentry = runtime->zoomPercent();
            },
        });

    runtime->setVideoOutputAttachment(&firstOutput, firstContentRect, sourceRect);

    const std::optional<int> expectedFirstZoom = kiriview::videoZoomPercentForRects(
        firstContentRect, sourceRect, window.effectiveDevicePixelRatio());
    const std::optional<int> expectedReplacementZoom = kiriview::videoZoomPercentForRects(
        replacementContentRect, sourceRect, window.effectiveDevicePixelRatio());
    QVERIFY(reentered);
    QCOMPARE(getterAfterReentry, &firstOutput);
    QCOMPARE(zoomBeforeReentry, expectedFirstZoom);
    QCOMPARE(zoomAfterReentry, expectedFirstZoom);
    QCOMPARE(outputPublicationCount, 2);
    QCOMPARE(runtime->videoOutput(), &replacementOutput);
    QCOMPARE(runtime->zoomPercent(), expectedReplacementZoom);
}

void TestVideoOutputRuntime::destructorDetachRejectsReentrantAttachment()
{
    QObject context;
    QObject firstOutput;
    QObject replacementOutput;
    kiriview::VideoOutputRuntime* runtimePointer = nullptr;
    bool reenterOnDetach = false;
    int backendSetCount = 0;
    int outputPublicationCount = 0;
    QObject* outputObservedDuringDetach = &firstOutput;
    std::optional<int> zoomObservedDuringDetach = 100;

    auto runtime = std::make_unique<kiriview::VideoOutputRuntime>(&context,
        kiriview::VideoOutputRuntimeCallbacks {
            [&](QObject* output) {
                ++backendSetCount;
                if (!reenterOnDetach || output != nullptr) {
                    return;
                }
                reenterOnDetach = false;
                outputObservedDuringDetach = runtimePointer->videoOutput();
                zoomObservedDuringDetach = runtimePointer->zoomPercent();
                runtimePointer->setVideoOutputAttachment(&replacementOutput, {}, {});
            },
            [&](bool outputChanged) {
                if (outputChanged) {
                    ++outputPublicationCount;
                }
            },
        });
    runtimePointer = runtime.get();
    runtime->setVideoOutputAttachment(&firstOutput, {}, {});
    reenterOnDetach = true;

    runtime.reset();

    QCOMPARE(backendSetCount, 2);
    QCOMPARE(outputPublicationCount, 1);
    QCOMPARE(outputObservedDuringDetach, nullptr);
    QVERIFY(!zoomObservedDuringDetach.has_value());
}

QTEST_MAIN(TestVideoOutputRuntime)

#include "tst_videooutputruntime.moc"
