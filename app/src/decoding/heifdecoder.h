// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_HEIFDECODER_H
#define KIRIVIEW_HEIFDECODER_H

#include "decodedimageresult.h"
#include "heifsequencereader.h"

#include <QByteArray>
#include <QtGlobal>
#include <memory>
#include <optional>

namespace kiriview {
class ImageDecodeRequest;

std::optional<DecodedImageResult> decodeHeifImageData(const QByteArray& data,
    const ImageDecodeRequest& request,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {},
    qsizetype retainedInputWorkspaceByteCount = 0);
std::optional<DecodedImageResult> decodePlannedHeifSequenceImageData(const QByteArray& data,
    const ImageDecodeRequest& request, const HeifSequenceWorkspacePlan& plan,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    qsizetype retainedInputWorkspaceByteCount = 0);
}

#endif
