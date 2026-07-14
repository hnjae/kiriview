#pragma once

#include "coordinateresult_p.h"
#include "imageviewport_testhooks_p.h"
#include <ImageViewport/ImageViewport>

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

using namespace ImageViewportTestHooks;

int enumValue(const QMetaObject* metaObject, const char* enumName, const char* key)
{
    int index = metaObject->indexOfEnumerator(enumName);
    if (index < 0) {
        metaObject = &ImageViewportEnums::staticMetaObject;
        index = metaObject->indexOfEnumerator(enumName);
    }
    if (index < 0) {
        return -1;
    }
    return metaObject->enumerator(index).keyToValue(key);
}

ImageViewportRequestStatus requestStatus(const ImageViewport& item)
{
    return item.state().request().status();
}

ImageViewportRequestReason requestReason(const ImageViewport& item)
{
    return item.state().request().reason();
}

ImageViewportDisplayStatus displayStatus(const ImageViewport& item)
{
    return item.state().display().status();
}

ImageViewportPlaybackPhase playbackPhase(const ImageViewport& item)
{
    return item.state().request().playbackPhase();
}

ImageViewportCommandReason viewportCommandReason(const ImageViewport& item)
{
    return item.state().diagnostics().commandReason();
}

QString viewportErrorString(const ImageViewport& item)
{
    return item.state().diagnostics().errorString();
}

QString viewportWarningString(const ImageViewport& item)
{
    return item.state().diagnostics().warningString();
}

int requestStatusValue(const ImageViewport& item) { return static_cast<int>(requestStatus(item)); }

int requestReasonValue(const ImageViewport& item) { return static_cast<int>(requestReason(item)); }

int displayStatusValue(const ImageViewport& item) { return static_cast<int>(displayStatus(item)); }

int playbackPhaseValue(const ImageViewport& item) { return static_cast<int>(playbackPhase(item)); }

int commandReasonValue(const ImageViewport& item)
{
    return static_cast<int>(viewportCommandReason(item));
}

void verifyEnumValues(
    const QMetaObject* metaObject, const char* enumName, const QList<QByteArray>& keys)
{
    int index = metaObject->indexOfEnumerator(enumName);
    if (index < 0) {
        metaObject = &ImageViewportEnums::staticMetaObject;
        index = metaObject->indexOfEnumerator(enumName);
    }
    QVERIFY2(index >= 0, enumName);
    const QMetaEnum enumerator = metaObject->enumerator(index);
    for (const QByteArray& key : keys) {
        QVERIFY2(enumerator.keyToValue(key.constData()) >= 0, key.constData());
    }
}

void verifyRequestStatusReasonPair(const ImageViewport& item)
{
    const ImageViewportRequestStatus status = requestStatus(item);
    const ImageViewportRequestReason reason = requestReason(item);

    const bool valid = (status == ImageViewportRequestStatus::NoRequest
                           && reason == ImageViewportRequestReason::NoRequest)
        || (status == ImageViewportRequestStatus::Loading
            && (reason == ImageViewportRequestReason::ProviderWaiting
                || reason == ImageViewportRequestReason::RequestQueued
                || reason == ImageViewportRequestReason::UploadPending
                || reason == ImageViewportRequestReason::RenderWaiting))
        || (status == ImageViewportRequestStatus::Ready
            && reason == ImageViewportRequestReason::Ready)
        || (status == ImageViewportRequestStatus::Unsupported
            && (reason == ImageViewportRequestReason::UnsupportedRequest
                || reason == ImageViewportRequestReason::InvalidRequest
                || reason == ImageViewportRequestReason::PayloadRejection))
        || (status == ImageViewportRequestStatus::Error
            && (reason == ImageViewportRequestReason::ProviderFailure
                || reason == ImageViewportRequestReason::PayloadRejection
                || reason == ImageViewportRequestReason::RenderFailure));
    const QString message = QStringLiteral("invalid request status/reason pair: %1/%2")
                                .arg(static_cast<int>(status))
                                .arg(static_cast<int>(reason));
    QVERIFY2(valid, qPrintable(message));
}

void verifyInvalidCoordinateResult(CoordinateResult result)
{
    QCOMPARE(result.isValid(), false);
    QCOMPARE(result.x(), 0.0);
    QCOMPARE(result.y(), 0.0);
}

void verifyInvalidCoordinateResult(const ImageViewportCoordinateResult& result)
{
    QCOMPARE(result.isValid(), false);
    QCOMPARE(result.point(), QPointF());
}

ImageViewportCoordinateInput coordinateInput(ImageViewportCoordinateSpace sourceSpace,
    ImageViewportCoordinateSpace targetSpace, QPointF point, QVariant role = {})
{
    ImageViewportCoordinateInput input;
    input.setSourceSpace(sourceSpace);
    input.setTargetSpace(targetSpace);
    input.setPoint(point);
    input.setRole(std::move(role));
    return input;
}

ImageViewportCoordinateResult mapItemToSpread(const ImageViewport& item, double x, double y)
{
    return item.mapPoint(coordinateInput(ImageViewportCoordinateSpace::Item,
        ImageViewportCoordinateSpace::DisplayedSpread, QPointF(x, y)));
}

ImageViewportCoordinateResult mapSpreadToItem(const ImageViewport& item, double x, double y)
{
    return item.mapPoint(coordinateInput(ImageViewportCoordinateSpace::DisplayedSpread,
        ImageViewportCoordinateSpace::Item, QPointF(x, y)));
}

ImageViewportCoordinateResult mapItemToPage(
    const ImageViewport& item, ImageViewportPageRole role, double x, double y)
{
    return item.mapPoint(coordinateInput(ImageViewportCoordinateSpace::Item,
        ImageViewportCoordinateSpace::DisplayedPage, QPointF(x, y), QVariant::fromValue(role)));
}

ImageViewportCoordinateResult mapPageToItem(
    const ImageViewport& item, ImageViewportPageRole role, double x, double y)
{
    return item.mapPoint(coordinateInput(ImageViewportCoordinateSpace::DisplayedPage,
        ImageViewportCoordinateSpace::Item, QPointF(x, y), QVariant::fromValue(role)));
}

ImageViewportCoordinateResult mapSpreadToPage(
    const ImageViewport& item, ImageViewportPageRole role, double x, double y)
{
    return item.mapPoint(coordinateInput(ImageViewportCoordinateSpace::DisplayedSpread,
        ImageViewportCoordinateSpace::DisplayedPage, QPointF(x, y), QVariant::fromValue(role)));
}

ImageViewportCoordinateResult mapPageToSpread(
    const ImageViewport& item, ImageViewportPageRole role, double x, double y)
{
    return item.mapPoint(coordinateInput(ImageViewportCoordinateSpace::DisplayedPage,
        ImageViewportCoordinateSpace::DisplayedSpread, QPointF(x, y), QVariant::fromValue(role)));
}

ImageViewportCoordinateResult mapItemToPrimaryPage(const ImageViewport& item, double x, double y)
{
    return mapItemToPage(item, ImageViewportPageRole::Primary, x, y);
}

ImageViewportCoordinateResult mapPrimaryPageToItem(const ImageViewport& item, double x, double y)
{
    return mapPageToItem(item, ImageViewportPageRole::Primary, x, y);
}

bool containsPrimaryPagePoint(const ImageViewport& item, double x, double y)
{
    return item.containsPoint(coordinateInput(ImageViewportCoordinateSpace::DisplayedPage,
        ImageViewportCoordinateSpace::DisplayedPage, QPointF(x, y),
        QVariant::fromValue(ImageViewportPageRole::Primary)));
}

ImageViewportRevisionToken viewportRequestRevision(const ImageViewport& item)
{
    return item.state().revisions().request();
}

ImageViewportRevisionToken viewportDisplayRevision(const ImageViewport& item)
{
    return item.state().revisions().display();
}

ImageViewportRevisionToken viewportCommandRevision(const ImageViewport& item)
{
    return item.state().revisions().command();
}

ImageViewportRevisionToken revisionTokenProperty(
    const ImageViewport& item, const char* propertyName)
{
    if (qstrcmp(propertyName, "requestRevision") == 0) {
        return viewportRequestRevision(item);
    }
    if (qstrcmp(propertyName, "displayRevision") == 0) {
        return viewportDisplayRevision(item);
    }
    if (qstrcmp(propertyName, "commandRevision") == 0) {
        return viewportCommandRevision(item);
    }
    QTest::qFail(qPrintable(QStringLiteral("unknown revision property: %1").arg(propertyName)),
        __FILE__, __LINE__);
    return {};
}

ImageSequence* viewportPrimarySequence(const ImageViewport& item)
{
    return item.state().primary().sequence();
}

ImageSequence* viewportSecondarySequence(const ImageViewport& item)
{
    return item.state().secondary().sequence();
}

int primaryDisplayedFrame(const ImageViewport& item)
{
    return item.state().primary().display().frame();
}

int primaryRequestedFrame(const ImageViewport& item)
{
    return item.state().primary().request().frame();
}

int secondaryDisplayedFrame(const ImageViewport& item)
{
    return item.state().secondary().display().frame();
}

int secondaryRequestedFrame(const ImageViewport& item)
{
    return item.state().secondary().request().frame();
}

int primaryDisplayedPosition(const ImageViewport& item)
{
    return item.state().primary().display().position();
}

int primaryRequestedPosition(const ImageViewport& item)
{
    return item.state().primary().request().position();
}

int secondaryDisplayedPosition(const ImageViewport& item)
{
    return item.state().secondary().display().position();
}

int secondaryRequestedPosition(const ImageViewport& item)
{
    return item.state().secondary().request().position();
}

QSizeF displayedImageSize(const ImageViewport& item)
{
    const QSizeF size = item.state().primary().display().sourceLogicalSize();
    return size.isValid() ? size : QSizeF(0.0, 0.0);
}

QSizeF displayedSpreadSize(const ImageViewport& item)
{
    return item.state().display().spreadSize();
}

QSizeF primaryDisplayedImageSize(const ImageViewport& item)
{
    return item.state().primary().display().sourceLogicalSize();
}

QSizeF secondaryDisplayedImageSize(const ImageViewport& item)
{
    return item.state().secondary().display().sourceLogicalSize();
}

QRectF visibleSpreadRect(const ImageViewport& item)
{
    return item.state().display().visibleSpreadRect();
}

QRectF contentRect(const ImageViewport& item) { return item.state().display().contentRect(); }

QRectF visibleImageRect(const ImageViewport& item)
{
    return item.state().primary().geometry().displayedVisiblePageRect();
}

int primaryFrameCount(const ImageViewport& item)
{
    return item.state().primary().metadata().frameCount();
}

int secondaryFrameCount(const ImageViewport& item)
{
    return item.state().secondary().metadata().frameCount();
}

int primaryTotalDuration(const ImageViewport& item)
{
    return item.state().primary().metadata().totalDuration();
}

int secondaryTotalDuration(const ImageViewport& item)
{
    return item.state().secondary().metadata().totalDuration();
}

ImageViewportRange primaryFrameSeekBounds(const ImageViewport& item)
{
    return item.state().primary().metadata().frameSeekBounds();
}

ImageViewportRange secondaryFrameSeekBounds(const ImageViewport& item)
{
    return item.state().secondary().metadata().frameSeekBounds();
}

ImageViewportRange primaryPositionSeekBounds(const ImageViewport& item)
{
    return item.state().primary().metadata().positionSeekBounds();
}

ImageViewportRange secondaryPositionSeekBounds(const ImageViewport& item)
{
    return item.state().secondary().metadata().positionSeekBounds();
}

ImageViewportCapabilitySupport primaryTimedPlaybackSupport(const ImageViewport& item)
{
    return item.state().primary().metadata().timedPlaybackSupport();
}

ImageViewportCapabilitySupport secondaryTimedPlaybackSupport(const ImageViewport& item)
{
    return item.state().secondary().metadata().timedPlaybackSupport();
}

ImageViewportCapabilitySupport primaryFrameSeekSupport(const ImageViewport& item)
{
    return item.state().primary().metadata().frameSeekSupport();
}

ImageViewportCapabilitySupport secondaryFrameSeekSupport(const ImageViewport& item)
{
    return item.state().secondary().metadata().frameSeekSupport();
}

ImageViewportCapabilitySupport primaryPositionSeekSupport(const ImageViewport& item)
{
    return item.state().primary().metadata().positionSeekSupport();
}

ImageViewportCapabilitySupport secondaryPositionSeekSupport(const ImageViewport& item)
{
    return item.state().secondary().metadata().positionSeekSupport();
}

QRectF primaryPageRect(const ImageViewport& item)
{
    return item.state().primary().geometry().displayedPageRect();
}

QRectF secondaryPageRect(const ImageViewport& item)
{
    return item.state().secondary().geometry().displayedPageRect();
}

QRectF primaryItemRect(const ImageViewport& item)
{
    return item.state().primary().geometry().displayedItemRect();
}

QRectF secondaryItemRect(const ImageViewport& item)
{
    return item.state().secondary().geometry().displayedItemRect();
}

QRectF visiblePrimaryPageRect(const ImageViewport& item)
{
    return item.state().primary().geometry().displayedVisiblePageRect();
}

QRectF visibleSecondaryPageRect(const ImageViewport& item)
{
    return item.state().secondary().geometry().displayedVisiblePageRect();
}

QSizeF contentSize(const ImageViewport& item) { return item.state().display().contentSize(); }

QPointF contentPosition(const ImageViewport& item)
{
    return item.state().display().contentPosition();
}

QPointF maximumContentPosition(const ImageViewport& item)
{
    return item.state().display().maximumContentPosition();
}

bool horizontalPannable(const ImageViewport& item)
{
    return item.state().display().horizontalPannable();
}

bool verticalPannable(const ImageViewport& item)
{
    return item.state().display().verticalPannable();
}

void acknowledgePendingRenderCommitForTest(ImageViewport& item)
{
    if (!hasPendingRenderCommitForTest(item)) {
        return;
    }
    const quint64 primaryPayloadId = pendingRenderPayloadIdForTest(item);
    const quint64 secondaryPayloadId = secondaryPendingRenderPayloadIdForTest(item) != 0
        ? secondaryPendingRenderPayloadIdForTest(item)
        : primaryPayloadId;
    acknowledgeRenderCommitForTest(item, pendingRenderGenerationForTest(item),
        activeRequestIdForTest(item), primaryPayloadId, secondaryPayloadId);
}

void acknowledgePendingPrimaryRenderCommitForTest(ImageViewport& item)
{
    acknowledgeRenderCommitForTest(item, pendingRenderGenerationForTest(item),
        activeRequestIdForTest(item), pendingRenderPayloadIdForTest(item));
}

void acknowledgePendingPrimaryRenderFailureForTest(ImageViewport& item)
{
    acknowledgeRenderFailureForTest(item, pendingRenderGenerationForTest(item),
        activeRequestIdForTest(item), pendingRenderPayloadIdForTest(item));
}

void verifyRevisionChanged(
    const ImageViewport& item, const char* propertyName, ImageViewportRevisionToken previousToken)
{
    const ImageViewportRevisionToken currentToken = revisionTokenProperty(item, propertyName);
    QVERIFY(currentToken.isValid());
    QVERIFY(currentToken != previousToken);
}

}
