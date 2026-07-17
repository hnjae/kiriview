#include "imageviewport_p.h"
using namespace ImageViewportInternal;

ImageViewportCommandResult ImageViewportPrivate::setPresentation(
    ImageViewportPresentationCommand command)
{
    auto reduced = engine.applyPresentationCommand({ command });
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
