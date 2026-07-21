// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGELOADTYPES_H
#define KIRIVIEW_IMAGELOADTYPES_H

#include "decoding/imagedecoderequest.h"
#include "imagedocumentsourceloadrequest.h"
#include "location/imagelocation.h"
#include "navigation/imagedocumentpagenavigationtypes.h"
#include "rendering/staticimage.h"

#include <QUrl>
#include <QtGlobal>
#include <optional>
#include <utility>

namespace kiriview {
using ImageLoadRequest = ImageDocumentSourceLoadRequest;

class ImageLoadSession
{
public:
    ImageLoadSession() = default;
    ImageLoadSession(quint64 id, ImageLoadRequest request, DisplayedImageLocation location,
        ImageFirstDisplayDecodeContext firstDisplay = {});

    quint64 id() const;
    const ImageLoadRequest& request() const;
    const DisplayedImageLocation& location() const;
    const ImageFirstDisplayDecodeContext& firstDisplay() const;
    const QUrl& imageUrl() const;
    ImageDocumentPageKind kind() const;
    const OpenedCollectionScopeLocation& openedCollectionScope() const;
    QUrl containerNavigationUrl() const;
    bool hasContainerNavigationTarget() const;
    ImageDecodeRequest decodeRequest() const;
    bool sameSession(const ImageLoadSession& session) const;

    void setImageDocumentPageCandidate(const ImageDocumentPageCandidate& candidate);

private:
    quint64 m_id = 0;
    std::optional<ImageLoadRequest> m_request;
    ImageDocumentPageKind m_kind = ImageDocumentPageKind::Image;
    DisplayedImageLocation m_location;
    ImageFirstDisplayDecodeContext m_firstDisplay;
};

}

#endif
