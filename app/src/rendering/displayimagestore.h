// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DISPLAYIMAGESTORE_H
#define KIRIVIEW_DISPLAYIMAGESTORE_H

#include "decoding/displayimagequality.h"
#include "decoding/imagesourcerevision.h"

#include <QImage>
#include <QImageIOHandler>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <functional>
#include <memory>
#include <optional>

namespace kiriview {
class DisplayImageStore;

class DisplayImageOutputAdmission final
{
public:
    ~DisplayImageOutputAdmission();
    Q_DISABLE_COPY_MOVE(DisplayImageOutputAdmission)

    [[nodiscard]] qsizetype byteCost() const;
    [[nodiscard]] bool retainOnly(qsizetype retainedByteCost);

private:
    friend class DisplayImageStore;
    class Private;

    explicit DisplayImageOutputAdmission(std::unique_ptr<Private> data);

    std::unique_ptr<Private> d;
};

enum class DisplayedPageRole {
    Primary,
    Secondary,
};

enum class DisplayImageRetentionPriority {
    Nearby,
    Background,
    Visible,
};

enum class DisplayImageOutputReservationOrigin {
    Unspecified,
    InitialStaticOutput,
    StaticRefinementOutput,
    AnimationOutput,
    ProviderPreparationOutput,
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
    using OutputPressureReclaimer = std::function<void()>;

    explicit DisplayImageStore(qsizetype byteBudget);
    ~DisplayImageStore();
    Q_DISABLE_COPY_MOVE(DisplayImageStore)

    QString acquireReusable(DisplayImageEntry entry, DisplayImageReuseKey reuseKey,
        std::shared_ptr<DisplayImageOutputAdmission> outputAdmission = {});
    [[nodiscard]] std::shared_ptr<DisplayImageOutputAdmission> reserveOutput(qsizetype byteCost,
        DisplayImageOutputReservationOrigin origin
        = DisplayImageOutputReservationOrigin::Unspecified);
    [[nodiscard]] qsizetype availableOutputBytesForRequest(qsizetype requestedByteCost,
        qsizetype minimumRequiredByteCost, DisplayImageOutputReservationOrigin origin);
    void setOutputPressureReclaimer(OutputPressureReclaimer reclaimer);
    [[nodiscard]] std::shared_ptr<DisplayImageOutputAdmission> outputAdmissionForImage(
        const QImage& image) const;
    [[nodiscard]] qsizetype availableOutputBytes() const;
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
