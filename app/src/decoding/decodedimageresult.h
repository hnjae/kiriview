// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DECODEDIMAGERESULT_H
#define KIRIVIEW_DECODEDIMAGERESULT_H

#include "decodedimagefailure.h"
#include "decoding/staticimage.h"
#include "imageanimationsourcecatalog.h"
#include "imagedecodeworkspace.h"
#include "metadata/embeddedmetadata.h"

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QtGlobal>
#include <expected>
#include <utility>
#include <variant>

namespace kiriview {
struct StaticDecodedImage
{
    StaticDisplayImagePayload displayImage;
    EmbeddedMetadata embeddedMetadata;
};

struct ApngAnimationImage // NOLINT(cppcoreguidelines-special-member-functions) --
                          // Pass-by-value assignment preserves aggregate initialization and
                          // retires the old image before its workspace hold.
{
    ImageDecodeWorkspaceHold firstFrameWorkspaceHold;
    QImage firstFrame;
    ImageSourceDataLease sourceDataLease;
    ImageDecodeWorkspaceHold inputWorkspaceHold;
    QByteArray data;
    ImageAnimationSourceCatalog catalog;
    EmbeddedMetadata embeddedMetadata;
    QString sourceIdentity;
    ImageSourceRevision sourceRevision;

    ApngAnimationImage& operator=(ApngAnimationImage other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(ApngAnimationImage& left, ApngAnimationImage& right) noexcept
    {
        using std::swap;
        swap(left.firstFrameWorkspaceHold, right.firstFrameWorkspaceHold);
        swap(left.firstFrame, right.firstFrame);
        swap(left.sourceDataLease, right.sourceDataLease);
        swap(left.inputWorkspaceHold, right.inputWorkspaceHold);
        swap(left.data, right.data);
        swap(left.catalog, right.catalog);
        swap(left.embeddedMetadata, right.embeddedMetadata);
        swap(left.sourceIdentity, right.sourceIdentity);
        swap(left.sourceRevision, right.sourceRevision);
    }
};

struct ReaderAnimationImage // NOLINT(cppcoreguidelines-special-member-functions) --
                            // Pass-by-value assignment preserves aggregate initialization and
                            // retires the old image before its workspace hold.
{
    ImageDecodeWorkspaceHold firstFrameWorkspaceHold;
    QImage firstFrame;
    ImageSourceDataLease sourceDataLease;
    ImageDecodeWorkspaceHold inputWorkspaceHold;
    QByteArray data;
    QByteArray format;
    ImageAnimationSourceCatalog catalog;
    EmbeddedMetadata embeddedMetadata;
    QString sourceIdentity;
    ImageSourceRevision sourceRevision;

    ReaderAnimationImage& operator=(ReaderAnimationImage other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(ReaderAnimationImage& left, ReaderAnimationImage& right) noexcept
    {
        using std::swap;
        swap(left.firstFrameWorkspaceHold, right.firstFrameWorkspaceHold);
        swap(left.firstFrame, right.firstFrame);
        swap(left.sourceDataLease, right.sourceDataLease);
        swap(left.inputWorkspaceHold, right.inputWorkspaceHold);
        swap(left.data, right.data);
        swap(left.format, right.format);
        swap(left.catalog, right.catalog);
        swap(left.embeddedMetadata, right.embeddedMetadata);
        swap(left.sourceIdentity, right.sourceIdentity);
        swap(left.sourceRevision, right.sourceRevision);
    }
};

struct WebPAnimationImage // NOLINT(cppcoreguidelines-special-member-functions) --
                          // Pass-by-value assignment preserves aggregate initialization and
                          // retires the old image before its workspace hold.
{
    ImageDecodeWorkspaceHold firstFrameWorkspaceHold;
    QImage firstFrame;
    ImageSourceDataLease sourceDataLease;
    ImageDecodeWorkspaceHold inputWorkspaceHold;
    QByteArray data;
    ImageAnimationSourceCatalog catalog;
    EmbeddedMetadata embeddedMetadata;
    QString sourceIdentity;
    ImageSourceRevision sourceRevision;

    WebPAnimationImage& operator=(WebPAnimationImage other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(WebPAnimationImage& left, WebPAnimationImage& right) noexcept
    {
        using std::swap;
        swap(left.firstFrameWorkspaceHold, right.firstFrameWorkspaceHold);
        swap(left.firstFrame, right.firstFrame);
        swap(left.sourceDataLease, right.sourceDataLease);
        swap(left.inputWorkspaceHold, right.inputWorkspaceHold);
        swap(left.data, right.data);
        swap(left.catalog, right.catalog);
        swap(left.embeddedMetadata, right.embeddedMetadata);
        swap(left.sourceIdentity, right.sourceIdentity);
        swap(left.sourceRevision, right.sourceRevision);
    }
};

struct JxlAnimationImage // NOLINT(cppcoreguidelines-special-member-functions) --
                         // Pass-by-value assignment preserves aggregate initialization and
                         // retires the old image before its workspace hold.
{
    ImageDecodeWorkspaceHold firstFrameWorkspaceHold;
    QImage firstFrame;
    ImageSourceDataLease sourceDataLease;
    ImageDecodeWorkspaceHold inputWorkspaceHold;
    QByteArray data;
    ImageAnimationSourceCatalog catalog;
    EmbeddedMetadata embeddedMetadata;
    QString sourceIdentity;
    ImageSourceRevision sourceRevision;

    JxlAnimationImage& operator=(JxlAnimationImage other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(JxlAnimationImage& left, JxlAnimationImage& right) noexcept
    {
        using std::swap;
        swap(left.firstFrameWorkspaceHold, right.firstFrameWorkspaceHold);
        swap(left.firstFrame, right.firstFrame);
        swap(left.sourceDataLease, right.sourceDataLease);
        swap(left.inputWorkspaceHold, right.inputWorkspaceHold);
        swap(left.data, right.data);
        swap(left.catalog, right.catalog);
        swap(left.embeddedMetadata, right.embeddedMetadata);
        swap(left.sourceIdentity, right.sourceIdentity);
        swap(left.sourceRevision, right.sourceRevision);
    }
};

struct HeifSequenceAnimationImage // NOLINT(cppcoreguidelines-special-member-functions) --
                                  // Pass-by-value assignment preserves aggregate initialization
                                  // and retires the old image before its workspace hold.
{
    ImageDecodeWorkspaceHold firstFrameWorkspaceHold;
    QImage firstFrame;
    ImageSourceDataLease sourceDataLease;
    ImageDecodeWorkspaceHold inputWorkspaceHold;
    QByteArray data;
    ImageAnimationSourceCatalog catalog;
    EmbeddedMetadata embeddedMetadata;
    QString sourceIdentity;
    ImageSourceRevision sourceRevision;

    HeifSequenceAnimationImage& operator=(HeifSequenceAnimationImage other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(HeifSequenceAnimationImage& left, HeifSequenceAnimationImage& right) noexcept
    {
        using std::swap;
        swap(left.firstFrameWorkspaceHold, right.firstFrameWorkspaceHold);
        swap(left.firstFrame, right.firstFrame);
        swap(left.sourceDataLease, right.sourceDataLease);
        swap(left.inputWorkspaceHold, right.inputWorkspaceHold);
        swap(left.data, right.data);
        swap(left.catalog, right.catalog);
        swap(left.embeddedMetadata, right.embeddedMetadata);
        swap(left.sourceIdentity, right.sourceIdentity);
        swap(left.sourceRevision, right.sourceRevision);
    }
};

using DecodedImage = std::variant<StaticDecodedImage, ApngAnimationImage, ReaderAnimationImage,
    WebPAnimationImage, JxlAnimationImage, HeifSequenceAnimationImage>;

using DecodedImageResult = std::expected<DecodedImage, DecodedImageFailure>;

DecodedImageResult failedDecodedImageResult(QString diagnosticDetail);
DecodedImageResult failedDecodedImageResult(DecodedImageFailure failure);
DecodedImageResult successfulDecodedImageResult(DecodedImage image);
template <typename Image> DecodedImageResult successfulDecodedImageResult(Image image)
{
    return successfulDecodedImageResult(DecodedImage { std::move(image) });
}
const DecodedImageFailure* decodedImageResultFailure(const DecodedImageResult& result);
const DecodedImage* decodedImageResultImage(const DecodedImageResult& result);
DecodedImage* decodedImageResultImage(DecodedImageResult& result);
DecodedImageFailure* decodedImageResultFailure(DecodedImageResult& result);
const EmbeddedMetadata& decodedImageEmbeddedMetadata(const DecodedImage& image);
void setDecodedImageEmbeddedMetadata(DecodedImage& image, EmbeddedMetadata metadata);
template <typename Image> const Image* decodedImageResultImageAs(const DecodedImageResult& result)
{
    const DecodedImage* image = decodedImageResultImage(result);
    return image == nullptr ? nullptr : std::get_if<Image>(image);
}
}

#endif
