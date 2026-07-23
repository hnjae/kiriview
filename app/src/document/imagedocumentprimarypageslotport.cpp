// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentprimarypageslotport.h"

#include "presentation/imagespreadpresentationcontroller.h"

namespace kiriview {
ImageDocumentPrimaryPageSlotPort::ImageDocumentPrimaryPageSlotPort(
    ImageSpreadPresentationController* spreadController)
    : m_spreadController(spreadController)
{
}

void ImageDocumentPrimaryPageSlotPort::commit(
    const DisplayedImageLocation& location, QSize imageSize) const
{
    if (m_spreadController != nullptr) {
        m_spreadController->commitPrimaryPageSlot(location, imageSize);
    }
}
}
