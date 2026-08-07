// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DIRECTORYLISTINGJOB_H
#define KIRIVIEW_DIRECTORYLISTINGJOB_H

#include "async/imageiojob.h"
#include "system/kiooperationfailure.h"

#include <KFileItem>
#include <QUrl>
#include <functional>

class QObject;

namespace kiriview {
using DirectoryItemListCallback = std::function<void(KFileItemList)>;
using DirectoryItemListProvider = std::function<ImageIoJob(
    QObject*, QUrl, DirectoryItemListCallback, KioOperationFailureCallback)>;

ImageIoJob startDirectoryItemList(QObject* receiver, QUrl directoryUrl,
    DirectoryItemListCallback callback, KioOperationFailureCallback errorCallback);
ImageIoJob startDirectoryItemList(QObject* receiver, QUrl directoryUrl,
    DirectoryItemListCallback callback, KioOperationFailureCallback errorCallback,
    DirectoryItemListProvider provider);
DirectoryItemListProvider defaultDirectoryItemListProvider();
}

#endif
