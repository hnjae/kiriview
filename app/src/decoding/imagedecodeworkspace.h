// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDECODEWORKSPACE_H
#define KIRIVIEW_IMAGEDECODEWORKSPACE_H

#include "system/systemmemory.h"

#include <QSize>
#include <QString>
#include <QtGlobal>
#include <expected>
#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace kiriview {
namespace ImageDecodeWorkspaceDetail {
    struct AdmissionState;
    struct BudgetState;
    struct LeaseState;
}

struct ImageDecodeWorkspaceBudgetRequest
{
    qsizetype aggregateByteLimit = 0;
    qsizetype perOperationByteLimit = 0;
};

struct ImageDecodeWorkspaceBudgetLimits
{
    qsizetype aggregateByteLimit = 0;
    qsizetype perOperationByteLimit = 0;
};

enum class ImageDecodeWorkspacePriority : quint8 {
    Interactive,
    Demanded,
    Speculative,
};

struct ImageDecodeWorkspaceAdmissionRequest
{
    qsizetype additionalPeakByteCount = 0;
    qsizetype perOperationBaselineByteCount = 0;
    ImageDecodeWorkspacePriority priority = ImageDecodeWorkspacePriority::Interactive;
};

enum class ImageDecodeWorkspaceAdmissionFailure {
    InvalidRequest,
    PerOperationLimitExceeded,
    AggregateLimitExceeded,
};

class ImageDecodeWorkspaceLease;
class ImageDecodeWorkspaceBudget;

namespace ImageDecodeWorkspaceDetail {
    [[nodiscard]] ImageDecodeWorkspaceLease startLease(const ImageDecodeWorkspaceBudget& budget);
    [[nodiscard]] ImageDecodeWorkspaceLease startLeaseForOperation(
        const ImageDecodeWorkspaceBudget& budget, qsizetype alreadyReservedByteCount);
    [[nodiscard]] bool tryReserve(ImageDecodeWorkspaceLease& lease, qsizetype additionalByteCount);
}

class ImageDecodeWorkspaceHold final
{
public:
    ImageDecodeWorkspaceHold() = default;

    [[nodiscard]] qsizetype reservedByteCount() const;
    [[nodiscard]] bool isManaged() const;

private:
    explicit ImageDecodeWorkspaceHold(
        std::shared_ptr<ImageDecodeWorkspaceDetail::LeaseState> state);

    std::shared_ptr<ImageDecodeWorkspaceDetail::LeaseState> m_state;
    friend class ImageDecodeWorkspaceLease;
};

class ImageDecodeWorkspaceLease final
{
public:
    ImageDecodeWorkspaceLease();
    ~ImageDecodeWorkspaceLease();
    ImageDecodeWorkspaceLease(ImageDecodeWorkspaceLease&& other) noexcept;
    ImageDecodeWorkspaceLease& operator=(ImageDecodeWorkspaceLease&& other) noexcept;
    Q_DISABLE_COPY(ImageDecodeWorkspaceLease)

    [[nodiscard]] bool tryReserve(qsizetype additionalByteCount);
    [[nodiscard]] bool release(qsizetype byteCount);
    [[nodiscard]] ImageDecodeWorkspaceHold sharedHold() const;
    [[nodiscard]] ImageDecodeWorkspaceHold splitRetained(qsizetype retainedByteCount);
    [[nodiscard]] ImageDecodeWorkspaceHold retainOnly(qsizetype retainedByteCount);
    [[nodiscard]] qsizetype reservedByteCount() const;
    [[nodiscard]] bool isManaged() const;

private:
    explicit ImageDecodeWorkspaceLease(
        std::shared_ptr<ImageDecodeWorkspaceDetail::LeaseState> state);

    std::shared_ptr<ImageDecodeWorkspaceDetail::LeaseState> m_state;
    friend class ImageDecodeWorkspaceBudget;
    friend struct ImageDecodeWorkspaceDetail::AdmissionState;
    friend bool ImageDecodeWorkspaceDetail::tryReserve(ImageDecodeWorkspaceLease&, qsizetype);
};

namespace ImageDecodeWorkspaceDetail {
    [[nodiscard]] std::optional<ImageDecodeWorkspaceLease> tryBestEffortAdmission(
        const ImageDecodeWorkspaceBudget& budget, ImageDecodeWorkspaceAdmissionRequest request);
}

class ImageDecodeWorkspaceAdmission final
{
public:
    ImageDecodeWorkspaceAdmission();
    ~ImageDecodeWorkspaceAdmission();
    ImageDecodeWorkspaceAdmission(ImageDecodeWorkspaceAdmission&& other) noexcept;
    ImageDecodeWorkspaceAdmission& operator=(ImageDecodeWorkspaceAdmission&& other) noexcept;
    Q_DISABLE_COPY(ImageDecodeWorkspaceAdmission)

    void cancel();
    [[nodiscard]] bool isPending() const;

private:
    explicit ImageDecodeWorkspaceAdmission(
        std::shared_ptr<ImageDecodeWorkspaceDetail::AdmissionState> state);

    std::shared_ptr<ImageDecodeWorkspaceDetail::AdmissionState> m_state;
    friend class ImageDecodeWorkspaceBudget;
};

using ImageDecodeWorkspaceGranted = std::move_only_function<void(ImageDecodeWorkspaceLease)>;

class ImageDecodeWorkspaceBudget final
{
public:
    ImageDecodeWorkspaceBudget(qsizetype aggregateByteLimit, qsizetype perOperationByteLimit);

    [[nodiscard]] ImageDecodeWorkspaceLease startLease() const;
    [[nodiscard]] ImageDecodeWorkspaceLease startLeaseForOperation(
        qsizetype alreadyReservedByteCount) const;
    [[nodiscard]] std::expected<ImageDecodeWorkspaceAdmission, ImageDecodeWorkspaceAdmissionFailure>
    requestAdmission(QObject* receiver, ImageDecodeWorkspaceAdmissionRequest request,
        ImageDecodeWorkspaceGranted granted) const;
    void finalizePrechargedAdmission() const;
    [[nodiscard]] qsizetype aggregateByteLimit() const;
    [[nodiscard]] qsizetype perOperationByteLimit() const;
    [[nodiscard]] qsizetype reservedByteCount() const;

private:
    [[nodiscard]] std::optional<ImageDecodeWorkspaceLease> tryBestEffortAdmission(
        ImageDecodeWorkspaceAdmissionRequest request) const;
    explicit ImageDecodeWorkspaceBudget(
        std::shared_ptr<ImageDecodeWorkspaceDetail::BudgetState> state);

    std::shared_ptr<ImageDecodeWorkspaceDetail::BudgetState> m_state;
    friend std::shared_ptr<ImageDecodeWorkspaceBudget> prechargedImageDecodeWorkspaceBudget(
        ImageDecodeWorkspaceLease, qsizetype);
    friend ImageDecodeWorkspaceLease ImageDecodeWorkspaceDetail::startLease(
        const ImageDecodeWorkspaceBudget&);
    friend ImageDecodeWorkspaceLease ImageDecodeWorkspaceDetail::startLeaseForOperation(
        const ImageDecodeWorkspaceBudget&, qsizetype);
    friend std::optional<ImageDecodeWorkspaceLease>
    ImageDecodeWorkspaceDetail::tryBestEffortAdmission(
        const ImageDecodeWorkspaceBudget&, ImageDecodeWorkspaceAdmissionRequest);
};

ImageDecodeWorkspaceBudgetLimits resolvedImageDecodeWorkspaceBudgetLimits(
    ImageDecodeWorkspaceBudgetRequest request, SystemMemorySnapshot systemMemory);
std::shared_ptr<ImageDecodeWorkspaceBudget> imageDecodeWorkspaceBudgetForSystemMemory(
    ImageDecodeWorkspaceBudgetRequest request, SystemMemorySnapshot systemMemory);
std::shared_ptr<ImageDecodeWorkspaceBudget> defaultImageDecodeWorkspaceBudget(
    ImageDecodeWorkspaceBudgetRequest request = {}, SystemMemoryRuntime runtime = {});
std::shared_ptr<ImageDecodeWorkspaceBudget> prechargedImageDecodeWorkspaceBudget(
    ImageDecodeWorkspaceLease grant, qsizetype perOperationBaselineByteCount = 0);
QString imageDecodeWorkspaceResourceLimitDiagnostic();
std::optional<qsizetype> checkedImageDecodeWorkspaceByteCount(
    QSize imageSize, qsizetype bytesPerPixel, qsizetype bufferCount);
}

#endif
