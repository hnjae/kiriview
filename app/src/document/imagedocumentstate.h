// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTSTATE_H
#define KIRIVIEW_IMAGEDOCUMENTSTATE_H

#include "imagedocumentchangebatcher.h"
#include "imagedocumenttypes.h"
#include "imageloadfailure.h"
#include "location/imagelocation.h"
#include "metadata/embeddedmetadata.h"
#include "navigation/imagedocumentpagenavigationtypes.h"

#include <QString>
#include <QUrl>
#include <memory>
#include <optional>

namespace kiriview {
struct ImageDocumentSelectedTarget
{
    QUrl url;
    ImageDocumentPageKind kind = ImageDocumentPageKind::Image;
    OpenedCollectionScopeLocation openedCollectionScope;

    friend bool operator==(
        const ImageDocumentSelectedTarget& left, const ImageDocumentSelectedTarget& right)
    {
        return left.url == right.url && left.kind == right.kind
            && left.openedCollectionScope == right.openedCollectionScope;
    }
};

class ImageDocumentState
{
public:
    using ChangeCallback = ImageDocumentChangeBatcher::ChangeCallback;
    using ChangeBatch = ImageDocumentChangeBatcher::Batch;

    explicit ImageDocumentState(ChangeCallback changeCallback = {});
    explicit ImageDocumentState(ImageDocumentChangeBatcher& changes);

    ChangeBatch beginChangeBatch();

    [[nodiscard]] const ImageDocumentSelectedTarget& selectedTarget() const;
    [[nodiscard]] const QUrl& sourceUrl() const;
    [[nodiscard]] ImageDocumentPageKind sourceKind() const;
    [[nodiscard]] const OpenedCollectionScopeLocation& selectedOpenedCollectionScope() const;
    [[nodiscard]] const DisplayedImageLocation& displayedImageLocation() const;
    [[nodiscard]] const OpenedCollectionScopeLocation& displayedOpenedCollectionScope() const;
    [[nodiscard]] const QUrl& displayedUrl() const;
    [[nodiscard]] ImageDocumentStatus status() const;
    [[nodiscard]] bool loading() const;
    [[nodiscard]] const QString& errorString() const;
    [[nodiscard]] const std::optional<ImageLoadFailure>& loadFailure() const;
    [[nodiscard]] QString windowTitleFileName() const;
    [[nodiscard]] const QUrl& containerNavigationUrl() const;
    [[nodiscard]] const QUrl& loadingContainerNavigationUrl() const;
    [[nodiscard]] bool containerNavigationAvailable() const;
    [[nodiscard]] bool unsupportedOpenedCollectionVideo() const;
    [[nodiscard]] const EmbeddedMetadata& embeddedMetadata() const;

    void setSelectedTarget(const ImageDocumentSelectedTarget& target);
    void setDisplayedImageLocation(const DisplayedImageLocation& location);
    void clearDisplayedImageLocation();
    void setStatus(ImageDocumentStatus status);
    void setLoading(bool loading);
    void setErrorString(const QString& errorString);
    void setLoadFailure(ImageLoadFailure failure);
    void setContainerNavigationUrl(const QUrl& containerUrl);
    void setLoadingContainerNavigationUrl(const QUrl& containerUrl);
    void clearLoadingContainerNavigationUrl();
    void setUnsupportedOpenedCollectionVideo(bool unsupported);
    void setEmbeddedMetadata(EmbeddedMetadata metadata);

private:
    void replaceDisplayedImageLocation(const DisplayedImageLocation& location);
    void notify(ImageDocumentChange change);

    std::unique_ptr<ImageDocumentChangeBatcher> m_ownedChanges;
    ImageDocumentChangeBatcher* m_changes = nullptr;
    ImageDocumentSelectedTarget m_selectedTarget;
    DisplayedImageLocation m_displayedImageLocation;
    ImageDocumentStatus m_status = ImageDocumentStatus::Null;
    bool m_loading = false;
    bool m_unsupportedOpenedCollectionVideo = false;
    EmbeddedMetadata m_embeddedMetadata;
    QString m_errorString;
    std::optional<ImageLoadFailure> m_loadFailure;
    QUrl m_containerNavigationUrl;
    QUrl m_loadingContainerNavigationUrl;
};
}

#endif
