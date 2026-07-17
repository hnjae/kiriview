#include "imageviewport_p.h"
#include "viewportcommandoutcome_p.h"
#include "viewportitemtransaction_p.h"
#include "viewportprovidertransporteffects_p.h"

using namespace ImageViewportInternal;

ImageViewportCommandResult ImageViewportPrivate::setPresentation(
    ImageViewportPresentationCommand command)
{
    const auto reduced = engine.applyPresentationCommand({ command });
    ViewportCommandResult result
        = ImageViewportInternal::CommandOutcome::fromEngineCommand(reduced.command);
    mergeChanges(result.transition.changes, reduced.changes);
    appendProviderTransport(
        result.transition.providerAfterPublication, reduced.providerEffects[0], PageRole::Primary);
    appendProviderTransport(result.transition.providerAfterPublication, reduced.providerEffects[1],
        PageRole::Secondary);
    const ImageViewportStateSnapshot snapshot = applyEngineTransition(result.transition);
    return commandResult(result.outcome, snapshot);
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
