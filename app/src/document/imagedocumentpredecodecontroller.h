// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPREDECODECONTROLLER_H
#define KIRIVIEW_IMAGEDOCUMENTPREDECODECONTROLLER_H

#include "async/timerscheduler.h"
#include "decoding/imagedecodedependencies.h"
#include "navigation/imagedocumentpagecandidatelistsource.h"
#include "navigation/imagedocumentpagenavigationtypes.h"
#include "predecode/predecodedimage.h"
#include "predecode/predecoderuntimefacts.h"
#include "predecode/predecodeschedulestate.h"
#include "system/powersaverprovider.h"

#include <QUrl>
#include <QtGlobal>
#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace kiriview {
class ImageDocumentState;
class ImagePredecodeCoordinator;

class ImageDocumentPredecodeController final
{
public:
    using CurrentPageNumberCallback = std::function<int()>;
    using EnsurePageCandidateSnapshotCallback = std::function<void(
        ImageDocumentPageCandidateListContext, ImageDocumentPageCandidateListSnapshotCallback)>;
    using PrimaryDisplayedImageCallback = std::function<std::optional<DisplayedPredecodeImage>()>;
    using FirstDisplayDecodeContextCallback = std::function<ImageFirstDisplayDecodeContext()>;

    ImageDocumentPredecodeController(ImageDocumentState& state,
        PrimaryDisplayedImageCallback primaryDisplayedImage,
        FirstDisplayDecodeContextCallback firstDisplayDecodeContext,
        ImageDecodeDependencies decodeDependencies, qsizetype cacheByteBudget,
        CurrentPageNumberCallback currentPageNumber = {},
        EnsurePageCandidateSnapshotCallback ensurePageCandidateSnapshot = {},
        PowerSaverProvider powerSaverProvider = {}, bool ordinaryDirectMediaPredecodeEnabled = true,
        TimerScheduler timerScheduler = {}, PredecodeThreadCountProvider threadCountProvider = {});
    ~ImageDocumentPredecodeController();
    Q_DISABLE_COPY_MOVE(ImageDocumentPredecodeController)

    void scheduleAdjacentImagePredecode(
        std::optional<DisplayedPredecodeImage> secondaryImage = std::nullopt);
    void scheduleImageNavigationTargetPredecode(const ImageDocumentPageTarget& target,
        int targetPageIndex, std::optional<DisplayedPredecodeImage> secondaryImage = std::nullopt);
    void cancel();
    void clear();
    [[nodiscard]] std::optional<PredecodedImage> findPredecodedImage(
        const DisplayedImageLocation& location) const;

private:
    void scheduleWithConfirmedCandidateSnapshot(
        PredecodeScheduleContext context, quint64 requestId);

    ImageDocumentState& m_state;
    PrimaryDisplayedImageCallback m_primaryDisplayedImage;
    FirstDisplayDecodeContextCallback m_firstDisplayDecodeContext;
    std::unique_ptr<ImagePredecodeCoordinator> m_coordinator;
    CurrentPageNumberCallback m_currentPageNumber;
    EnsurePageCandidateSnapshotCallback m_ensurePageCandidateSnapshot;
    std::shared_ptr<int> m_callbackLifetime = std::make_shared<int>(0);
    quint64 m_candidateSnapshotRequestId = 0;
    bool m_ordinaryDirectMediaPredecodeEnabled = true;
};
}

#endif
