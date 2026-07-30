// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGESPREADPRESENTATIONCONTROLLER_H
#define KIRIVIEW_IMAGESPREADPRESENTATIONCONTROLLER_H

#include "document/imagedocumentstate.h"
#include "document/imageloadfailure.h"
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

enum class ImageSpreadPageReplacementPairingResult {
    Stale,
    PrimaryOnly,
    PreparingSecondary,
};

enum class ImageSpreadPageReplacementSecondaryMetadataResult {
    Stale,
    PrimaryOnly,
    Secondary,
};

struct ImageSpreadPreparedSecondaryPage
{
    ImageLoadSession session;
    QSize imageSize;
};

class ImageSpreadPresentationController final
{
public:
    using ChangeBatchCallback = std::function<void(const std::vector<ImageDocumentChange>&)>;
    using FindPredecodedImageCallback
        = std::function<std::optional<PredecodedImage>(const DisplayedImageLocation&)>;
    using PageNavigationSnapshotProvider = std::function<ImageDocumentPageNavigationSnapshot()>;
    using ScheduleAdjacentPredecodeCallback = std::function<void()>;
    using SecondaryImagePreparedCallback
        = std::function<void(ImageLoadSession, std::optional<PredecodedImage>)>;
    using SecondaryImageClearedCallback = std::function<void()>;
    using SecondaryPresentationTeardownCallback = std::function<void()>;
    using SecondaryDisplayImageCallback = std::function<std::optional<StaticDisplayImagePayload>()>;
    using NavigationSecondaryImagePreparedCallback
        = std::function<void(quint64, ImageLoadSession, std::optional<PredecodedImage>)>;
    using NavigationSecondaryImagePreparationFailedCallback
        = std::function<void(quint64, ImageLoadSession, ImageLoadFailure)>;

    struct Callbacks
    {
        ChangeBatchCallback changes;
        FindPredecodedImageCallback findPredecodedImage;
        PageNavigationSnapshotProvider pageNavigationSnapshot;
        ScheduleAdjacentPredecodeCallback scheduleAdjacentPredecode;
        SecondaryImagePreparedCallback secondaryImagePrepared;
        SecondaryImageClearedCallback secondaryImageCleared;
        SecondaryDisplayImageCallback secondaryDisplayImage;
        NavigationSecondaryImagePreparedCallback navigationSecondaryImagePrepared;
        NavigationSecondaryImagePreparationFailedCallback navigationSecondaryImagePreparationFailed;
        SecondaryPresentationTeardownCallback secondaryPresentationTeardown;
    };

    ImageSpreadPresentationController(ImageDocumentState& state, Callbacks callbacks);
    ~ImageSpreadPresentationController();
    Q_DISABLE_COPY_MOVE(ImageSpreadPresentationController)

    [[nodiscard]] int currentLastPageNumber() const;
    [[nodiscard]] ImageDocumentPageActiveNavigationSnapshot activeNavigationSnapshot() const;
    [[nodiscard]] ImageSpreadPageNavigationTarget imageDocumentPageNavigationTarget(
        NavigationDirection direction) const;
    [[nodiscard]] int relativePageNavigationTarget(int offset) const;

    [[nodiscard]] bool twoPageModeEnabled() const;
    void setTwoPageModeEnabled(bool enabled);
    [[nodiscard]] bool twoPageModeAvailable() const;
    [[nodiscard]] bool twoPageModeActive() const;
    [[nodiscard]] bool rightToLeftReadingEnabled() const;
    void setRightToLeftReadingEnabled(bool enabled);
    [[nodiscard]] bool rightToLeftReadingAvailable() const;
    [[nodiscard]] bool rightToLeftReadingActive() const;
    [[nodiscard]] bool secondaryPageVisible() const;
    [[nodiscard]] std::optional<DisplayedPredecodeImage> secondaryDisplayedPredecodeImage() const;

    void commitPrimaryPageSlot(const DisplayedImageLocation& location, QSize imageSize);
    [[nodiscard]] ImageSpreadPageReplacementPairingResult beginPageReplacementPairing(
        const ImageLoadSession& primarySession, QSize primaryImageSize);
    [[nodiscard]] ImageSpreadPageReplacementSecondaryMetadataResult
    finishPageReplacementSecondaryMetadata(quint64 primarySessionId,
        const ImageLoadSession& secondarySession, QSize secondaryImageSize);
    [[nodiscard]] bool commitPageReplacementPresentation(const ImageLoadSession& primarySession,
        QSize primaryImageSize,
        const std::optional<ImageSpreadPreparedSecondaryPage>& secondary = std::nullopt);
    void cancelPageReplacementPairing(quint64 primarySessionId = 0);
    [[nodiscard]] bool pageReplacementPairingPending() const;
    void clearPrimaryPageSlot();
    void refreshSecondaryPage();
    void handleDocumentChange(ImageDocumentChange change);
    void clearSecondaryPage();
    void shutdown();
    void finishViewportSecondaryPageLoad(const ImageLoadSession& session, QSize imageSize);
    void finishViewportSecondaryPageLoadWithError(const ImageLoadSession& session);
    void resetRightToLeftReading();
    void notifyRightToLeftReadingChanged();

private:
    enum class SecondaryPageDiscardIntent {
        Silent,
        PresentationShapeChange,
        PresentationTeardown,
    };

    enum class PageReplacementPhase {
        PrimaryOnly,
        PreparingSecondary,
        Secondary,
    };

    struct PendingPageReplacement
    {
        ImageLoadSession primarySession;
        QSize primaryImageSize;
        PageReplacementPhase phase = PageReplacementPhase::PrimaryOnly;
        std::optional<ImageSpreadPreparedSecondaryPage> secondary;
    };

    void startSecondaryPageLoad(const QUrl& url);
    void handleSecondaryPageLoadFinished(ImageSecondaryPageLoadResult result,
        const DisplayedImageLocation& location, QSize imageSize);
    void discardSecondaryPage(SecondaryPageDiscardIntent intent);
    void finishSecondaryPageAsPrimaryOnly();
    void finishSecondaryPageVisible();
    [[nodiscard]] bool primaryPageIsWide() const;
    [[nodiscard]] bool primaryPageSupportsSpread(
        const DisplayedImageLocation& location, QSize imageSize) const;
    [[nodiscard]] bool readingControlsAvailable() const;
    [[nodiscard]] bool secondaryPageVisibleForNavigation() const;
    [[nodiscard]] ImageSpreadPageNavigationContext pageNavigationContext() const;
    void scheduleAdjacentPredecode();
    [[nodiscard]] ImageDocumentPageNavigationSnapshot pageNavigationSnapshot() const;
    void notifyTwoPageModeChanged();
    void notifyChanges(const std::vector<ImageDocumentChange>& changes);

    ImageDocumentState& m_state;
    Callbacks m_callbacks;
    std::unique_ptr<ImageSecondaryPageController> m_secondaryPageController;
    ImageSpreadSecondaryPageRefresh m_secondaryPageRefresh;
    QSize m_committedPrimaryImageSize;
    bool m_twoPageModeEnabled = false;
    bool m_rightToLeftReadingEnabled = false;
    bool m_pendingShapeChange = false;
    std::optional<PendingPageReplacement> m_pendingPageReplacement;
};
}

#endif
