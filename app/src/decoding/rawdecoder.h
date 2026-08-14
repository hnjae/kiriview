// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_RAWDECODER_H
#define KIRIVIEW_RAWDECODER_H

#include "decodedimageresult.h"
#include "imagedecoderequest.h"
#include "imagedecodeworkspace.h"

#include <QByteArray>
#include <expected>
#include <memory>

namespace kiriview {
inline constexpr qsizetype rawImageOpenWorkspaceByteCount = qsizetype { 64 } * 1024 * 1024;

class OpenedRawImage final
{
public:
    ~OpenedRawImage();
    OpenedRawImage(OpenedRawImage&& other) noexcept;
    OpenedRawImage& operator=(OpenedRawImage&& other) noexcept;
    Q_DISABLE_COPY(OpenedRawImage)

    [[nodiscard]] qsizetype retainedWorkspaceByteCount() const;
    [[nodiscard]] qsizetype productionAdditionalPeakByteCount() const;
    DecodedImageResult decode(
        const ImageDecodeRequest& request, ImageDecodeWorkspaceLease producerLease) &&;

private:
    class Private;
    explicit OpenedRawImage(std::unique_ptr<Private> state);

    std::unique_ptr<Private> d;
    friend std::expected<std::unique_ptr<OpenedRawImage>, DecodedImageFailure> openRawImageData(
        const QByteArray&, ImageDecodeWorkspaceLease);
};

using OpenedRawImageResult = std::expected<std::unique_ptr<OpenedRawImage>, DecodedImageFailure>;

OpenedRawImageResult openRawImageData(
    const QByteArray& data, ImageDecodeWorkspaceLease openWorkspaceLease);
DecodedImageResult decodeRawImageData(const QByteArray& data, const ImageDecodeRequest& request,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {});
}

#endif
