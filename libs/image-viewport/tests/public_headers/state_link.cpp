// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <ImageViewport/imageviewportstate.h>

#include <cstring>

#if defined(QT_QML_LIB) || defined(QT_QUICK_LIB)
#error "State-only consumers must not inherit Qt QML or Qt Quick compile requirements"
#endif

int main()
{
    return std::strcmp(ImageViewportStateSnapshot::staticMetaObject.className(),
               "ImageViewportStateSnapshot")
            == 0
        ? 0
        : 1;
}
