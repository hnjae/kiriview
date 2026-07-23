// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPRIMARYPAGESLOTPORT_H
#define KIRIVIEW_IMAGEDOCUMENTPRIMARYPAGESLOTPORT_H

#include "location/imagelocation.h"

#include <QSize>

namespace kiriview {
class ImageSpreadPresentationController;

class ImageDocumentPrimaryPageSlotPort final
{
public:
    explicit ImageDocumentPrimaryPageSlotPort(
        ImageSpreadPresentationController* spreadController = nullptr);

    void commit(const DisplayedImageLocation& location, QSize imageSize) const;

private:
    ImageSpreadPresentationController* m_spreadController = nullptr;
};
}

#endif
