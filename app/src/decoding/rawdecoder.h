// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_RAWDECODER_H
#define KIRIVIEW_RAWDECODER_H

#include "decodedimageresult.h"
#include "imagedecoderequest.h"
#include "imagedecodeworkspace.h"

#include <QByteArray>
#include <memory>

namespace kiriview {
DecodedImageResult decodeRawImageData(const QByteArray& data, const ImageDecodeRequest& request,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {});
}

#endif
