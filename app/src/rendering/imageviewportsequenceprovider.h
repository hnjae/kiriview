// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEVIEWPORTSEQUENCEPROVIDER_H
#define KIRIVIEW_IMAGEVIEWPORTSEQUENCEPROVIDER_H

#include "imageviewportproviderresource.h"

#include <ImageViewport/imagesequenceprovider.h>

#include <memory>

namespace kiriview {
class ImageViewportSequenceProvider final : public ImageSequenceProviderAdapter
{
public:
    explicit ImageViewportSequenceProvider(
        std::shared_ptr<ImageViewportProviderResource> resource, QObject* parent = nullptr);

    ImageSequenceProviderDescriptor descriptor() const override;

private:
    std::shared_ptr<ImageViewportProviderResource> m_resource;
};
}

#endif
