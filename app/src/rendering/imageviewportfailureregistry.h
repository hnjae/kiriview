// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEVIEWPORTFAILUREREGISTRY_H
#define KIRIVIEW_IMAGEVIEWPORTFAILUREREGISTRY_H

#include "document/imageloadfailure.h"

#include <ImageViewport/imagesequenceprovider.h>

#include <QtGlobal>
#include <memory>
#include <optional>

namespace kiriview {
class ImageViewportFailureRegistry final
{
public:
    ImageViewportFailureRegistry();
    ~ImageViewportFailureRegistry();
    Q_DISABLE_COPY_MOVE(ImageViewportFailureRegistry)

    ImageSequenceProviderFailureHandle* registerFailure(ImageLoadFailure failure);
    [[nodiscard]] std::optional<ImageLoadFailure> resolve(
        ImageSequenceProviderFailureReference reference) const;
    [[nodiscard]] qsizetype size() const;

private:
    class State;
    std::shared_ptr<State> m_state;
};
}

#endif
