// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_AVIFCOMPATIBILITY_H
#define KIRIVIEW_AVIFCOMPATIBILITY_H

#include <QByteArray>
#include <QByteArrayView>

namespace kiriview {
QByteArray avifDataWithCompatibilityFixes(QByteArrayView data);
}

#endif
