// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGESECONDARYPAGESTATE_H
#define KIRIVIEW_IMAGESECONDARYPAGESTATE_H

#include "location/imagelocation.h"

#include <QSize>
#include <optional>

namespace kiriview {
enum class ImageSecondaryPageLoadResult {
    PrimaryOnly,
    Visible,
    Failed,
};

struct ImageSecondaryPageDisplayState
{
    DisplayedImageLocation location;
    QSize imageSize;
};

struct ImageSecondaryPageLoadCompletion
{
    ImageSecondaryPageLoadResult result = ImageSecondaryPageLoadResult::Failed;
    DisplayedImageLocation location;
    QSize imageSize;
    bool clearPresentation = false;
};

class ImageSecondaryPageState final
{
public:
    [[nodiscard]] bool visible() const;
    [[nodiscard]] DisplayedImageLocation displayedImageLocation() const;
    [[nodiscard]] QSize imageSize() const;
    [[nodiscard]] bool stagedPageReplacementMatches(
        const DisplayedImageLocation& location, QSize imageSize) const;

    void clear();
    void stagePageReplacement(const DisplayedImageLocation& location, QSize imageSize);
    void discardStagedPageReplacement();
    void commitStagedPageReplacement(bool includeSecondary);
    ImageSecondaryPageLoadCompletion finishPresentedLoad(
        const DisplayedImageLocation& location, QSize imageSize, bool primaryOnly);
    [[nodiscard]] ImageSecondaryPageLoadCompletion finishFailedLoad(
        const DisplayedImageLocation& location) const;

private:
    std::optional<ImageSecondaryPageDisplayState> m_displayedPage;
    std::optional<ImageSecondaryPageDisplayState> m_stagedPageReplacement;
};
}

#endif
