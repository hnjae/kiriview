// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATELOADING_H
#define KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATELOADING_H

#include "async/directorylistingjob.h"
#include "async/imageasynccallbacks.h"
#include "async/imageiojob.h"
#include "navigation/imagedocumentpagecandidatecallbacks.h"

#include <QUrl>

class QObject;

namespace kiriview {
ImageIoJob startDirectoryImageDocumentPageCandidateList(QObject* receiver, const QUrl& directoryUrl,
    ImageDocumentPageCandidatesCallback callback, KioOperationFailureCallback errorCallback);
ImageIoJob startDirectoryImageDocumentPageCandidateList(QObject* receiver, const QUrl& directoryUrl,
    ImageDocumentPageCandidatesCallback callback, KioOperationFailureCallback errorCallback,
    DirectoryItemListProvider directoryItemListProvider);
ImageIoJob startDirectoryContainerCandidateList(QObject* receiver, const QUrl& directoryUrl,
    ContainerCandidatesCallback callback, KioOperationFailureCallback errorCallback);
ImageIoJob startDirectoryContainerCandidateList(QObject* receiver, const QUrl& directoryUrl,
    ContainerCandidatesCallback callback, KioOperationFailureCallback errorCallback,
    DirectoryItemListProvider directoryItemListProvider);
}

#endif
