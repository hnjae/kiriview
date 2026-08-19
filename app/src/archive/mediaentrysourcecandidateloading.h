// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAENTRYSOURCECANDIDATELOADING_H
#define KIRIVIEW_MEDIAENTRYSOURCECANDIDATELOADING_H

#include "archive/mediaentrysourcebackend.h"
#include "archive/mediaentrysourceerror.h"
#include "async/imageiojob.h"
#include "async/imageworkerscheduler.h"
#include "location/imagelocation.h"

class QObject;

namespace kiriview {
using MediaEntrySourceEntryLoader = std::function<ImageIoJob(QObject*,
    OpenedCollectionScopeLocation, MediaEntrySourceEntriesCallback, MediaEntrySourceErrorCallback)>;

ImageIoJob startOpenedCollectionEntryList(QObject* receiver,
    OpenedCollectionScopeLocation openedCollectionScope, MediaEntrySourceEntriesCallback callback,
    MediaEntrySourceErrorCallback errorCallback);
ImageIoJob startOpenedCollectionEntryList(QObject* receiver,
    OpenedCollectionScopeLocation openedCollectionScope,
    const ImageWorkerScheduler& workerScheduler, MediaEntrySourceEntriesCallback callback,
    MediaEntrySourceErrorCallback errorCallback);
}

#endif
