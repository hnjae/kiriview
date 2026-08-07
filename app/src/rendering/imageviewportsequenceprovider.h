// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEVIEWPORTSEQUENCEPROVIDER_H
#define KIRIVIEW_IMAGEVIEWPORTSEQUENCEPROVIDER_H

#include "imageviewportproviderresource.h"

#include <ImageViewport/imagesequenceprovider.h>

#include <memory>
#include <optional>

namespace kiriview {
class ImageViewportSequenceProviderPrivate;

class ImageViewportSequenceProvider final : public ImageSequenceProviderAdapter
{
public:
    ImageViewportSequenceProvider(std::shared_ptr<ImageViewportProviderResource> initialResource,
        ImageViewportProviderResourceFactory resourceFactory, QObject* parent = nullptr);

    [[nodiscard]] ImageSequenceProviderDescriptor descriptor() const override;
    [[nodiscard]] std::optional<StaticDisplayImagePayload> currentStillDisplayImage(
        ImageViewportDemandRevisionToken demandRevision) const;
    bool acceptDisplayedStillDisplayImage(
        ImageViewportPageRole role, ImageViewportDemandRevisionToken demandRevision);
    [[nodiscard]] std::optional<ImageLoadFailure> resolveFailure(
        ImageSequenceProviderFailureReference reference) const;

private:
    std::shared_ptr<ImageViewportSequenceProviderPrivate> m_private;
};
}

#endif
