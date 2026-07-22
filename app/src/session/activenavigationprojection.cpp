// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "activenavigationprojection.h"

#include <algorithm>
#include <optional>
#include <variant>

namespace {
using namespace kiriview;

ActiveNavigationSnapshot normalizedSnapshot(ActiveNavigationSnapshot snapshot)
{
    if (!snapshot.available || !snapshot.known || snapshot.currentNumber < 1 || snapshot.count < 1
        || snapshot.currentNumber > snapshot.count) {
        return { snapshot.available, false, false, false, false, false, false, 0, 0 };
    }
    snapshot.editable = true;
    return snapshot;
}

std::optional<int> dispatchNumber(ActiveNavigationSnapshot snapshot, int number)
{
    if (!snapshot.known || !snapshot.editable || snapshot.currentNumber < 1 || snapshot.count < 1
        || snapshot.currentNumber > snapshot.count) {
        return std::nullopt;
    }
    return std::clamp(number, 1, snapshot.count);
}

ActiveNavigationDispatchOperation previousOperation(ActiveNavigationSourceKind sourceKind)
{
    switch (sourceKind) {
    case ActiveNavigationSourceKind::OrdinaryDirectMedia:
        return OpenPreviousDirectMediaNavigationOperation {};
    case ActiveNavigationSourceKind::ImageDocumentPages:
        return OpenPreviousImageDocumentPageOperation {};
    case ActiveNavigationSourceKind::None:
        return {};
    }
    return {};
}

ActiveNavigationDispatchOperation nextOperation(ActiveNavigationSourceKind sourceKind)
{
    switch (sourceKind) {
    case ActiveNavigationSourceKind::OrdinaryDirectMedia:
        return OpenNextDirectMediaNavigationOperation {};
    case ActiveNavigationSourceKind::ImageDocumentPages:
        return OpenNextImageDocumentPageOperation {};
    case ActiveNavigationSourceKind::None:
        return {};
    }
    return {};
}

ActiveNavigationDispatchOperation numberedOperation(
    ActiveNavigationSourceKind sourceKind, int number)
{
    switch (sourceKind) {
    case ActiveNavigationSourceKind::OrdinaryDirectMedia:
        return OpenDirectMediaNavigationAtNumberOperation { number };
    case ActiveNavigationSourceKind::ImageDocumentPages:
        return OpenImageDocumentPageAtNumberOperation { number };
    case ActiveNavigationSourceKind::None:
        return {};
    }
    return {};
}

ActiveNavigationDispatchPlan dispatch(ActiveNavigationDispatchOperation operation)
{
    if (std::holds_alternative<std::monostate>(operation)) {
        return {};
    }
    return { operation, ActiveNavigationDispatchOutcome::Dispatch };
}
}

namespace kiriview {
bool ActiveNavigationDispatchPlan::shouldDispatch() const
{
    return outcome == ActiveNavigationDispatchOutcome::Dispatch
        && !std::holds_alternative<std::monostate>(operation);
}

ActiveNavigationSnapshot projectActiveNavigation(ActiveNavigationSourceKind sourceKind,
    DirectMediaActiveNavigationInput directMediaInput,
    ImageDocumentPageActiveNavigationSnapshot imageDocumentPageSnapshot,
    bool fileDeletionInProgress)
{
    ActiveNavigationSnapshot snapshot;
    switch (sourceKind) {
    case ActiveNavigationSourceKind::OrdinaryDirectMedia:
        if (!directMediaInput.known) {
            snapshot.available = true;
            break;
        }
        snapshot = normalizedSnapshot({ true, true, true,
            directMediaInput.boundaryState.canOpenPrevious,
            directMediaInput.boundaryState.canOpenNext, directMediaInput.boundaryState.atKnownFirst,
            directMediaInput.boundaryState.atKnownLast,
            directMediaInput.boundaryState.currentNumber, directMediaInput.boundaryState.count });
        break;
    case ActiveNavigationSourceKind::ImageDocumentPages:
        snapshot = normalizedSnapshot({ true, imageDocumentPageSnapshot.known, true,
            imageDocumentPageSnapshot.canOpenPrevious, imageDocumentPageSnapshot.canOpenNext,
            imageDocumentPageSnapshot.atKnownFirst, imageDocumentPageSnapshot.atKnownLast,
            imageDocumentPageSnapshot.currentNumber, imageDocumentPageSnapshot.count });
        break;
    case ActiveNavigationSourceKind::None:
        break;
    }
    if (fileDeletionInProgress) {
        snapshot.editable = false;
        snapshot.canOpenPrevious = false;
        snapshot.canOpenNext = false;
    }
    return snapshot;
}

ActiveNavigationBoundaryScope activeNavigationBoundaryScopeForSource(
    ActiveNavigationSourceKind sourceKind)
{
    switch (sourceKind) {
    case ActiveNavigationSourceKind::OrdinaryDirectMedia:
        return ActiveNavigationBoundaryScope::DirectMedia;
    case ActiveNavigationSourceKind::ImageDocumentPages:
        return ActiveNavigationBoundaryScope::ImageDocumentPage;
    case ActiveNavigationSourceKind::None:
        return ActiveNavigationBoundaryScope::None;
    }
    return ActiveNavigationBoundaryScope::None;
}

ActiveNavigationDispatchRequest previousActiveNavigationDispatchRequest()
{
    return ActiveNavigationDispatchRequest { ActiveNavigationDispatchRequestKind::Previous, 0 };
}

ActiveNavigationDispatchRequest nextActiveNavigationDispatchRequest()
{
    return ActiveNavigationDispatchRequest { ActiveNavigationDispatchRequestKind::Next, 0 };
}

ActiveNavigationDispatchRequest firstActiveNavigationDispatchRequest()
{
    return ActiveNavigationDispatchRequest { ActiveNavigationDispatchRequestKind::First, 1 };
}

ActiveNavigationDispatchRequest lastActiveNavigationDispatchRequest()
{
    return ActiveNavigationDispatchRequest { ActiveNavigationDispatchRequestKind::Last, 0 };
}

ActiveNavigationDispatchRequest numberedActiveNavigationDispatchRequest(int number)
{
    return ActiveNavigationDispatchRequest { ActiveNavigationDispatchRequestKind::Number, number };
}

ActiveNavigationDispatchPlan activeNavigationDispatchPlan(ActiveNavigationSourceKind sourceKind,
    ActiveNavigationSnapshot snapshot, ActiveNavigationDispatchRequest request)
{
    if (!snapshot.available) {
        return {};
    }
    switch (request.kind) {
    case ActiveNavigationDispatchRequestKind::Previous:
        if (snapshot.canOpenPrevious) {
            return dispatch(previousOperation(sourceKind));
        }
        return snapshot.known && snapshot.editable && snapshot.atKnownFirst
            ? ActiveNavigationDispatchPlan { {}, ActiveNavigationDispatchOutcome::FirstBoundary }
            : ActiveNavigationDispatchPlan {};
    case ActiveNavigationDispatchRequestKind::Next:
        if (snapshot.canOpenNext) {
            return dispatch(nextOperation(sourceKind));
        }
        return snapshot.known && snapshot.editable && snapshot.atKnownLast
            ? ActiveNavigationDispatchPlan { {}, ActiveNavigationDispatchOutcome::LastBoundary }
            : ActiveNavigationDispatchPlan {};
    case ActiveNavigationDispatchRequestKind::First:
        if (!snapshot.known || !snapshot.editable || snapshot.atKnownFirst) {
            return {};
        }
        request.number = 1;
        break;
    case ActiveNavigationDispatchRequestKind::Last:
        if (!snapshot.known || !snapshot.editable || snapshot.atKnownLast) {
            return {};
        }
        request.number = snapshot.count;
        break;
    case ActiveNavigationDispatchRequestKind::Number:
        break;
    }
    const std::optional<int> number = dispatchNumber(snapshot, request.number);
    return number.has_value() ? dispatch(numberedOperation(sourceKind, *number))
                              : ActiveNavigationDispatchPlan {};
}
}
