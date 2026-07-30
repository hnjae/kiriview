// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGESECONDARYPAGECONTROLLER_H
#define KIRIVIEW_IMAGESECONDARYPAGECONTROLLER_H

#include "document/imageloadfailure.h"
#include "document/imageloadtypes.h"
#include "predecode/predecodedimage.h"
#include "presentation/imagesecondarypagestate.h"

#include <QSize>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace kiriview {
class ImageLoader;

enum class ImageSecondaryPageReplacementMetadataResult {
    Stale,
    PrimaryOnly,
    Secondary,
};

struct ImageSecondaryPageReplacementCommit
{
    ImageLoadSession session;
    QSize imageSize;
};

class ImageSecondaryPageController final
{
public:
    using LoadFinishedCallback = std::function<void(
        ImageSecondaryPageLoadResult, const DisplayedImageLocation&, const QSize&)>;
    using FindPredecodedImageCallback
        = std::function<std::optional<PredecodedImage>(const DisplayedImageLocation&)>;
    using PreparedImageCallback
        = std::function<void(ImageLoadSession, std::optional<PredecodedImage>)>;
    using PageReplacementPreparedImageCallback
        = std::function<void(quint64, ImageLoadSession, std::optional<PredecodedImage>)>;
    using PageReplacementFailedCallback
        = std::function<void(quint64, ImageLoadSession, ImageLoadFailure)>;

    struct Callbacks
    {
        LoadFinishedCallback loadFinished;
        FindPredecodedImageCallback findPredecodedImage;
        PreparedImageCallback preparedImage;
        PageReplacementPreparedImageCallback pageReplacementPreparedImage;
        PageReplacementFailedCallback pageReplacementFailed;
    };

    explicit ImageSecondaryPageController(Callbacks callbacks);
    ~ImageSecondaryPageController();
    Q_DISABLE_COPY_MOVE(ImageSecondaryPageController)

    [[nodiscard]] bool visible() const;
    [[nodiscard]] DisplayedImageLocation displayedImageLocation() const;
    [[nodiscard]] QSize imageSize() const;

    void startLoad(
        const QUrl& url, const OpenedCollectionScopeLocation& displayedOpenedCollectionScope);
    void beginPageReplacement(quint64 primarySessionId);
    [[nodiscard]] bool startPageReplacementLoad(quint64 primarySessionId, const QUrl& url,
        const OpenedCollectionScopeLocation& openedCollectionScope);
    [[nodiscard]] ImageSecondaryPageReplacementMetadataResult finishPageReplacementProviderLoad(
        quint64 primarySessionId, const ImageLoadSession& session, QSize imageSize);
    [[nodiscard]] bool commitPageReplacement(quint64 primarySessionId,
        const std::optional<ImageSecondaryPageReplacementCommit>& secondary);
    void cancelPageReplacement(quint64 primarySessionId = 0);
    [[nodiscard]] bool pageReplacementPending() const;
    void clear();
    void cancel();
    void finishProviderLoad(const ImageLoadSession& session, QSize imageSize);
    void finishProviderLoadWithError(const ImageLoadSession& session);

private:
    enum class PageReplacementPhase {
        PrimaryOnly,
        PreparingSecondary,
        Secondary,
    };

    void finishClaimedLoadWithError(const ImageLoadSession& session);
    void applyLoadCompletion(const ImageSecondaryPageLoadCompletion& completion);

    Callbacks m_callbacks;
    std::unique_ptr<ImageLoader> m_imageLoader;
    ImageSecondaryPageState m_displayState;
    quint64 m_pageReplacementPrimarySessionId = 0;
    PageReplacementPhase m_pageReplacementPhase = PageReplacementPhase::PrimaryOnly;
    std::optional<ImageLoadSession> m_pageReplacementPreparedSession;
    std::optional<ImageLoadSession> m_pageReplacementSecondarySession;
    QSize m_pageReplacementSecondaryImageSize;
};
}

#endif
