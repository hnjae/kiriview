// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kiridocumentsession.h"
#include "facade/kiriimagedocument.h"
#include "facade/kiriimageviewportsurface.h"
#include "qml_component_test_support.h"

#include <ImageViewport/imageviewport.h>

#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <memory>

class TestImageViewportComponentBoundary : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void applicationSurfaceStartsWithEmptySnapshot();
    void applicationQmlModuleCreatesSurfaceWithEmptySnapshot();
    void attachedApplicationSurfaceDisplaysDocumentSource();
};

void TestImageViewportComponentBoundary::applicationSurfaceStartsWithEmptySnapshot()
{
    KiriImageViewportSurface surface;

    QCOMPARE(surface.viewport()->state().request().status(), ImageViewportRequestStatus::NoRequest);
    QVERIFY(!surface.viewport()->state().request().acceptedRoleSet().primary());
    QVERIFY(!surface.viewport()->state().request().acceptedRoleSet().secondary());
}

void TestImageViewportComponentBoundary::applicationQmlModuleCreatesSurfaceWithEmptySnapshot()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(KIRIVIEW_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import org.hnjae.kiriview

KiriImageViewportSurface {
    width: 32
    height: 24
}
)",
        QUrl(QStringLiteral("memory:imageviewportcomponentboundary.qml")));

    QVERIFY(waitForQmlComponentReady(component));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const std::unique_ptr<QObject> surface(component.create());
    QVERIFY2(surface != nullptr, qPrintable(component.errorString()));
    auto* viewportSurface = qobject_cast<KiriImageViewportSurface*>(surface.get());
    QVERIFY(viewportSurface != nullptr);
    QCOMPARE(viewportSurface->viewport()->state().request().status(),
        ImageViewportRequestStatus::NoRequest);
}

void TestImageViewportComponentBoundary::attachedApplicationSurfaceDisplaysDocumentSource()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("surface.png"));
    QImage image(40, 20, QImage::Format_RGBA8888);
    image.fill(QColor(20, 40, 60, 255));
    QVERIFY(image.save(path));

    KiriDocumentSession session;
    QQuickWindow window;
    window.resize(160, 120);
    KiriImageViewportSurface surface;
    surface.setParentItem(window.contentItem());
    surface.setSize(QSizeF(160, 120));
    surface.setDocument(session.imageDocument());
    window.show();
    session.setSourceUrl(QUrl::fromLocalFile(path));

    for (int attempt = 0; attempt < 200
        && surface.viewport()->state().request().status() != ImageViewportRequestStatus::Ready;
        ++attempt) {
        window.update();
        window.grabWindow();
        QTest::qWait(10);
    }
    QCOMPARE(session.imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(surface.viewport()->state().request().status(), ImageViewportRequestStatus::Ready);
    QCOMPARE(surface.viewport()->state().display().displayedRoleSet().primary(), true);
    QCOMPARE(session.imageDocument()->displayedUrl(), QUrl::fromLocalFile(path));
}

QTEST_MAIN(TestImageViewportComponentBoundary)

#include "tst_imageviewportcomponentboundary.moc"
