// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTSOURCELOADREQUEST_H
#define KIRIVIEW_IMAGEDOCUMENTSOURCELOADREQUEST_H

#include "location/imagelocation.h"
#include "navigation/imagedocumentpagenavigationtypes.h"

#include <type_traits>
#include <utility>
#include <variant>

namespace kiriview {
struct ExternalResolvedSource
{
    ResolvedNavigationSource source;
    ImageDocumentPageKind kind = ImageDocumentPageKind::Image;
};

struct SameScopePageTarget
{
    OpenedCollectionScopeLocation scope;
    ImageDocumentPageTarget target;
};

struct ContainerTarget
{
    OpenedCollectionScopeLocation scope;
    ImageDocumentPageTarget target;
};

using ImageDocumentSourceAssignment
    = std::variant<ExternalResolvedSource, SameScopePageTarget, ContainerTarget>;

class ImageDocumentSourceLoadRequest
{
public:
    static ImageDocumentSourceLoadRequest fromExternalSource(ResolvedNavigationSource source,
        ImageDocumentPageKind kind = ImageDocumentPageKind::Image,
        bool preserveTwoPageSpreadTransition = false)
    {
        return ImageDocumentSourceLoadRequest(
            ExternalResolvedSource { std::move(source), kind }, preserveTwoPageSpreadTransition);
    }

    static ImageDocumentSourceLoadRequest fromSameScopePageTarget(ImageDocumentPageTarget target,
        OpenedCollectionScopeLocation scope, bool preserveTwoPageSpreadTransition)
    {
        return ImageDocumentSourceLoadRequest(
            SameScopePageTarget { std::move(scope), std::move(target) },
            preserveTwoPageSpreadTransition);
    }

    static ImageDocumentSourceLoadRequest fromContainerTarget(
        ImageDocumentPageTarget target, OpenedCollectionScopeLocation scope)
    {
        return ImageDocumentSourceLoadRequest(
            ContainerTarget { std::move(scope), std::move(target) }, false);
    }

    const QUrl& sourceUrl() const
    {
        return std::visit(
            [](const auto& assignment) -> const QUrl& {
                using Assignment = std::decay_t<decltype(assignment)>;
                if constexpr (std::is_same_v<Assignment, ExternalResolvedSource>) {
                    return assignment.source.requestedUrl();
                } else {
                    return assignment.target.url;
                }
            },
            m_assignment);
    }
    ImageDocumentPageKind sourceKind() const
    {
        return std::visit(
            [](const auto& assignment) {
                using Assignment = std::decay_t<decltype(assignment)>;
                if constexpr (std::is_same_v<Assignment, ExternalResolvedSource>) {
                    return assignment.kind;
                } else {
                    return assignment.target.kind;
                }
            },
            m_assignment);
    }
    OpenedCollectionScopeLocation openedCollectionScope() const
    {
        return std::visit(
            [](const auto& assignment) {
                using Assignment = std::decay_t<decltype(assignment)>;
                if constexpr (std::is_same_v<Assignment, ExternalResolvedSource>) {
                    return OpenedCollectionScopeLocation::none();
                } else {
                    return assignment.scope;
                }
            },
            m_assignment);
    }
    QUrl containerNavigationUrl() const
    {
        const auto* target = std::get_if<ContainerTarget>(&m_assignment);
        return target == nullptr ? QUrl() : target->scope.fileUrl();
    }
    const ResolvedNavigationSource* externalSource() const
    {
        const auto* source = std::get_if<ExternalResolvedSource>(&m_assignment);
        return source == nullptr ? nullptr : &source->source;
    }
    bool preserveTwoPageSpreadTransition() const { return m_preserveTwoPageSpreadTransition; }
    bool sameScopePageNavigation() const
    {
        return std::holds_alternative<SameScopePageTarget>(m_assignment);
    }
    bool isEmpty() const { return sourceUrl().isEmpty(); }

private:
    explicit ImageDocumentSourceLoadRequest(
        ImageDocumentSourceAssignment assignment, bool preserveTwoPageSpreadTransition)
        : m_assignment(std::move(assignment))
        , m_preserveTwoPageSpreadTransition(preserveTwoPageSpreadTransition)
    {
    }

    ImageDocumentSourceAssignment m_assignment;
    bool m_preserveTwoPageSpreadTransition = false;
};
}

#endif
