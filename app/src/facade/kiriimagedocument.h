// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_KIRIIMAGEDOCUMENT_H
#define KIRIVIEW_KIRIIMAGEDOCUMENT_H

#include "archive/mediaentrysourcebackend.h"
#include "document/imagedocumenttypes.h"
#include "metadata/embeddedmetadata.h"
#include "navigation/imagedocumentpagecandidatelistsource.h"
#include "navigation/imagedocumentpagenavigationtypes.h"
#include "predecode/predecodedimage.h"

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>
#include <memory>
#include <optional>
#include <vector>

namespace kiriview {
class OpenedCollectionScopeLocation;
class ImageDocumentRuntime;
struct KiriImageDocumentComposition;
}

class KiriDocumentSession;
class ImageViewport;

class KiriImageDocument : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KiriImageDocument)
    QML_UNCREATABLE("KiriImageDocument is owned by KiriDocumentSession")

    Q_PROPERTY(QUrl sourceUrl READ sourceUrl NOTIFY sourceUrlChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString presentationLifecycleToken READ presentationLifecycleToken NOTIFY
            presentationLifecycleTokenChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(
        QString windowTitleFileName READ windowTitleFileName NOTIFY windowTitleFileNameChanged)
    Q_PROPERTY(QUrl displayedUrl READ displayedUrl NOTIFY displayedUrlChanged)
    Q_PROPERTY(bool completeAuthoritativeDisplayAvailable READ completeAuthoritativeDisplayAvailable
            NOTIFY completeAuthoritativeDisplayAvailableChanged)
    Q_PROPERTY(QSize imageSize READ imageSize NOTIFY imageSizeChanged)
    Q_PROPERTY(QSize primaryImageSize READ primaryImageSize NOTIFY imageSizeChanged)
    Q_PROPERTY(QSize secondaryImageSize READ secondaryImageSize NOTIFY twoPageModeChanged)
    Q_PROPERTY(bool viewportHorizontallyPannable READ viewportHorizontallyPannable NOTIFY
            viewportFrameChanged)
    Q_PROPERTY(
        bool viewportVerticallyPannable READ viewportVerticallyPannable NOTIFY viewportFrameChanged)
    Q_PROPERTY(bool viewportPannable READ viewportPannable NOTIFY viewportFrameChanged)
    Q_PROPERTY(
        double horizontalScrollPosition READ horizontalScrollPosition NOTIFY viewportFrameChanged)
    Q_PROPERTY(
        double horizontalScrollPageSize READ horizontalScrollPageSize NOTIFY viewportFrameChanged)
    Q_PROPERTY(
        double verticalScrollPosition READ verticalScrollPosition NOTIFY viewportFrameChanged)
    Q_PROPERTY(
        double verticalScrollPageSize READ verticalScrollPageSize NOTIFY viewportFrameChanged)
    Q_PROPERTY(bool zoomPercentKnown READ zoomPercentKnown NOTIFY zoomPercentKnownChanged)
    Q_PROPERTY(double zoomPercent READ zoomPercent NOTIFY zoomPercentChanged)
    Q_PROPERTY(ZoomMode zoomMode READ zoomMode NOTIFY zoomModeChanged)
    Q_PROPERTY(ZoomMode fitModeSelection READ fitModeSelection NOTIFY zoomModeChanged)
    Q_PROPERTY(int minimumManualZoomPercent READ minimumManualZoomPercent CONSTANT)
    Q_PROPERTY(int maximumManualZoomPercent READ maximumManualZoomPercent NOTIFY
            maximumManualZoomPercentChanged)
    Q_PROPERTY(double zoomStepFactor READ zoomStepFactor CONSTANT)
    Q_PROPERTY(int currentPageNumber READ currentPageNumber NOTIFY pageNavigationChanged)
    Q_PROPERTY(int currentLastPageNumber READ currentLastPageNumber NOTIFY pageNavigationChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pageNavigationChanged)
    Q_PROPERTY(bool containerNavigationAvailable READ containerNavigationAvailable NOTIFY
            containerNavigationChanged)
    Q_PROPERTY(bool ordinaryDirectMediaScopeActive READ ordinaryDirectMediaScopeActive NOTIFY
            imageDocumentSourceScopeChanged)
    Q_PROPERTY(bool openedCollectionScopeActive READ openedCollectionScopeActive NOTIFY
            imageDocumentSourceScopeChanged)
    Q_PROPERTY(bool fileDeletionInProgress READ fileDeletionInProgress NOTIFY
            fileDeletionInProgressChanged)
    Q_PROPERTY(bool twoPageModeEnabled READ twoPageModeEnabled NOTIFY twoPageModeChanged)
    Q_PROPERTY(bool twoPageModeAvailable READ twoPageModeAvailable NOTIFY twoPageModeChanged)
    Q_PROPERTY(bool rightToLeftReadingEnabled READ rightToLeftReadingEnabled NOTIFY
            rightToLeftReadingChanged)
    Q_PROPERTY(bool rightToLeftReadingAvailable READ rightToLeftReadingAvailable NOTIFY
            rightToLeftReadingChanged)
    Q_PROPERTY(bool secondaryPageVisible READ secondaryPageVisible NOTIFY twoPageModeChanged)
    Q_PROPERTY(bool unsupportedOpenedCollectionVideo READ unsupportedOpenedCollectionVideo NOTIFY
            unsupportedOpenedCollectionVideoChanged)

public:
    enum class Status {
        Null,
        Loading,
        Ready,
        Error,
    };
    Q_ENUM(Status)

    enum class ZoomMode {
        Fit,
        FitHeight,
        FitWidth,
        Manual,
    };
    Q_ENUM(ZoomMode)

    enum class DeletionMode {
        MoveToTrash,
        DeletePermanently,
    };
    Q_ENUM(DeletionMode)

    explicit KiriImageDocument(QObject* parent = nullptr);
    ~KiriImageDocument() override;
    Q_DISABLE_COPY_MOVE(KiriImageDocument)

    [[nodiscard]] QUrl sourceUrl() const;
    [[nodiscard]] kiriview::ImageDocumentPageKind sourceKind() const;

    [[nodiscard]] Status status() const;
    [[nodiscard]] bool loading() const;
    [[nodiscard]] QString presentationLifecycleToken() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] QString windowTitleFileName() const;
    [[nodiscard]] QUrl displayedUrl() const;
    [[nodiscard]] bool completeAuthoritativeDisplayAvailable() const;
    [[nodiscard]] kiriview::OpenedCollectionScopeLocation displayedOpenedCollectionScope() const;
    [[nodiscard]] QSize imageSize() const;
    [[nodiscard]] QSize primaryImageSize() const;
    [[nodiscard]] QSize secondaryImageSize() const;
    [[nodiscard]] bool viewportHorizontallyPannable() const;
    [[nodiscard]] bool viewportVerticallyPannable() const;
    [[nodiscard]] bool viewportPannable() const;
    [[nodiscard]] double horizontalScrollPosition() const;
    [[nodiscard]] double horizontalScrollPageSize() const;
    [[nodiscard]] double verticalScrollPosition() const;
    [[nodiscard]] double verticalScrollPageSize() const;
    [[nodiscard]] bool zoomPercentKnown() const;
    [[nodiscard]] double zoomPercent() const;
    [[nodiscard]] ZoomMode zoomMode() const;
    [[nodiscard]] ZoomMode fitModeSelection() const;
    [[nodiscard]] int minimumManualZoomPercent() const;
    [[nodiscard]] int maximumManualZoomPercent() const;
    [[nodiscard]] double zoomStepFactor() const;
    [[nodiscard]] int currentPageNumber() const;
    [[nodiscard]] int currentLastPageNumber() const;
    [[nodiscard]] int pageCount() const;
    [[nodiscard]] kiriview::ImageDocumentPageNavigationSnapshot pageNavigationSnapshot() const;
    [[nodiscard]] const kiriview::ImageDocumentPageCandidateListSnapshot&
    confirmedPageCandidateSnapshot() const;
    [[nodiscard]] kiriview::ImageDocumentPageActiveNavigationSnapshot
    activeNavigationSnapshot() const;
    [[nodiscard]] bool containerNavigationAvailable() const;
    [[nodiscard]] bool ordinaryDirectMediaScopeActive() const;
    [[nodiscard]] bool openedCollectionScopeActive() const;
    [[nodiscard]] bool fileDeletionInProgress() const;
    [[nodiscard]] bool twoPageModeEnabled() const;
    [[nodiscard]] bool twoPageModeAvailable() const;
    [[nodiscard]] bool rightToLeftReadingEnabled() const;
    [[nodiscard]] bool rightToLeftReadingAvailable() const;
    [[nodiscard]] bool secondaryPageVisible() const;
    [[nodiscard]] bool unsupportedOpenedCollectionVideo() const;
    [[nodiscard]] std::optional<kiriview::DisplayedPredecodeImage>
    primaryDisplayedPredecodeImage() const;
    [[nodiscard]] kiriview::ImageFirstDisplayDecodeContext firstDisplayDecodeContext() const;
    [[nodiscard]] const kiriview::EmbeddedMetadata& embeddedMetadata() const;

    void attachImageViewport(ImageViewport* viewport);
    void detachImageViewport(ImageViewport* viewport);

    void openPreviousPage();
    void openNextPage();
    void openPreviousSinglePage();
    void openNextSinglePage();
    void openImageAtPage(int pageNumber);
    void openPreviousContainer();
    void openNextContainer();
    void deleteDisplayedFile(KiriImageDocument::DeletionMode mode);
    Q_INVOKABLE void rotateClockwise();
    Q_INVOKABLE void rotateCounterclockwise();
    Q_INVOKABLE void flipHorizontally();
    Q_INVOKABLE void flipVertically();
    Q_INVOKABLE [[nodiscard]] double steppedManualZoomPercent(double stepCount) const;
    Q_INVOKABLE bool requestManualZoomPercent(double zoomPercent);
    Q_INVOKABLE bool requestZoomByStep(double stepCount, QPointF viewportAnchorPoint);
    Q_INVOKABLE bool requestZoomByStepAtCenter(double stepCount);
    Q_INVOKABLE bool requestViewportPinchUpdate(
        double scaleFactor, QPointF previousViewportCentroid, QPointF currentViewportCentroid);
    Q_INVOKABLE bool requestFitMode(KiriImageDocument::ZoomMode zoomMode);
    Q_INVOKABLE bool requestToggleFitOrActualSize(QPointF viewportPoint);
    Q_INVOKABLE bool requestViewportPanBy(double deltaX, double deltaY);
    Q_INVOKABLE bool requestViewportPanToInitialScanPosition();
    Q_INVOKABLE bool requestViewportPanToFinalScanPosition();
    Q_INVOKABLE bool requestViewportScanForward();
    Q_INVOKABLE bool requestViewportScanBackward();
    void requestNextViewportTargetAnchorAtEnd();
    Q_INVOKABLE [[nodiscard]] QPointF nearestImageViewportPoint(QPointF viewportPoint) const;
    Q_INVOKABLE void requestToggleTwoPageMode();
    Q_INVOKABLE void requestToggleRightToLeftReading();
    Q_INVOKABLE bool submitHorizontalScrollPosition(double position);
    Q_INVOKABLE bool submitVerticalScrollPosition(double position);

Q_SIGNALS:
    void sourceUrlChanged();
    void statusChanged();
    void loadingChanged();
    void presentationLifecycleTokenChanged();
    void errorStringChanged();
    void windowTitleFileNameChanged();
    void displayedUrlChanged();
    void completeAuthoritativeDisplayAvailableChanged();
    void imageSizeChanged();
    void viewportFrameChanged();
    void zoomPercentKnownChanged();
    void zoomPercentChanged();
    void zoomModeChanged();
    void maximumManualZoomPercentChanged();
    void pageNavigationChanged();
    void containerNavigationChanged();
    void imageDocumentSourceScopeChanged();
    void fileDeletionInProgressChanged();
    void twoPageModeChanged();
    void rightToLeftReadingChanged();
    void unsupportedOpenedCollectionVideoChanged();
    void embeddedMetadataChanged();
    void unsupportedOpenedCollectionVideoEntered(const QString& message);
    void containerNavigationBoundaryReached(const QString& message);

private:
    friend class KiriDocumentSession;

    explicit KiriImageDocument(kiriview::KiriImageDocumentComposition composition, QObject* parent);

    Q_SIGNAL void documentSessionSnapshotChanged();

    void setSourceUrl(const QUrl& sourceUrl);
    void setSource(const kiriview::ResolvedNavigationSource& source);
    [[nodiscard]] kiriview::MediaEntrySourceVideoPlaybackDeviceResult
    loadOpenedCollectionVideoPlaybackDevice(
        const kiriview::OpenedCollectionScopeLocation& openedCollectionScope,
        const QUrl& videoUrl) const;
    void setTwoPageModeEnabled(bool enabled);
    void setRightToLeftReadingEnabled(bool enabled);
    void handleDocumentChanges(const std::vector<kiriview::ImageDocumentChange>& changes);

    std::unique_ptr<kiriview::ImageDocumentRuntime> m_runtime;
};

#endif
