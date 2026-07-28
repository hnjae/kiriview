// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGESOURCEREVISION_H
#define KIRIVIEW_IMAGESOURCEREVISION_H

#include <QByteArray>
#include <QByteArrayView>
#include <utility>

namespace kiriview {
class ImageSourceRevision final
{
public:
    ImageSourceRevision() = default;

    static ImageSourceRevision fromData(QByteArrayView data);

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] const QByteArray& digest() const { return m_digest; }

    friend bool operator==(const ImageSourceRevision& left, const ImageSourceRevision& right)
    {
        return left.m_digest == right.m_digest;
    }

private:
    explicit ImageSourceRevision(QByteArray digest)
        : m_digest(std::move(digest))
    {
    }

    QByteArray m_digest;
};
}

#endif
