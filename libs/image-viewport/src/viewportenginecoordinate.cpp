// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "coordinateresult_p.h"
#include "imageviewportvalidation_p.h"
#include "presentationgeometry_p.h"
#include "viewportengine_p.h"
#include "viewportengineprojection_p.h"
#include "viewportenginestate_p.h"

namespace {

bool coordinateSpaceValid(ImageViewportCoordinateSpace space)
{
    switch (space) {
    case ImageViewportCoordinateSpace::Item:
    case ImageViewportCoordinateSpace::DisplayedSpread:
    case ImageViewportCoordinateSpace::DisplayedPage:
        return true;
    }
    return false;
}

bool coordinateUsesPage(const ViewportEngineCoordinateQueryRequest& input)
{
    return input.sourceSpace == ImageViewportCoordinateSpace::DisplayedPage
        || input.targetSpace == ImageViewportCoordinateSpace::DisplayedPage;
}

bool roleIsDisplayed(ImageViewportPageRole role, ImageViewportRoleSet displayedRoles)
{
    return role == ImageViewportPageRole::Primary ? displayedRoles.primary()
                                                  : displayedRoles.secondary();
}

ViewportEngineCoordinateQueryResult invalidCoordinateResult(
    const ViewportEngineCoordinateQueryRequest& input,
    std::optional<ImageViewportPageRole> role = std::nullopt)
{
    ViewportEngineCoordinateQueryResult result;
    result.space = coordinateSpaceValid(input.targetSpace) ? input.targetSpace
                                                           : ImageViewportCoordinateSpace::Item;
    result.role = role;
    return result;
}

ViewportEngineCoordinateQueryResult coordinateResult(
    const ViewportEngineCoordinateQueryRequest& input, CoordinateResult mapped,
    std::optional<ImageViewportPageRole> role)
{
    ViewportEngineCoordinateQueryResult result;
    result.valid = mapped.isValid();
    result.space = input.targetSpace;
    result.role = role;
    if (mapped.isValid()) {
        result.point = QPointF(mapped.x(), mapped.y());
    }
    return result;
}

} // namespace

ViewportEngineCoordinateQueryResult ViewportEngine::queryCoordinate(
    const ViewportEngineCoordinateQueryRequest& input) const
{
    if (!coordinateSpaceValid(input.sourceSpace) || !coordinateSpaceValid(input.targetSpace)
        || !ImageViewportInternal::isFinitePoint(input.point)) {
        return invalidCoordinateResult(input);
    }

    const bool usesPage = coordinateUsesPage(input);
    std::optional<ImageViewportPageRole> role;
    if (usesPage) {
        if (input.roleKind != ViewportEngineCoordinateRoleKind::Value
            || !ImageViewportInternal::isValidPageRole(input.role)) {
            return invalidCoordinateResult(input);
        }
        role = input.role;
    } else if (input.roleKind != ViewportEngineCoordinateRoleKind::Null) {
        return invalidCoordinateResult(input);
    }

    const auto& display = m_state->displayState.display;
    if (role && !roleIsDisplayed(*role, projectViewportDisplayedRoleSet(display))) {
        return invalidCoordinateResult(input, role);
    }

    const PresentationGeometry::State state = geometryState();
    CoordinateResult mapped;
    if (input.sourceSpace == input.targetSpace) {
        bool valid = false;
        switch (input.sourceSpace) {
        case ImageViewportCoordinateSpace::Item:
            valid
                = PresentationGeometry::containsItemPoint(state, input.point.x(), input.point.y());
            break;
        case ImageViewportCoordinateSpace::DisplayedSpread:
            valid = PresentationGeometry::containsSpreadPoint(
                state, input.point.x(), input.point.y());
            break;
        case ImageViewportCoordinateSpace::DisplayedPage:
            valid = PresentationGeometry::containsPagePoint(
                state, *role, input.point.x(), input.point.y());
            break;
        }
        if (valid) {
            mapped = CoordinateResult(true, input.point.x(), input.point.y());
        }
    } else if (input.sourceSpace == ImageViewportCoordinateSpace::Item
        && input.targetSpace == ImageViewportCoordinateSpace::DisplayedSpread) {
        mapped = PresentationGeometry::itemToSpread(state, input.point.x(), input.point.y());
    } else if (input.sourceSpace == ImageViewportCoordinateSpace::DisplayedSpread
        && input.targetSpace == ImageViewportCoordinateSpace::Item) {
        mapped = PresentationGeometry::spreadToItem(state, input.point.x(), input.point.y());
    } else if (input.sourceSpace == ImageViewportCoordinateSpace::Item
        && input.targetSpace == ImageViewportCoordinateSpace::DisplayedPage) {
        mapped = PresentationGeometry::itemToPage(state, *role, input.point.x(), input.point.y());
    } else if (input.sourceSpace == ImageViewportCoordinateSpace::DisplayedPage
        && input.targetSpace == ImageViewportCoordinateSpace::Item) {
        mapped = PresentationGeometry::pageToItem(state, *role, input.point.x(), input.point.y());
    } else if (input.sourceSpace == ImageViewportCoordinateSpace::DisplayedSpread
        && input.targetSpace == ImageViewportCoordinateSpace::DisplayedPage) {
        mapped = PresentationGeometry::spreadToPage(state, *role, input.point.x(), input.point.y());
    } else if (input.sourceSpace == ImageViewportCoordinateSpace::DisplayedPage
        && input.targetSpace == ImageViewportCoordinateSpace::DisplayedSpread) {
        mapped = PresentationGeometry::pageToSpread(state, *role, input.point.x(), input.point.y());
    }

    return coordinateResult(input, mapped, role);
}
