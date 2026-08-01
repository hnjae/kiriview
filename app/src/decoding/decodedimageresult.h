// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DECODEDIMAGERESULT_H
#define KIRIVIEW_DECODEDIMAGERESULT_H

#include "decodedimagefailure.h"
#include "imageanimationsourcecatalog.h"
#include "metadata/embeddedmetadata.h"
#include "rendering/staticimage.h"

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

struct ApngAnimationImage
{
    QImage firstFrame;
    QByteArray data;
    ImageAnimationSourceCatalog catalog;
    EmbeddedMetadata embeddedMetadata;
    QString sourceIdentity;
    ImageSourceRevision sourceRevision;
    ImageSourceDataLease sourceDataLease;
};

struct ReaderAnimationImage
{
    QImage firstFrame;
    QByteArray data;
    QByteArray format;
    ImageAnimationSourceCatalog catalog;
    EmbeddedMetadata embeddedMetadata;
    QString sourceIdentity;
    ImageSourceRevision sourceRevision;
    ImageSourceDataLease sourceDataLease;
};

struct WebPAnimationImage
{
    QImage firstFrame;
    QByteArray data;
    ImageAnimationSourceCatalog catalog;
    EmbeddedMetadata embeddedMetadata;
    QString sourceIdentity;
    ImageSourceRevision sourceRevision;
    ImageSourceDataLease sourceDataLease;
};

struct JxlAnimationImage
{
    QImage firstFrame;
    QByteArray data;
    ImageAnimationSourceCatalog catalog;
    EmbeddedMetadata embeddedMetadata;
    QString sourceIdentity;
    ImageSourceRevision sourceRevision;
    ImageSourceDataLease sourceDataLease;
};

struct HeifSequenceAnimationImage
{
    QImage firstFrame;
    QByteArray data;
    ImageAnimationSourceCatalog catalog;
    EmbeddedMetadata embeddedMetadata;
    QString sourceIdentity;
    ImageSourceRevision sourceRevision;
    ImageSourceDataLease sourceDataLease;
};

using DecodedImage = std::variant<StaticDecodedImage, ApngAnimationImage, ReaderAnimationImage,
    WebPAnimationImage, JxlAnimationImage, HeifSequenceAnimationImage>;

using DecodedImageResult = std::expected<DecodedImage, DecodedImageFailure>;

DecodedImageResult failedDecodedImageResult(QString errorString);
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
