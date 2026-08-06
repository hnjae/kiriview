// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDECODEJOB_H
#define KIRIVIEW_IMAGEDECODEJOB_H

#include "async/imageiojob.h"
#include "decodedimageresult.h"
#include "imagedecodedependencies.h"
#include "imagedecodejobstate.h"
#include "imagedecoderequest.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <functional>
#include <optional>

namespace kiriview {
class ImageDecodeJob final : public QObject
{
    Q_OBJECT
public:
    using DecodedCallback = std::function<void(ImageDecodeRequest, DecodedImageResult)>;
    using LoadErrorCallback = std::function<void(const ImageDecodeRequest&, ImageDataLoadError)>;
    using ThumbnailPreviewCallback
        = std::function<void(const ImageDecodeRequest&, StaticDisplayImagePayload)>;

    struct Callbacks
    {
        DecodedCallback decoded;
        LoadErrorCallback loadError;
        ThumbnailPreviewCallback thumbnailPreview;
    };

    explicit ImageDecodeJob(QObject* parent = nullptr);
    ImageDecodeJob(QObject* parent, Callbacks callbacks);
    ImageDecodeJob(QObject* parent, ImageDecodeDependencies dependencies);
    ImageDecodeJob(QObject* parent, ImageDecodeDependencies dependencies, Callbacks callbacks);

    void start(ImageDecodeRequest request,
        std::optional<StaticDisplayImagePayload> authoritativeSeed = std::nullopt);
    void cancel();
    [[nodiscard]] bool hasActiveRequest() const;

private:
    void startThumbnailPreviewLookup(const QByteArray& data, ImageSourceDataLease sourceDataLease,
        ImageDecodeJobTicket ticket, const ImageDecodeRequest& request);
    void startRawEmbeddedThumbnailPreviewValidation(
        ImageSourceData sourceData, ImageDecodeJobTicket ticket, const ImageDecodeRequest& request);
    void startDecode(
        ImageSourceData sourceData, ImageDecodeJobTicket ticket, ImageDecodeRequest request);

    ImageDecodeDependencies m_dependencies;
    Callbacks m_callbacks;
    ImageIoJob m_dataLoadJob;
    ImageIoJob m_thumbnailPreviewLookupJob;
    ImageWorkerTask m_decodeWorkerTask;
    ImageWorkerTask m_rawThumbnailPreviewWorkerTask;
    ImageDecodeJobState m_state;
    std::optional<StaticDisplayImagePayload> m_authoritativeSeed;
};
}

#endif
