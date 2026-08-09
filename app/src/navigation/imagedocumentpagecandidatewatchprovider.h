// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATEWATCHPROVIDER_H
#define KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATEWATCHPROVIDER_H

#include "async/imageasynccallbacks.h"
#include "async/imageiojob.h"
#include "imagedocumentpagecandidatecallbacks.h"
#include "imagedocumentpagecandidateloaderror.h"
#include "imagedocumentpagenavigationtypes.h"

#include <QList>
#include <QUrl>
#include <QtGlobal>
#include <functional>
#include <vector>

class QObject;

namespace kiriview {
class ImageDocumentPageCandidateRefreshAdmission final
{
public:
    [[nodiscard]] bool requestRefresh();
    void beginRefresh();
    [[nodiscard]] bool finishRefresh();
    void invalidatePendingRefresh();
    [[nodiscard]] quint64 epoch() const;
    [[nodiscard]] bool acceptsEpoch(quint64 epoch) const;

private:
    bool m_refreshPending = false;
    bool m_refreshRequested = false;
    quint64 m_epoch = 0;
};

using ImageDocumentPageCandidateWatchSnapshotCallback
    = std::function<void(std::vector<ImageDocumentPageCandidate>)>;
using ImageDocumentPageCandidateWatchDeletedCallback = std::function<void(QList<QUrl>)>;
using ImageDocumentPageCandidateWatchProvider = std::function<ImageIoJob(QObject*, QUrl,
    ImageDocumentPageCandidateWatchSnapshotCallback,
    ImageDocumentPageCandidateWatchSnapshotCallback, ImageDocumentPageCandidateWatchDeletedCallback,
    ImageDocumentPageCandidateLoadErrorCallback)>;

ImageDocumentPageCandidateWatchProvider defaultImageDocumentPageCandidateWatchProvider();
}

#endif
