// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGESECONDARYPAGECONTROLLER_H
#define KIRIVIEW_IMAGESECONDARYPAGECONTROLLER_H

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

class ImageSecondaryPageController final
{
public:
    using LoadFinishedCallback = std::function<void(
        ImageSecondaryPageLoadResult, const DisplayedImageLocation&, const QSize&)>;
    using FindPredecodedImageCallback
        = std::function<std::optional<PredecodedImage>(const DisplayedImageLocation&)>;
    using PreparedImageCallback
        = std::function<void(ImageLoadSession, std::optional<PredecodedImage>)>;

    struct Callbacks
    {
        LoadFinishedCallback loadFinished;
        FindPredecodedImageCallback findPredecodedImage;
        PreparedImageCallback preparedImage;
    };

    explicit ImageSecondaryPageController(Callbacks callbacks);
    ~ImageSecondaryPageController();
    Q_DISABLE_COPY_MOVE(ImageSecondaryPageController)

    [[nodiscard]] bool visible() const;
    [[nodiscard]] DisplayedImageLocation displayedImageLocation() const;
    [[nodiscard]] QSize imageSize() const;

    void startLoad(
        const QUrl& url, const OpenedCollectionScopeLocation& displayedOpenedCollectionScope);
    void clear();
    void cancel();
    void finishProviderLoad(const ImageLoadSession& session, QSize imageSize);
    void finishProviderLoadWithError(const ImageLoadSession& session);

private:
    void finishClaimedLoadWithError(const ImageLoadSession& session);
    void applyLoadCompletion(const ImageSecondaryPageLoadCompletion& completion);

    Callbacks m_callbacks;
    std::unique_ptr<ImageLoader> m_imageLoader;
    ImageSecondaryPageState m_displayState;
};
}

#endif
