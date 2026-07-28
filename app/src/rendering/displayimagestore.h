// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DISPLAYIMAGESTORE_H
#define KIRIVIEW_DISPLAYIMAGESTORE_H

#include "decoding/imagesourcerevision.h"
#include "rendering/displayimagequality.h"

#include <QImage>
#include <QImageIOHandler>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <memory>
#include <optional>

namespace kiriview {
enum class DisplayedPageRole {
    Primary,
    Secondary,
};

enum class DisplayImageRetentionPriority {
    Nearby,
    Background,
    Visible,
};

struct DisplayImageRasterIdentity
{
    DisplayImageRasterKind kind = DisplayImageRasterKind::AuthoritativeStill;
    int authoredFrame = -1;

    static DisplayImageRasterIdentity provisionalPreview();
    static DisplayImageRasterIdentity authoritativeStill();
    static DisplayImageRasterIdentity timedFrame(int authoredFrame);
    static DisplayImageRasterIdentity refinement();

    [[nodiscard]] bool isValid() const;
};

struct DisplayImageReuseKey
{
    QString locationIdentity;
    QString sourceIdentity;
    ImageSourceRevision sourceRevision;
    DisplayImageRasterIdentity rasterIdentity;
    QImageIOHandler::Transformations imageReaderTransformations
        = QImageIOHandler::TransformationNone;
    QSize originalSize;
    QSize rasterSize;
    DisplayImageQuality quality = DisplayImageQuality::Exact;
    DisplayImagePreviewOrigin previewOrigin = DisplayImagePreviewOrigin::None;
    DisplayedPageRole pageRole = DisplayedPageRole::Primary;
};

struct DisplayImageEntry
{
    QImage image;
    QSize originalSize;
    QSize rasterSize;
    DisplayImageQuality quality = DisplayImageQuality::Exact;
    DisplayImageRetentionPriority priority = DisplayImageRetentionPriority::Nearby;
};

struct DisplayImageStoreEntry
{
    QImage image;
    QSize originalSize;
    QSize rasterSize;
    QImageIOHandler::Transformations imageReaderTransformations
        = QImageIOHandler::TransformationNone;
    DisplayImageQuality quality = DisplayImageQuality::Exact;
    qsizetype byteCost = 0;
};

class DisplayImageStore final
{
public:
    explicit DisplayImageStore(qsizetype byteBudget);
    ~DisplayImageStore();
    Q_DISABLE_COPY_MOVE(DisplayImageStore)

    QString acquireReusable(DisplayImageEntry entry, DisplayImageReuseKey reuseKey);
    [[nodiscard]] std::optional<DisplayImageStoreEntry> entry(const QString& id) const;
    bool acquireFrameLease(const QString& id);
    void releaseFrameLease(const QString& id);
    [[nodiscard]] qsizetype byteBudget() const;
    [[nodiscard]] qsizetype byteCost() const;
    [[nodiscard]] qsizetype size() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};
}

#endif
