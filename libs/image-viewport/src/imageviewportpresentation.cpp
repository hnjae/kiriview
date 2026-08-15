// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_p.h"
using namespace ImageViewportInternal;

namespace {

ViewportEnginePresentationCommand enginePresentationCommand(
    const ImageViewportPresentationCommand& command)
{
    ViewportEnginePresentationCommand result;
    result.resetViewValue = command.resetView();
    result.fitModeSet = command.hasFitMode();
    result.fitModeValue = command.fitMode();
    result.preferredManualZoomPercentSet = command.hasPreferredManualZoomPercent();
    result.preferredManualZoomPercentValue = command.preferredManualZoomPercent();
    result.zoomStepDeltaSet = command.hasZoomStepDelta();
    result.zoomStepDeltaValue = command.zoomStepDelta();
    result.zoomAnchorSet = command.hasZoomAnchor();
    result.zoomAnchorValue = command.zoomAnchor();
    result.contentPositionSet = command.hasContentPosition();
    result.contentPositionValue = command.contentPosition();
    result.panDeltaSet = command.hasPanDelta();
    result.panDeltaValue = command.panDelta();
    result.contentAnchorSet = command.hasContentAnchor();
    result.contentAnchorValue = command.contentAnchor();
    result.rotationDegreesSet = command.hasRotationDegrees();
    result.rotationDegreesValue = command.rotationDegrees();
    result.mirrorHorizontallySet = command.hasMirrorHorizontally();
    result.mirrorHorizontallyValue = command.mirrorHorizontally();
    result.mirrorVerticallySet = command.hasMirrorVertically();
    result.mirrorVerticallyValue = command.mirrorVertically();
    result.spreadDirectionSet = command.hasSpreadDirection();
    result.spreadDirectionValue = command.spreadDirection();
    result.pageGapSet = command.hasPageGap();
    result.pageGapValue = command.pageGap();
    result.backgroundModeSet = command.hasBackgroundMode();
    result.backgroundModeValue = command.backgroundMode();
    result.backgroundColorSet = command.hasBackgroundColor();
    result.backgroundColorValue = command.backgroundColor();
    result.checkerboardLightColorSet = command.hasCheckerboardLightColor();
    result.checkerboardLightColorValue = command.checkerboardLightColor();
    result.checkerboardDarkColorSet = command.hasCheckerboardDarkColor();
    result.checkerboardDarkColorValue = command.checkerboardDarkColor();
    result.checkerboardCellSizeSet = command.hasCheckerboardCellSize();
    result.checkerboardCellSizeValue = command.checkerboardCellSize();
    result.smoothingSet = command.hasSmoothing();
    result.smoothingValue = command.smoothing();
    result.mipmapSet = command.hasMipmap();
    result.mipmapValue = command.mipmap();
    result.loopingSet = command.hasLooping();
    result.loopingValue = command.looping();
    result.qualityPreferenceSet = command.hasQualityPreference();
    result.qualityPreferenceValue = command.qualityPreference();
    result.exactnessPreferenceSet = command.hasExactnessPreference();
    result.exactnessPreferenceValue = command.exactnessPreference();
    return result;
}

} // namespace

ImageViewportCommandResult ImageViewportPrivate::setPresentation(
    ImageViewportPresentationCommand command)
{
    auto reduced = engine.applyPresentationCommand({ enginePresentationCommand(command),
        command.rotationQuarterTurnDelta(), command.hasRotationQuarterTurnDelta() });
    const CommandOutcome outcome = reduced.outcome();
    const ImageViewportStateSnapshot snapshot = applyEngineTransition(reduced.takeTransition());
    return commandResult(outcome, snapshot);
}

namespace {

ViewportEngineCoordinateQueryRequest coordinateQueryFor(const ImageViewportCoordinateInput& input)
{
    ViewportEngineCoordinateQueryRequest query;
    query.sourceSpace = input.sourceSpace();
    query.targetSpace = input.targetSpace();
    query.point = input.point();
    if (input.role().isNull()) {
        query.roleKind = ViewportEngineCoordinateRoleKind::Null;
    } else if (input.role().canConvert<ImageViewportPageRole>()) {
        query.roleKind = ViewportEngineCoordinateRoleKind::Value;
        query.role = input.role().value<ImageViewportPageRole>();
    } else {
        query.roleKind = ViewportEngineCoordinateRoleKind::Invalid;
    }
    return query;
}

ImageViewportCoordinateResult publicCoordinateResult(
    const ViewportEngineCoordinateQueryResult& result)
{
    const QVariant role = result.role ? QVariant::fromValue(*result.role) : QVariant {};
    return ImageViewportCoordinateResult(result.valid, result.point, result.space, role);
}

} // namespace

ImageViewportCoordinateResult ImageViewportPrivate::mapPoint(
    const ImageViewportCoordinateInput& input) const
{
    return publicCoordinateResult(engine.queryCoordinate(coordinateQueryFor(input)));
}

bool ImageViewportPrivate::containsPoint(const ImageViewportCoordinateInput& input) const
{
    return engine.queryCoordinate(coordinateQueryFor(input)).valid;
}

QRectF ImageViewportPrivate::itemBounds() const
{
    if (width() <= 0.0 || height() <= 0.0) {
        return {};
    }

    return QRectF(0.0, 0.0, width(), height());
}
