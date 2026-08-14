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
#include <memory>
#include <optional>
#include <vector>

namespace kiriview {
struct XdgThumbnailPreviewRequest;

class ImageDecodeJob final : public QObject
{
    Q_OBJECT
public:
    using DecodedCallback = std::function<void(ImageDecodeRequest, DecodedImageResult)>;
    using LoadErrorCallback = std::function<void(const ImageDecodeRequest&, ImageDataLoadError)>;
    using ThumbnailPreviewCallback
        = std::function<void(const ImageDecodeRequest&, StaticDisplayImagePayload)>;
    using RetiredCallback = std::function<void(const ImageDecodeRequest&)>;

    struct Callbacks
    {
        DecodedCallback decoded;
        LoadErrorCallback loadError;
        ThumbnailPreviewCallback thumbnailPreview;
        RetiredCallback retired;
    };

    explicit ImageDecodeJob(QObject* parent = nullptr);
    ImageDecodeJob(QObject* parent, Callbacks callbacks);
    ImageDecodeJob(QObject* parent, ImageDecodeDependencies dependencies);
    ImageDecodeJob(QObject* parent, ImageDecodeDependencies dependencies, Callbacks callbacks);
    ~ImageDecodeJob() override;
    Q_DISABLE_COPY_MOVE(ImageDecodeJob)

    void start(ImageDecodeRequest request,
        std::optional<StaticDisplayImagePayload> authoritativeSeed = std::nullopt,
        ImageDecodeWorkspacePriority priority = ImageDecodeWorkspacePriority::Interactive);
    void cancel();
    [[nodiscard]] bool hasActiveRequest() const;

private:
    void startThumbnailPreviewLookup(
        ImageSourceData sourceData, ImageDecodeJobTicket ticket, const ImageDecodeRequest& request);
    void startThumbnailPreviewPlanning(ImageSourceData sourceData,
        ImageDecodeWorkspaceLease workspaceLease, ImageDecodeJobTicket ticket,
        const ImageDecodeRequest& request);
    void startThumbnailPreviewCacheLookup(XdgThumbnailPreviewRequest previewRequest,
        ImageSourceData sourceData, ImageDecodeJobTicket ticket, const ImageDecodeRequest& request);
    void requestRawEmbeddedThumbnailPreviewAdmission(
        ImageSourceData sourceData, ImageDecodeJobTicket ticket, const ImageDecodeRequest& request);
    void startRawEmbeddedThumbnailPreviewValidation(ImageSourceData sourceData,
        ImageDecodeWorkspaceLease workspaceLease, ImageDecodeJobTicket ticket,
        const ImageDecodeRequest& request);
    void startDecode(
        ImageSourceData sourceData, ImageDecodeJobTicket ticket, ImageDecodeRequest request);
    void processPreparedResult(
        PreparedImageDecodeResult result, ImageDecodeJobTicket ticket, ImageDecodeRequest request);
    void requestPreparedAdmission(std::unique_ptr<PreparedImageDecodeWork> prepared,
        ImageDecodeJobTicket ticket, ImageDecodeRequest request);
    void startPreparedExecution(std::unique_ptr<PreparedImageDecodeWork> prepared,
        ImageDecodeWorkspaceLease lease, ImageDecodeJobTicket ticket, ImageDecodeRequest request);
    void deliverDecodeResult(
        DecodedImageResult result, const ImageDecodeJobTicket& ticket, ImageDecodeRequest request);
    ImageDecodeDependencies m_dependencies;
    Callbacks m_callbacks;
    std::vector<ImageIoJob> m_ioJobs;
    std::vector<ImageWorkerTask> m_workerTasks;
    ImageDecodeWorkspaceAdmission m_workspaceAdmission;
    ImageDecodeWorkspaceAdmission m_previewWorkspaceAdmission;
    ImageDecodeJobState m_state;
    std::optional<StaticDisplayImagePayload> m_authoritativeSeed;
    std::shared_ptr<class ImageDecodeJobRun> m_run;
};
}

#endif
