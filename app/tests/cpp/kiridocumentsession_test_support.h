// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include "facade/kiridocumentsession.h"

#include "archive/mediaentrysourcebackend.h"
#include "candidate_test_support.h"
#include "facade/kiriimagedocument.h"
#include "facade/kiriimageviewportsurface.h"
#include "facade/kirimediainformation.h"
#include "facade/kirivideodocument.h"
#include "image_async_test_support.h"
#include "image_test_support.h"
#include "location/imagedocumentlocation.h"
#include "metadata/embeddedmetadata.h"
#include "navigation/directmedianavigationmodel.h"
#include "navigation/imagedocumentpagecandidateprovider.h"
#include "rendering/displayimagestore.h"
#include "session/activenavigationthumbnailmodel.h"
#include "session/thumbnailimagestore.h"

#include <QAbstractItemModel>
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QObject>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QSizeF>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>
#include <cstddef>
#include <map>
#include <memory>
#include <utility>

namespace {
QString keyForUrl(const QUrl& url) { return url.adjusted(QUrl::NormalizePathSegments).toString(); }

QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

QVariantMap thumbnailDemand(int number, const QUrl& url, int physicalMaxEdge,
    KiriDocumentSession::ThumbnailDemandPriority priority, quint64 navigationGeneration)
{
    return { { QStringLiteral("number"), number }, { QStringLiteral("url"), url },
        { QStringLiteral("physicalMaxEdge"), physicalMaxEdge },
        { QStringLiteral("priority"), static_cast<int>(priority) },
        { QStringLiteral("navigationGeneration"), navigationGeneration } };
}

kiriview::DirectMediaNavigationCandidate directMediaNavigationCandidate(const QUrl& url)
{
    return kiriview::DirectMediaNavigationCandidate { url, url.fileName(QUrl::PrettyDecoded) };
}

QVariant thumbnailData(const KiriDocumentSession& session, int row, int role)
{
    QAbstractItemModel* model = session.activeNavigationThumbnailModel();
    return model->data(model->index(row, 0), role);
}

int roleForName(const QAbstractItemModel& model, const QByteArray& name)
{
    const QHash<int, QByteArray> roles = model.roleNames();
    for (auto iterator = roles.cbegin(); iterator != roles.cend(); ++iterator) {
        if (iterator.value() == name) {
            return iterator.key();
        }
    }

    return -1;
}

QVariant mediaInformationRowData(
    const QAbstractItemModel& model, int row, const QByteArray& roleName)
{
    const int role = roleForName(model, roleName);
    if (role < 0) {
        return {};
    }

    return model.data(model.index(row, 0), role);
}

QString mediaInformationValueForLabel(const QAbstractItemModel& model, const QString& label)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        if (mediaInformationRowData(model, row, QByteArrayLiteral("label")).toString() == label) {
            return mediaInformationRowData(model, row, QByteArrayLiteral("value")).toString();
        }
    }

    return {};
}

void compareThumbnailRow(const KiriDocumentSession& session, int row, int number, const QUrl& url,
    const QString& label, const QString& iconName, bool current)
{
    QCOMPARE(
        thumbnailData(session, row, kiriview::ActiveNavigationThumbnailModel::NumberRole).toInt(),
        number);
    QCOMPARE(thumbnailData(session, row, kiriview::ActiveNavigationThumbnailModel::UrlRole).toUrl(),
        url);
    QCOMPARE(
        thumbnailData(session, row, kiriview::ActiveNavigationThumbnailModel::LabelRole).toString(),
        label);
    QCOMPARE(thumbnailData(session, row, kiriview::ActiveNavigationThumbnailModel::IconNameRole)
                 .toString(),
        iconName);
    QCOMPARE(
        thumbnailData(session, row, kiriview::ActiveNavigationThumbnailModel::CurrentRole).toBool(),
        current);
}

QVariant thumbnailDataForRoleName(
    const KiriDocumentSession& session, int row, const QByteArray& roleName)
{
    QAbstractItemModel* model = session.activeNavigationThumbnailModel();
    const int role = roleForName(*model, roleName);
    if (role < 0) {
        return {};
    }

    return model->data(model->index(row, 0), role);
}

class FakeDirectMediaNavigationCandidateProvider
{
public:
    void setMedia(
        const QUrl& parentUrl, std::vector<kiriview::DirectMediaNavigationCandidate> candidates)
    {
        m_candidates[keyForUrl(parentUrl)] = std::move(candidates);
    }

    kiriview::DirectMediaNavigationCandidateProvider provider()
    {
        return kiriview::DirectMediaNavigationCandidateProvider {
            [this](QObject*, QUrl parentUrl,
                kiriview::DirectMediaNavigationCandidatesCallback callback,
                kiriview::ErrorCallback errorCallback) {
                const auto candidates = m_candidates.find(keyForUrl(parentUrl));
                if (candidates == m_candidates.cend()) {
                    if (errorCallback) {
                        errorCallback(QStringLiteral("missing direct media candidates"));
                    }
                    return kiriview::ImageIoJob();
                }

                if (callback) {
                    callback(candidates->second);
                }
                return kiriview::ImageIoJob();
            },
        };
    }

private:
    std::map<QString, std::vector<kiriview::DirectMediaNavigationCandidate>> m_candidates;
};

struct ManualDirectMediaNavigationCandidateLoad
{
    QObject* object = nullptr;
    QUrl parentUrl;
    kiriview::DirectMediaNavigationCandidatesCallback callback;
    kiriview::ErrorCallback errorCallback;
    kiriview::ImageIoJobCompletion completion;
    bool canceled = false;
};

class ManualDirectMediaNavigationCandidateProvider
{
public:
    kiriview::DirectMediaNavigationCandidateProvider provider()
    {
        return kiriview::DirectMediaNavigationCandidateProvider {
            [this](QObject* receiver, QUrl parentUrl,
                kiriview::DirectMediaNavigationCandidatesCallback callback,
                kiriview::ErrorCallback errorCallback) {
                auto load = std::make_shared<ManualDirectMediaNavigationCandidateLoad>();
                load->parentUrl = std::move(parentUrl);
                load->callback = std::move(callback);
                load->errorCallback = std::move(errorCallback);

                kiriview::ImageIoJob job
                    = kiriview::TestSupport::Detail::startManualIoJob(receiver, load);
                m_loads.push_back(load);
                return job;
            },
        };
    }

    std::size_t loadCount() const { return m_loads.size(); }

    ManualDirectMediaNavigationCandidateLoad& loadAt(std::size_t index)
    {
        return *m_loads.at(index);
    }

    void finishLoad(
        std::size_t index, std::vector<kiriview::DirectMediaNavigationCandidate> candidates)
    {
        kiriview::TestSupport::Detail::finishManualIoJob(m_loads.at(index),
            [candidates = std::move(candidates)](
                ManualDirectMediaNavigationCandidateLoad& load) mutable {
                if (load.callback) {
                    load.callback(std::move(candidates));
                }
            });
    }

    void deliverIgnoringCancellation(
        std::size_t index, std::vector<kiriview::DirectMediaNavigationCandidate> candidates)
    {
        ManualDirectMediaNavigationCandidateLoad& load = loadAt(index);
        if (load.callback) {
            load.callback(std::move(candidates));
        }
    }

private:
    std::vector<std::shared_ptr<ManualDirectMediaNavigationCandidateLoad>> m_loads;
};

class FakeMediaOpenWithProvider
{
public:
    kiriview::MediaOpenWithProvider provider()
    {
        return [this](QObject*, kiriview::MediaOpenWithRequest request,
                   kiriview::MediaOpenWithCallback callback) {
            requests.push_back(std::move(request));
            if (callback) {
                callback(result, failureFor(requests.back().targetUrl, result, errorString));
            }
            return kiriview::ImageIoJob();
        };
    }

    std::vector<kiriview::MediaOpenWithRequest> requests;
    kiriview::MediaOpenWithResult result = kiriview::MediaOpenWithResult::Succeeded;
    QString errorString;

private:
    static kiriview::KioOperationFailure failureFor(
        const QUrl& targetUrl, kiriview::MediaOpenWithResult result, const QString& errorString)
    {
        kiriview::KioOperationFailure failure;
        failure.operationKind = kiriview::KioOperationKind::MediaOpenWith;
        failure.targetUrl = targetUrl;
        failure.canceled = result == kiriview::MediaOpenWithResult::Canceled;
        failure.userMessage
            = result == kiriview::MediaOpenWithResult::Failed ? errorString : QString();
        failure.diagnosticDetail = errorString;
        failure.retryable = result == kiriview::MediaOpenWithResult::Failed;
        return failure;
    }
};

class FakeOpenedCollectionMediaEntrySource final : public kiriview::MediaEntrySource
{
public:
    explicit FakeOpenedCollectionMediaEntrySource(
        std::vector<kiriview::ImageDocumentPageCandidate> candidates)
        : m_candidates(std::move(candidates))
    {
    }

    kiriview::MediaEntrySourceCandidatesResult loadImageDocumentPageCandidates() override
    {
        return kiriview::MediaEntrySourceCandidates { m_candidates };
    }

    kiriview::MediaEntrySourceImageDataResult loadImageData(const QUrl&) override
    {
        QByteArray data;
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        QImage image(QSize(1, 1), QImage::Format_RGBA8888);
        image.fill(Qt::red);
        image.save(&buffer, "PNG");
        return kiriview::MediaEntrySourceImageData { data };
    }

    kiriview::MediaEntrySourceVideoPlaybackDeviceResult loadVideoPlaybackDevice(
        const QUrl&) override
    {
        auto device = std::make_unique<QBuffer>();
        device->setData(QByteArrayLiteral("video"));
        device->open(QIODevice::ReadOnly);
        return kiriview::MediaEntrySourceVideoPlaybackDevice { {}, std::move(device) };
    }

private:
    std::vector<kiriview::ImageDocumentPageCandidate> m_candidates;
};

kiriview::MediaEntrySourceFactory mediaEntrySourceFactoryForCandidates(
    std::vector<kiriview::ImageDocumentPageCandidate> candidates)
{
    return [candidates = std::move(candidates)](const kiriview::OpenedCollectionScopeLocation&)
               -> kiriview::MediaEntrySourceOpenResult {
        return kiriview::MediaEntrySourcePtr(
            std::make_shared<FakeOpenedCollectionMediaEntrySource>(candidates));
    };
}

struct ManualMediaOpenWithOperation
{
    QObject* object = nullptr;
    kiriview::MediaOpenWithRequest request;
    kiriview::MediaOpenWithCallback callback;
    kiriview::ImageIoJobCompletion completion;
    bool canceled = false;
};

class ManualMediaOpenWithProvider
{
public:
    kiriview::MediaOpenWithProvider provider()
    {
        return [this](QObject* receiver, kiriview::MediaOpenWithRequest request,
                   kiriview::MediaOpenWithCallback callback) {
            auto operation = std::make_shared<ManualMediaOpenWithOperation>();
            operation->request = std::move(request);
            operation->callback = std::move(callback);

            kiriview::ImageIoJob job
                = kiriview::TestSupport::Detail::startManualIoJob(receiver, operation);
            m_operations.push_back(operation);
            return job;
        };
    }

    std::size_t operationCount() const { return m_operations.size(); }

    ManualMediaOpenWithOperation& operationAt(std::size_t index) { return *m_operations.at(index); }

    void finishOperationAt(
        std::size_t index, kiriview::MediaOpenWithResult result, const QString& errorString = {})
    {
        finishOperationAt(index, result,
            failureFor(m_operations.at(index)->request.targetUrl, result, errorString));
    }

    void finishOperationAt(std::size_t index, kiriview::MediaOpenWithResult result,
        kiriview::KioOperationFailure failure)
    {
        kiriview::TestSupport::Detail::finishManualIoJob(m_operations.at(index),
            [result, failure = std::move(failure)](ManualMediaOpenWithOperation& operation) {
                if (operation.callback) {
                    operation.callback(result, failure);
                }
            });
    }

    void deliverOperationAtIgnoringCancellation(
        std::size_t index, kiriview::MediaOpenWithResult result, const QString& errorString = {})
    {
        deliverOperationAtIgnoringCancellation(index, result,
            failureFor(m_operations.at(index)->request.targetUrl, result, errorString));
    }

    void deliverOperationAtIgnoringCancellation(std::size_t index,
        kiriview::MediaOpenWithResult result, const kiriview::KioOperationFailure& failure)
    {
        ManualMediaOpenWithOperation& operation = operationAt(index);
        if (operation.callback) {
            operation.callback(result, failure);
        }
    }

private:
    static kiriview::KioOperationFailure failureFor(
        const QUrl& targetUrl, kiriview::MediaOpenWithResult result, const QString& errorString)
    {
        kiriview::KioOperationFailure failure;
        failure.operationKind = kiriview::KioOperationKind::MediaOpenWith;
        failure.targetUrl = targetUrl;
        failure.canceled = result == kiriview::MediaOpenWithResult::Canceled;
        failure.userMessage
            = result == kiriview::MediaOpenWithResult::Failed ? errorString : QString();
        failure.diagnosticDetail = errorString;
        failure.retryable = result == kiriview::MediaOpenWithResult::Failed;
        return failure;
    }

    std::vector<std::shared_ptr<ManualMediaOpenWithOperation>> m_operations;
};

class FakeThumbnailLookupProvider
{
public:
    kiriview::ThumbnailCacheLookupProvider provider()
    {
        return [this](QObject*, kiriview::ThumbnailCacheLookupRequest request,
                   kiriview::ThumbnailCacheLookupCallback callback) {
            requests.push_back(std::move(request));
            if (callback) {
                callback(result);
            }
            return kiriview::ImageIoJob();
        };
    }

    std::vector<kiriview::ThumbnailCacheLookupRequest> requests;
    kiriview::ThumbnailCacheLookupResult result;
};

class FakeThumbnailGenerationProvider
{
public:
    kiriview::ThumbnailGenerationProvider provider()
    {
        return [this](QObject*, kiriview::ThumbnailGenerationRequest request,
                   kiriview::ThumbnailGenerationCallback callback) {
            requests.push_back(std::move(request));
            if (callback) {
                callback(result);
            }
            return kiriview::ImageIoJob();
        };
    }

    std::vector<kiriview::ThumbnailGenerationRequest> requests;
    kiriview::ThumbnailGenerationResult result;
};

void attachTestViewport(KiriDocumentSession& session)
{
    auto* viewportWindow = new QQuickWindow();
    viewportWindow->QObject::setParent(&session);
    viewportWindow->resize(320, 240);
    auto* viewportSurface = new KiriImageViewportSurface(viewportWindow->contentItem());
    viewportSurface->setSize(QSizeF(320.0, 240.0));
    viewportSurface->setDocument(session.imageDocument());
    viewportWindow->show();
}

std::unique_ptr<KiriDocumentSession> createSessionWithProvider(
    kiriview::DirectMediaNavigationCandidateProvider directMediaNavigationCandidateProvider,
    kiriview::TestSupport::ManualFileDeletionProvider* fileDeletion = nullptr,
    kiriview::TestSupport::ManualImageDataLoader* imageDataLoader = nullptr,
    kiriview::ImageDocumentPageCandidateProvider imageDocumentPageCandidateProvider = {},
    kiriview::ImageDataDecoder imageDataDecoder = kiriview::TestSupport::staticImageDataDecoder(),
    kiriview::MediaOpenWithProvider mediaOpenWithProvider = {},
    kiriview::ThumbnailCacheLookupProvider thumbnailLookupProvider = {},
    kiriview::ThumbnailGenerationProvider thumbnailGenerationProvider = {},
    std::shared_ptr<kiriview::ThumbnailImageStore> thumbnailImageStore = {},
    kiriview::MediaEntrySourceFactory mediaEntrySourceFactory = {},
    kiriview::NavigationSourceEntryFactProvider navigationSourceFacts = {})
{
    kiriview::KiriDocumentSessionDependencies dependencies;
    dependencies.sessionRuntime.directMediaNavigationCandidateProvider
        = std::move(directMediaNavigationCandidateProvider);
    if (navigationSourceFacts) {
        dependencies.sessionRuntime.navigationSourceResolver
            = kiriview::NavigationSourceResolver(std::move(navigationSourceFacts));
    }
    dependencies.sessionRuntime.mediaOpenWithProvider = std::move(mediaOpenWithProvider);
    dependencies.sessionRuntime.activeNavigationThumbnails.lookupProvider
        = std::move(thumbnailLookupProvider);
    dependencies.sessionRuntime.activeNavigationThumbnails.generationProvider
        = std::move(thumbnailGenerationProvider);
    dependencies.sessionRuntime.activeNavigationThumbnails.imageStore
        = std::move(thumbnailImageStore);
    dependencies.imageDocument.candidateProvider = std::move(imageDocumentPageCandidateProvider);
    dependencies.imageDocument.mediaEntrySourceFactory = std::move(mediaEntrySourceFactory);
    if (fileDeletion != nullptr) {
        dependencies.sessionRuntime.fileDeletionProvider
            = kiriview::TestSupport::fileDeletionProviderFor(*fileDeletion);
        dependencies.imageDocument.fileDeletionProvider
            = kiriview::TestSupport::fileDeletionProviderFor(*fileDeletion);
    }
    if (imageDataLoader != nullptr) {
        dependencies.imageDocument.imageDecode = kiriview::TestSupport::imageDecodeDependenciesFor(
            *imageDataLoader, std::move(imageDataDecoder));
    }
    auto session = std::make_unique<KiriDocumentSession>(std::move(dependencies));
    attachTestViewport(*session);
    return session;
}

std::unique_ptr<KiriDocumentSession> createSession(
    FakeDirectMediaNavigationCandidateProvider& directMediaNavigationProvider,
    kiriview::TestSupport::ManualFileDeletionProvider* fileDeletion = nullptr,
    kiriview::TestSupport::ManualImageDataLoader* imageDataLoader = nullptr)
{
    return createSessionWithProvider(
        directMediaNavigationProvider.provider(), fileDeletion, imageDataLoader);
}

void compareUnavailableActiveNavigation(const KiriDocumentSession& session)
{
    QVERIFY(!session.activeNavigationAvailable());
    QVERIFY(!session.activeNavigationKnown());
    QVERIFY(!session.activeNavigationEditable());
    QCOMPARE(session.activeNavigationCurrentNumber(), 0);
    QCOMPARE(session.activeNavigationCount(), 0);
    QVERIFY(!session.canOpenPreviousActiveNavigation());
    QVERIFY(!session.canOpenNextActiveNavigation());
    QVERIFY(!session.atKnownFirstActiveNavigation());
    QVERIFY(!session.atKnownLastActiveNavigation());
}

bool writeTestImage(const QString& path)
{
    QImage image(QSize(2, 2), QImage::Format_RGBA8888);
    image.fill(Qt::red);
    return image.save(path, "PNG");
}

bool writeEmptyFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

kiriview::EmbeddedMetadata testCameraMetadata()
{
    kiriview::EmbeddedMetadata metadata;
    metadata.cameraMake = QStringLiteral("Kiri Camera Co.");
    metadata.cameraModel = QStringLiteral("KiriCam 1");
    metadata.taken = QStringLiteral("2026-05-31 12:34:56");
    metadata.location = QStringLiteral("37.7749, -122.4194");
    metadata.lens = QStringLiteral("Kiri Prime 35mm");
    metadata.exposure = QStringLiteral("1/125 s at f/5.6");
    metadata.iso = QStringLiteral("400");
    metadata.focalLength = QStringLiteral("35 mm");
    metadata.software = QStringLiteral("KiriOS Camera");
    metadata.advancedRows = {
        kiriview::EmbeddedMetadataRow {
            QStringLiteral("Artist"),
            QStringLiteral("Kiri Tester"),
        },
    };
    return metadata;
}

kiriview::ImageDataDecoder staticImageDataDecoderWithMetadata(
    kiriview::EmbeddedMetadata metadata, QImage image = kiriview::TestSupport::testImage(2, 2))
{
    return [metadata = std::move(metadata), image = std::move(image)](
               const QByteArray&, const kiriview::ImageDecodeRequest&) mutable {
        kiriview::StaticDecodedImage decoded = kiriview::TestSupport::staticDecodedTestImage(image);
        decoded.embeddedMetadata = metadata;
        return kiriview::successfulDecodedImageResult(std::move(decoded));
    };
}

bool modelValuesContainCaseInsensitive(const QAbstractItemModel& model, const QString& needle)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        const QString value
            = mediaInformationRowData(model, row, QByteArrayLiteral("value")).toString();
        if (value.contains(needle, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}
}
