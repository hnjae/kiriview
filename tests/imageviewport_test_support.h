#pragma once

#include "imageviewport.h"

#include <QtCore/QList>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaProperty>
#include <QtCore/QPointF>
#include <QtCore/QScopeGuard>
#include <QtGui/QImage>
#include <QtGui/QTransform>
#include <QtQuick/QQuickItem>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

static_assert(std::is_abstract_v<ImageSequenceProviderAdapter>,
    "ImageSequenceProviderAdapter must remain an abstract public extension-point base");

namespace {

int enumValue(const QMetaObject* metaObject, const char* enumName, const char* key)
{
    const int index = metaObject->indexOfEnumerator(enumName);
    if (index < 0) {
        return -1;
    }
    return metaObject->enumerator(index).keyToValue(key);
}

void verifyEnumValues(
    const QMetaObject* metaObject, const char* enumName, const QList<QByteArray>& keys)
{
    const int index = metaObject->indexOfEnumerator(enumName);
    QVERIFY2(index >= 0, enumName);
    const QMetaEnum enumerator = metaObject->enumerator(index);
    for (const QByteArray& key : keys) {
        QVERIFY2(enumerator.keyToValue(key.constData()) >= 0, key.constData());
    }
}

void verifyRequestStatusReasonPair(const ImageViewport& item)
{
    const QMetaObject* metaObject = item.metaObject();
    const int status = item.property("requestStatus").toInt();
    const int reason = item.property("requestReason").toInt();

    const bool valid = (status == enumValue(metaObject, "RequestStatus", "NoRequest")
                           && reason == enumValue(metaObject, "RequestReason", "NoRequest"))
        || (status == enumValue(metaObject, "RequestStatus", "Loading")
            && (reason == enumValue(metaObject, "RequestReason", "ProviderWaiting")
                || reason == enumValue(metaObject, "RequestReason", "RequestQueued")
                || reason == enumValue(metaObject, "RequestReason", "UploadPending")
                || reason == enumValue(metaObject, "RequestReason", "RenderWaiting")))
        || (status == enumValue(metaObject, "RequestStatus", "Ready")
            && reason == enumValue(metaObject, "RequestReason", "Ready"))
        || (status == enumValue(metaObject, "RequestStatus", "Unsupported")
            && (reason == enumValue(metaObject, "RequestReason", "UnsupportedRequest")
                || reason == enumValue(metaObject, "RequestReason", "InvalidRequest")
                || reason == enumValue(metaObject, "RequestReason", "PayloadRejection")))
        || (status == enumValue(metaObject, "RequestStatus", "Error")
            && (reason == enumValue(metaObject, "RequestReason", "ProviderFailure")
                || reason == enumValue(metaObject, "RequestReason", "PayloadRejection")
                || reason == enumValue(metaObject, "RequestReason", "RenderFailure")));
    const QString message
        = QStringLiteral("invalid request status/reason pair: %1/%2").arg(status).arg(reason);
    QVERIFY2(valid, qPrintable(message));
}

void verifyInvalidCoordinateResult(const QVariantMap& result)
{
    QCOMPARE(result.value(QStringLiteral("valid")).toBool(), false);
    QCOMPARE(result.value(QStringLiteral("x")).toDouble(), 0.0);
    QCOMPARE(result.value(QStringLiteral("y")).toDouble(), 0.0);
}

}
