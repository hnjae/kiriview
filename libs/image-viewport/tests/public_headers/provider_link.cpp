// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <ImageViewport/imagesequenceprovider.h>

#if defined(QT_QML_LIB) || defined(QT_QUICK_LIB)
#error "Provider-only consumers must not inherit Qt QML or Qt Quick compile requirements"
#endif

int main()
{
    return imageViewportProviderLog().categoryName() != nullptr
            && ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)).isValid()
        ? 0
        : 1;
}
