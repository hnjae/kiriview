// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_SUPPORTEDMEDIAFORMATS_H
#define KIRIVIEW_SUPPORTEDMEDIAFORMATS_H

#include <QString>
#include <QStringList>

namespace kiriview::SupportedMediaFormats {
QStringList imageExtensions();
QStringList directVideoExtensions();
QStringList ordinaryMediaExtensions();
QStringList imageMimeTypes();
QStringList directVideoMimeTypes();
bool isSupportedImageFileName(const QString& name);
bool isSupportedDirectVideoFileName(const QString& name);
bool isSupportedOrdinaryMediaFileName(const QString& name);
bool isRawImageExtension(const QString& extension);
}

#endif
