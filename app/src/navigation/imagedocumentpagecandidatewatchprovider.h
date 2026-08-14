// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATEWATCHPROVIDER_H
#define KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATEWATCHPROVIDER_H

#include "async/directorylistingjob.h"
#include "async/imageasynccallbacks.h"
#include "async/imageiojob.h"
#include "imagedocumentpagecandidatecallbacks.h"
#include "imagedocumentpagecandidateloaderror.h"
#include "imagedocumentpagenavigationtypes.h"

#include <QHash>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <functional>
#include <vector>

class QObject;

namespace kiriview {
class ImageDocumentPageCandidateFreshnessState final
{
public:
    void noteSnapshot(const DirectoryItemList& items);
    void apply(std::vector<ImageDocumentPageCandidate>* candidates);

private:
    struct SourceVersion
    {
        std::optional<qint64> byteSize;
        std::optional<qint64> modificationTimeSeconds;

        friend bool operator==(const SourceVersion&, const SourceVersion&) = default;
    };

    void noteItem(const DirectoryItem& item);
    QHash<QString, SourceVersion> m_lastVersionBySource;
    QHash<QString, quint64> m_freshnessBySource;
    quint64 m_nextFreshness = 0;
    bool m_snapshotRecorded = false;
};

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
using ImageDocumentPageCandidateWatchProvider = std::function<ImageIoJob(QObject*, QUrl,
    ImageDocumentPageCandidateWatchSnapshotCallback,
    ImageDocumentPageCandidateWatchSnapshotCallback, ImageDocumentPageCandidateLoadErrorCallback)>;

ImageDocumentPageCandidateWatchProvider defaultImageDocumentPageCandidateWatchProvider(
    DirectoryItemListProvider directoryItemListProvider = {},
    SiblingCandidateAdmissionLimits limits = defaultSiblingCandidateAdmissionLimits());
}

#endif
