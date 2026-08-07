// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_QIMAGEREADERDECODER_H
#define KIRIVIEW_QIMAGEREADERDECODER_H

#include "decodedimageresult.h"
#include "imagedecoderequest.h"
#include "imageinputclassification.h"

#include <QByteArray>
#include <memory>

namespace kiriview {
DecodedImageResult decodeQImageReaderImageData(const QByteArray& data,
    const ImageDecodeRequest& request, QtRasterFormat format,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {});
}

#endif
