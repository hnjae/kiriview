// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEASYNCCALLBACKS_H
#define KIRIVIEW_IMAGEASYNCCALLBACKS_H

#include "decoding/imagesourcedata.h"

#include <QString>
#include <functional>

namespace kiriview {
using ImageDataCallback = std::function<void(ImageSourceData)>;
using ErrorCallback = std::function<void(const QString&)>;
}

#endif
