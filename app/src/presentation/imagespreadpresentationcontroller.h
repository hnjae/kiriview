// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGESPREADPRESENTATIONCONTROLLER_H
#define KIRIVIEW_IMAGESPREADPRESENTATIONCONTROLLER_H

#include "document/imagedocumentstate.h"
#include "document/imageloadtypes.h"
#include "navigation/imagedocumentpagenavigationtypes.h"
#include "predecode/predecodedimage.h"
#include "presentation/imagespreadnavigation.h"
#include "presentation/imagespreadsecondarypagerefresh.h"

#include <QSize>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QObject;

namespace kiriview {
class ImageSecondaryPageController;
enum class ImageSecondaryPageLoadResult;

class ImageSpreadPresentationController final
{
public:
    using ChangeBatchCallback = std::function<void(const std::vector<ImageDocumentChange>&)>;
    using FindPredecodedImageCallback = std::function<std::optional<PredecodedImage>(const QUrl&)>;
    using PageNavigationSnapshotProvider = std::function<ImageDocumentPageNavigationSnapshot()>;
    using ScheduleAdjacentPredecodeCallback = std::function<void()>;
    using SecondaryImagePreparedCallback
        = std::function<void(ImageLoadSession, std::optional<PredecodedImage>, bool)>;
    using SecondaryImageClearedCallback = std::function<void(bool)>;
    using SecondaryDisplayImageCallback = std::function<std::optional<StaticDisplayImagePayload>()>;

    struct Callbacks
    {
        ChangeBatchCallback changes;
        FindPredecodedImageCallback findPredecodedImage;
        PageNavigationSnapshotProvider pageNavigationSnapshot;
        ScheduleAdjacentPredecodeCallback scheduleAdjacentPredecode;
        SecondaryImagePreparedCallback secondaryImagePrepared;
        SecondaryImageClearedCallback secondaryImageCleared;
        SecondaryDisplayImageCallback secondaryDisplayImage;
    };

    ImageSpreadPresentationController(ImageDocumentState& state, Callbacks callbacks);
    ~ImageSpreadPresentationController();

    int currentLastPageNumber() const;
    ImageDocumentPageActiveNavigationSnapshot activeNavigationSnapshot() const;
    ImageSpreadPageNavigationTarget imageDocumentPageNavigationTarget(
        NavigationDirection direction) const;
    int relativePageNavigationTarget(int offset) const;

    bool twoPageModeEnabled() const;
    void setTwoPageModeEnabled(bool enabled);
    void restoreTwoPageModeEnabled(bool enabled);
    bool twoPageModeAvailable() const;
    bool twoPageModeActive() const;
    bool rightToLeftReadingEnabled() const;
    void setRightToLeftReadingEnabled(bool enabled);
    bool rightToLeftReadingAvailable() const;
    bool rightToLeftReadingActive() const;
    bool secondaryPageVisible() const;
    std::optional<DisplayedPredecodeImage> secondaryDisplayedPredecodeImage() const;

    void commitPrimaryPageSlot(const DisplayedImageLocation& location, QSize imageSize);
    void clearPrimaryPageSlot();
    void refreshSecondaryPage();
    void handleDocumentChange(ImageDocumentChange change);
    bool shouldBeginTransition(int targetPageNumber) const;
    void clearSecondaryPage();
    void shutdown();
    void finishViewportSecondaryPageLoad(
        const ImageLoadSession& session, QSize imageSize, bool presentationRestored);
    void finishViewportSecondaryPageLoadWithError(const ImageLoadSession& session);
    void resetRightToLeftReading();
    void notifyRightToLeftReadingChanged();

private:
    void startSecondaryPageLoad(const QUrl& url);
    void handleSecondaryPageLoadFinished(ImageSecondaryPageLoadResult result,
        const DisplayedImageLocation& location, QSize imageSize);
    void discardSecondaryPage(bool submitShapeChange);
    void finishSecondaryPageAsPrimaryOnly();
    void finishSecondaryPageVisible();
    bool primaryPageIsWide() const;
    bool readingControlsAvailable() const;
    bool secondaryPageVisibleForNavigation() const;
    ImageSpreadPageNavigationContext pageNavigationContext() const;
    void scheduleAdjacentPredecode();
    ImageDocumentPageNavigationSnapshot pageNavigationSnapshot() const;
    void notifyTwoPageModeChanged();
    void notifyChanges(const std::vector<ImageDocumentChange>& changes);

    ImageDocumentState& m_state;
    Callbacks m_callbacks;
    std::unique_ptr<ImageSecondaryPageController> m_secondaryPageController;
    ImageSpreadSecondaryPageRefresh m_secondaryPageRefresh;
    QSize m_committedPrimaryImageSize;
    bool m_twoPageModeEnabled = false;
    bool m_rightToLeftReadingEnabled = false;
    std::optional<bool> m_pendingShapePriorTwoPageMode;
};
}

#endif
