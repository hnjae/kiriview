// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAENTRYSOURCECANDIDATELOADING_H
#define KIRIVIEW_MEDIAENTRYSOURCECANDIDATELOADING_H

#include "archive/mediaentrysourceerror.h"
#include "async/imageiojob.h"
#include "async/imageworkerscheduler.h"
#include "location/imagelocation.h"
#include "navigation/imagedocumentpagecandidatecallbacks.h"

class QObject;

namespace kiriview {
ImageIoJob startOpenedCollectionCandidateList(QObject* receiver,
    OpenedCollectionScopeLocation openedCollectionScope,
    ImageDocumentPageCandidatesCallback callback, MediaEntrySourceErrorCallback errorCallback);
ImageIoJob startOpenedCollectionCandidateList(QObject* receiver,
    OpenedCollectionScopeLocation openedCollectionScope,
    const ImageWorkerScheduler& workerScheduler, ImageDocumentPageCandidatesCallback callback,
    MediaEntrySourceErrorCallback errorCallback);
}

#endif
