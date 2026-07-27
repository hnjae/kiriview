// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGELOADER_H
#define KIRIVIEW_IMAGELOADER_H

#include "imageloadfailure.h"
#include "imageloadsessiontracker.h"
#include "imageloadtypes.h"
#include "navigation/imagedocumentpagecandidatelistsource.h"
#include "predecode/predecodedimage.h"

#include <QObject>
#include <QUrl>
#include <functional>
#include <optional>

namespace kiriview {
class ImageLoader final : public QObject
{
    Q_OBJECT
public:
    using SourcePreparedCallback = std::function<void(ImageLoadSession)>;
    using ErrorCallback = std::function<void(ImageLoadSession, ImageLoadFailure)>;
    using UnsupportedOpenedCollectionVideoCallback = std::function<void(ImageLoadSession)>;
    using FindPredecodedImageCallback = std::function<std::optional<PredecodedImage>(const QUrl&)>;
    using EnsurePageCandidateSnapshotCallback = std::function<void(
        ImageDocumentPageCandidateListContext, ImageDocumentPageCandidateListSnapshotCallback)>;
    using TargetStartedCallback = std::function<void(ImageLoadSession)>;
    using ResolvedImageCallback
        = std::function<void(ImageLoadSession, std::optional<PredecodedImage>)>;

    struct Callbacks
    {
        ErrorCallback error;
        UnsupportedOpenedCollectionVideoCallback unsupportedOpenedCollectionVideo;
        FindPredecodedImageCallback findPredecodedImage;
        SourcePreparedCallback sourcePrepared;
        EnsurePageCandidateSnapshotCallback ensurePageCandidateSnapshot;
        TargetStartedCallback targetStarted;
        ResolvedImageCallback resolvedImage;
    };

    ImageLoader();
    explicit ImageLoader(Callbacks callbacks);

    void start(ImageLoadRequest request, ImageFirstDisplayDecodeContext firstDisplayContext = {});
    void cancel();
    [[nodiscard]] bool isCurrentSession(const ImageLoadSession& session) const;
    [[nodiscard]] std::optional<ImageLoadSession> claimCurrentSession(
        const ImageLoadSession& session);

private:
    void startOpenedCollectionLoad(const ImageLoadSession& session);
    void finishOpenedCollectionSnapshot(const ImageLoadSession& session,
        const ImageDocumentPageCandidateListSource& candidateSource,
        const ImageDocumentPageCandidateListSnapshotResult& result);
    void finishOpenedCollectionCandidates(
        const ImageLoadSession& session, const std::vector<ImageDocumentPageCandidate>& candidates);
    bool tryReportUnsupportedOpenedCollectionVideo(const ImageLoadSession& session);
    bool startProviderTarget(const ImageLoadSession& session);
    void resolveProviderImage(ImageLoadSession session);
    [[nodiscard]] std::optional<PredecodedImage> matchingPredecodedImage(
        const ImageLoadSession& session) const;

    Callbacks m_callbacks;
    ImageLoadSessionTracker m_sessionTracker;
};
}

#endif
