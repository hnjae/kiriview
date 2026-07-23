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

    [[nodiscard]] quint64 id() const;
    [[nodiscard]] const ImageLoadRequest& request() const;
    [[nodiscard]] const DisplayedImageLocation& location() const;
    [[nodiscard]] const ImageFirstDisplayDecodeContext& firstDisplay() const;
    [[nodiscard]] const QUrl& imageUrl() const;
    [[nodiscard]] ImageDocumentPageKind kind() const;
    [[nodiscard]] const OpenedCollectionScopeLocation& openedCollectionScope() const;
    [[nodiscard]] QUrl containerNavigationUrl() const;
    [[nodiscard]] bool hasContainerNavigationTarget() const;
    [[nodiscard]] ImageDecodeRequest decodeRequest() const;
    [[nodiscard]] bool sameSession(const ImageLoadSession& session) const;

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
