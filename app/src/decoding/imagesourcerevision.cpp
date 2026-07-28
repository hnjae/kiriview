// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagesourcerevision.h"

#include <QCryptographicHash>

namespace {
constexpr qsizetype sha256DigestSize = 32;
}

namespace kiriview {
ImageSourceRevision ImageSourceRevision::fromData(QByteArrayView data)
{
    return ImageSourceRevision(
        QCryptographicHash::hash(data, QCryptographicHash::Algorithm::Sha256));
}

bool ImageSourceRevision::isValid() const { return m_digest.size() == sha256DigestSize; }
}
