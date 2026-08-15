// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDECODEREQUEST_H
#define KIRIVIEW_IMAGEDECODEREQUEST_H

#include "decoding/imagesourcerevision.h"
#include "decoding/staticimage.h"
#include "location/imagelocation.h"
#include "location/imageurl.h"

#include <QUrl>
#include <QtGlobal>
#include <utility>

namespace kiriview {
class ImageDecodeRequest
{
public:
    ImageDecodeRequest() = default;

    static ImageDecodeRequest fromUrl(
        quint64 id, const QUrl& imageUrl, ImageFirstDisplayDecodeContext firstDisplay = {})
    {
        return fromLocation(id, DisplayedImageLocation::fromUrl(imageUrl), firstDisplay);
    }

    static ImageDecodeRequest fromLocation(quint64 id, DisplayedImageLocation location,
        ImageFirstDisplayDecodeContext firstDisplay = {})
    {
        return ImageDecodeRequest(id, std::move(location), firstDisplay);
    }

    [[nodiscard]] quint64 id() const { return m_id; }
    [[nodiscard]] const DisplayedImageLocation& location() const { return m_location; }
    [[nodiscard]] const QUrl& imageUrl() const { return m_location.imageUrl(); }
    [[nodiscard]] const OpenedCollectionScopeLocation& openedCollectionScope() const
    {
        return m_location.openedCollectionScope();
    }
    [[nodiscard]] const ImageFirstDisplayDecodeContext& firstDisplay() const
    {
        return m_firstDisplay;
    }
    [[nodiscard]] const ImageSourceRevision& sourceRevision() const { return m_sourceRevision; }
    [[nodiscard]] ImageDecodeRequest withSourceRevision(ImageSourceRevision revision) const
    {
        ImageDecodeRequest request = *this;
        request.m_sourceRevision = std::move(revision);
        return request;
    }
    [[nodiscard]] bool isEmpty() const { return m_location.isEmpty(); }
    [[nodiscard]] bool matches(quint64 id, const QUrl& imageUrl) const
    {
        return m_id == id && sameNormalizedUrl(m_location.imageUrl(), imageUrl);
    }
    [[nodiscard]] bool matches(const ImageDecodeRequest& request) const
    {
        return m_id == request.m_id && m_location == request.m_location;
    }

private:
    ImageDecodeRequest(
        quint64 id, DisplayedImageLocation location, ImageFirstDisplayDecodeContext firstDisplay)
        : m_id(id)
        , m_location(std::move(location))
        , m_firstDisplay(firstDisplay)
    {
    }

    quint64 m_id = 0;
    DisplayedImageLocation m_location;
    ImageFirstDisplayDecodeContext m_firstDisplay;
    ImageSourceRevision m_sourceRevision;
};
}

#endif
