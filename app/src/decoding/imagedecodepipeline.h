// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDECODEPIPELINE_H
#define KIRIVIEW_IMAGEDECODEPIPELINE_H

#include "decodedimageresult.h"
#include "imagedecoderequest.h"
#include "imagedecodeworkspace.h"
#include "imageinputclassification.h"
#include "imagesourcedata.h"

#include <QByteArray>
#include <QtGlobal>
#include <functional>
#include <memory>
#include <optional>
#include <variant>

namespace kiriview {
class PreparedImageDecodeWork;

using PreparedImageDecodeResult
    = std::variant<DecodedImageResult, std::unique_ptr<PreparedImageDecodeWork>>;
using PreparedImageDecodeExecutor
    = std::move_only_function<PreparedImageDecodeResult(ImageDecodeWorkspaceLease)>;

class PreparedImageDecodeWork final
{
public:
    PreparedImageDecodeWork(ImageDecodeWorkspaceAdmissionRequest admissionRequest,
        DecodedImageFailure hardLimitFailure, PreparedImageDecodeExecutor execute);
    ~PreparedImageDecodeWork();
    PreparedImageDecodeWork(PreparedImageDecodeWork&& other) noexcept;
    PreparedImageDecodeWork& operator=(PreparedImageDecodeWork&& other) noexcept;
    Q_DISABLE_COPY(PreparedImageDecodeWork)

    [[nodiscard]] const ImageDecodeWorkspaceAdmissionRequest& admissionRequest() const;
    [[nodiscard]] const DecodedImageFailure& hardLimitFailure() const;
    PreparedImageDecodeResult execute(ImageDecodeWorkspaceLease lease) &&;

private:
    class Private;
    std::unique_ptr<Private> d;
};

using ImageDataDecodePlanner = std::function<PreparedImageDecodeResult(ImageSourceData sourceData,
    const ImageDecodeRequest& request, ImageDecodeWorkspacePriority priority)>;

struct ImageDecodeRouterInput
{
    const QByteArray& data;
    const ImageDecodeRequest& request;
    QtRasterFormat qtRasterFormat = QtRasterFormat::None;
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget;
    qsizetype retainedInputWorkspaceByteCount = 0;
};

enum class ImageDecodeHandlerKind {
    None,
    Svg,
    Apng,
    HeifFamily,
    Raw,
    QtRaster,
};

struct ImageDecodeRoute
{
    ImageDecodeHandlerKind handlerKind = ImageDecodeHandlerKind::None;
    ImageDecodeDataSource dataSource = ImageDecodeDataSource::Original;
    QtRasterFormat qtRasterFormat = QtRasterFormat::None;

    [[nodiscard]] bool shouldDecode() const { return handlerKind != ImageDecodeHandlerKind::None; }
};

using ImageDecodeRouterHandler = std::function<DecodedImageResult(const ImageDecodeRouterInput&)>;

struct ImageDecodeRouterHandlers
{
    ImageDecodeRouterHandler svg;
    ImageDecodeRouterHandler apng;
    ImageDecodeRouterHandler heifFamily;
    ImageDecodeRouterHandler raw;
    ImageDecodeRouterHandler qtRaster;
};

using ImageDecodeInputClassifier
    = std::function<ImageInputClassification(const QByteArray&, const QString&)>;
struct ImageDecodeCompatibleDataTransform
{
    enum class Storage {
        Original,
        OwnedReplacement,
    };

    struct Result
    {
        QByteArray data;
        Storage storage = Storage::Original;
    };

    // The plan must bound both peak transform allocation and retained replacement capacity.
    std::function<std::optional<qsizetype>(qsizetype)> workspaceByteCost;
    std::function<Result(const QByteArray&)> apply;

    [[nodiscard]] explicit operator bool() const { return static_cast<bool>(apply); }
};

ImageDecodeRoute imageDecodeRouteForClassification(ImageInputClassification classification);

class ImageDecodeRouterRuntime final
{
public:
    explicit ImageDecodeRouterRuntime(ImageDecodeRouterHandlers handlers = {},
        ImageDecodeCompatibleDataTransform compatibleDataTransform = {});

    [[nodiscard]] DecodedImageResult execute(ImageDecodeRoute route, const QByteArray& data,
        const ImageDecodeRequest& request,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {}) const;
    [[nodiscard]] DecodedImageResult executeRoutedData(ImageDecodeRoute route,
        const QByteArray& data, const ImageDecodeRequest& request,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        qsizetype retainedInputWorkspaceByteCount = 0) const;
    [[nodiscard]] bool hasInjectedHandler(ImageDecodeHandlerKind handlerKind) const;
    [[nodiscard]] const ImageDecodeCompatibleDataTransform& compatibleDataTransform() const;

private:
    bool m_hasInjectedSvgHandler = false;
    bool m_hasInjectedApngHandler = false;
    bool m_hasInjectedHeifFamilyHandler = false;
    bool m_hasInjectedRawHandler = false;
    bool m_hasInjectedQtRasterHandler = false;
    ImageDecodeRouterHandlers m_handlers;
    ImageDecodeCompatibleDataTransform m_compatibleDataTransform;
};

class ImageDecodeRouter final
{
public:
    explicit ImageDecodeRouter(ImageDecodeRouterHandlers handlers = {},
        ImageDecodeInputClassifier classifier = {},
        ImageDecodeCompatibleDataTransform compatibleDataTransform = {});

    [[nodiscard]] DecodedImageResult decode(const QByteArray& data,
        const ImageDecodeRequest& request,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {}) const;
    [[nodiscard]] PreparedImageDecodeResult prepare(ImageSourceData sourceData,
        const ImageDecodeRequest& request, ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {}) const;

private:
    ImageDecodeInputClassifier m_classifier;
    ImageDecodeRouterRuntime m_runtime;
};

DecodedImageResult decodeImageDataWithDefaultRouter(const QByteArray& data,
    const ImageDecodeRequest& request,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {});
ImageDataDecodePlanner defaultImageDataDecodePlanner(
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {});
}

#endif
