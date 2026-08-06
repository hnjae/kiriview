// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "kiriimagedecoder.h"

#include "imagedecodepipeline.h"

namespace kiriview {
DecodedImageResult decodeImageData(const QByteArray& data)
{
    return decodeImageData(data, ImageDecodeRequest {});
}

DecodedImageResult decodeImageData(const QByteArray& data, const ImageDecodeRequest& request)
{
    return decodeImageData(data, request, {});
}

DecodedImageResult decodeImageData(const QByteArray& data, const ImageDecodeRequest& request,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    const ImageDecodeRequest revisionedRequest = request.sourceRevision().isValid()
        ? request
        : request.withSourceRevision(ImageSourceRevision::fromData(data));
    return decodeImageDataWithDefaultRouter(data, revisionedRequest, std::move(workspaceBudget));
}

}
