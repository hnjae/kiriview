// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_STATICIMAGEDISPLAYSOURCEHELPERS_P_H
#define KIRIVIEW_STATICIMAGEDISPLAYSOURCEHELPERS_P_H

#include <QImage>
#include <QSize>
#include <QString>

namespace kiriview {
QSize boundedPreviewSize(QSize imageSize, int maximumLongEdge);
QImage scaledDisplayImage(const QImage& image, QSize size);
void setStaticImageDisplaySourceError(QString* errorString, const QString& message);
}

#endif
