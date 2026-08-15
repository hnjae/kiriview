// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_TESTS_IMAGE_TEST_SUPPORT_H
#define KIRIVIEW_TESTS_IMAGE_TEST_SUPPORT_H

#include "candidate_test_support.h"
#include "decoding/staticimage.h"
#include "document/imagedocumentruntimedependencies.h"
#include "image_async_test_support.h"
#include "location/sourcekey.h"

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace kiriview {
using ImageDataDecoder
    = std::function<DecodedImageResult(const QByteArray&, const ImageDecodeRequest&)>;
}

namespace kiriview::TestSupport {
namespace Detail {
    inline void appendFourCc(QByteArray& data, std::string_view fourCc)
    {
        Q_ASSERT(fourCc.size() == 4);

        if (fourCc.size() >= 4) {
            data.append(fourCc.data(), 4);
            return;
        }

        data.append(fourCc.data(), static_cast<qsizetype>(fourCc.size()));
        data.append(4 - static_cast<qsizetype>(fourCc.size()), '\0');
    }
}

inline QByteArray heifFtypBox(
    std::string_view majorBrand, std::initializer_list<std::string_view> compatibleBrands)
{
    const quint32 boxSize = 16 + static_cast<quint32>(compatibleBrands.size() * 4);
    QByteArray data;
    data.append(static_cast<char>((boxSize >> 24) & 0xff));
    data.append(static_cast<char>((boxSize >> 16) & 0xff));
    data.append(static_cast<char>((boxSize >> 8) & 0xff));
    data.append(static_cast<char>(boxSize & 0xff));
    Detail::appendFourCc(data, "ftyp");
    Detail::appendFourCc(data, majorBrand);
    data.append(4, '\0');
    for (std::string_view brand : compatibleBrands) {
        Detail::appendFourCc(data, brand);
    }
    return data;
}

inline QImage testImage(const QSize& size = QSize(1, 1))
{
    QImage image(size, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    return image;
}

inline QImage testImage(int width, int height = 1) { return testImage(QSize(width, height)); }

class TestStaticImageDisplaySource final : public StaticImageDisplaySource
{
public:
    explicit TestStaticImageDisplaySource(QImage image, StaticImageReaderTransform transform = {})
        : m_image(std::move(image))
        , m_transform(transform)
    {
    }

    QSize imageSize() const override { return m_image.size(); }
    qsizetype byteCost() const override { return m_image.sizeInBytes(); }
    StaticImageReaderTransform imageReaderTransform() const override { return m_transform; }

    StaticImageDisplayDecodeResult decodeBlockingDisplayImage(int) const override
    {
        return { m_image, {} };
    }

private:
    QImage m_image;
    StaticImageReaderTransform m_transform;
};

inline StaticDisplayImagePayload staticDisplayTestImagePayload(const QImage& sourceImage,
    const QImage& displayImage, DisplayImageQuality quality = DisplayImageQuality::Exact,
    StaticImageReaderTransform transform = {})
{
    return StaticDisplayImagePayload {
        QStringLiteral("test-image"),
        transform,
        sourceImage.size(),
        displayImage,
        quality,
        {},
        {},
        {},
        std::make_shared<TestStaticImageDisplaySource>(sourceImage, transform),
        DisplayImagePreviewOrigin::None,
        StaticImageSourceDetailModel::FiniteRaster,
        ImageSourceRevision::fromData(QByteArrayView("image-test-support")),
        DisplayImageRasterKind::AuthoritativeStill,
    };
}

inline StaticDisplayImagePayload staticDisplayTestImagePayload(
    const QImage& image = testImage(), DisplayImageQuality quality = DisplayImageQuality::Exact)
{
    return staticDisplayTestImagePayload(image, image, quality);
}

inline StaticDecodedImage staticDecodedTestImage(const QImage& image = testImage())
{
    return StaticDecodedImage { staticDisplayTestImagePayload(image) };
}

inline QString testImageDecodeFailureString() { return QStringLiteral("decode failed"); }

inline DecodedImageResult failedTestImageDecodeResult()
{
    return failedDecodedImageResult(testImageDecodeFailureString());
}

inline DecodedImageFailure testImageDecodeFailure(DecodedImageFailureRoute route,
    DecodedImageFailureOperation operation, QString diagnosticDetail, bool retryable)
{
    return DecodedImageFailure {
        testImageDecodeFailureString(),
        route,
        operation,
        std::move(diagnosticDetail),
        DecodedImageFailureSeverity::Error,
        retryable,
    };
}

inline ImageDataDecoder imageDataDecoderReturningFailure(DecodedImageFailure failure)
{
    return [failure = std::move(failure)](const QByteArray&, const ImageDecodeRequest&) {
        return failedDecodedImageResult(failure);
    };
}

inline ImageDataDecoder staticImageDataDecoder(QImage image = testImage())
{
    return [image = std::move(image)](const QByteArray&, const ImageDecodeRequest& request) {
        StaticDecodedImage decoded = staticDecodedTestImage(image);
        decoded.displayImage.sourceIdentity = sourceKeyForUrl(request.imageUrl()).identity;
        if (request.sourceRevision().isValid()) {
            decoded.displayImage.sourceRevision = request.sourceRevision();
        }
        return successfulDecodedImageResult(std::move(decoded));
    };
}

inline ImageDataDecoder staticImageDataDecoderRejectingBadData(QImage image = testImage())
{
    return [decoder = staticImageDataDecoder(std::move(image))](
               const QByteArray& data, const ImageDecodeRequest& request) {
        if (data == QByteArrayLiteral("bad")) {
            return failedTestImageDecodeResult();
        }

        return decoder(data, request);
    };
}

inline void retainDecodedSourceDataLease(DecodedImageResult& result, ImageSourceDataLease lease)
{
    DecodedImage* image = decodedImageResultImage(result);
    if (image == nullptr) {
        return;
    }
    std::visit(
        [lease = std::move(lease)](auto& decoded) mutable {
            using Image = std::decay_t<decltype(decoded)>;
            if constexpr (std::is_same_v<Image, StaticDecodedImage>) {
                decoded.displayImage.sourceDataLease = std::move(lease);
            } else {
                decoded.sourceDataLease = std::move(lease);
            }
        },
        *image);
}

inline ImageDataDecodePlanner imageDataDecodePlanner(ImageDataDecoder decoder)
{
    return [decoder = std::move(decoder)](ImageSourceData sourceData,
               const ImageDecodeRequest& request,
               ImageDecodeWorkspacePriority priority) -> PreparedImageDecodeResult {
        auto execute = [decoder, sourceData = std::move(sourceData), request](
                           ImageDecodeWorkspaceLease) mutable -> PreparedImageDecodeResult {
            DecodedImageResult result = decoder(sourceData.data, request);
            retainDecodedSourceDataLease(result, std::move(sourceData.lease));
            return result;
        };
        return std::make_unique<PreparedImageDecodeWork>(
            ImageDecodeWorkspaceAdmissionRequest { 0, 0, priority },
            DecodedImageFailure {
                testImageDecodeFailureString(),
                DecodedImageFailureRoute::Unknown,
                DecodedImageFailureOperation::Unknown,
                imageDecodeWorkspaceResourceLimitDiagnostic(),
                DecodedImageFailureSeverity::Error,
                false,
                DecodedImageFailureCause::ResourceLimitExceeded,
            },
            std::move(execute));
    };
}

inline ImageDecodeDependencies imageDecodeDependenciesFor(
    ManualImageDataLoader& dataLoader, ImageDataDecoder dataDecoder)
{
    ImageDecodeDependencies dependencies;
    dependencies.dataLoader = dataLoaderFor(dataLoader);
    dependencies.dataPlanner = imageDataDecodePlanner(std::move(dataDecoder));
    return dependencies;
}

inline ImageDocumentRuntimeDependencyOverrides imageDocumentRuntimeDependencyOverridesFor(
    FakeImageDocumentPageCandidateProvider& candidateProvider, ManualImageDataLoader& dataLoader,
    ImageDataDecoder dataDecoder, FileDeletionProvider fileDeletionProvider = {})
{
    return ImageDocumentRuntimeDependencyOverrides {
        candidateProvider.provider(),
        imageDecodeDependenciesFor(dataLoader, std::move(dataDecoder)),
        std::move(fileDeletionProvider),
    };
}

}

#endif
