// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kiriimageviewportsurface.h"
#include "qml_component_test_support.h"

#include <ImageViewport/imageviewportstate.h>

#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTest>
#include <QUrl>
#include <memory>

class TestImageViewportComponentBoundary : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void applicationSurfaceStartsWithEmptySnapshot();
    void applicationQmlModuleCreatesSurfaceWithEmptySnapshot();
};

void TestImageViewportComponentBoundary::applicationSurfaceStartsWithEmptySnapshot()
{
    KiriImageViewportSurface surface;

    QCOMPARE(surface.state().request().status(), ImageViewportRequestStatus::NoRequest);
    QVERIFY(!surface.state().request().acceptedRoleSet().primary());
    QVERIFY(!surface.state().request().acceptedRoleSet().secondary());
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
    property bool emptySnapshot:
        !state.request.acceptedRoleSet.primary
        && !state.request.acceptedRoleSet.secondary
}
)",
        QUrl(QStringLiteral("memory:imageviewportcomponentboundary.qml")));

    QVERIFY(waitForQmlComponentReady(component));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const std::unique_ptr<QObject> surface(component.create());
    QVERIFY2(surface != nullptr, qPrintable(component.errorString()));
    QVERIFY(qobject_cast<KiriImageViewportSurface*>(surface.get()) != nullptr);
    QVERIFY(surface->property("emptySnapshot").toBool());
}

QTEST_MAIN(TestImageViewportComponentBoundary)

#include "tst_imageviewportcomponentboundary.moc"
