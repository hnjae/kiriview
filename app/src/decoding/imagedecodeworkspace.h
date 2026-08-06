// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDECODEWORKSPACE_H
#define KIRIVIEW_IMAGEDECODEWORKSPACE_H

#include "system/systemmemory.h"

#include <QString>
#include <QtGlobal>
#include <memory>

namespace kiriview {
namespace ImageDecodeWorkspaceDetail {
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

class ImageDecodeWorkspaceLease;

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
    [[nodiscard]] ImageDecodeWorkspaceHold sharedHold() const;
    [[nodiscard]] ImageDecodeWorkspaceHold retainOnly(qsizetype retainedByteCount);
    [[nodiscard]] qsizetype reservedByteCount() const;
    [[nodiscard]] bool isManaged() const;

private:
    explicit ImageDecodeWorkspaceLease(
        std::shared_ptr<ImageDecodeWorkspaceDetail::LeaseState> state);

    std::shared_ptr<ImageDecodeWorkspaceDetail::LeaseState> m_state;
    friend class ImageDecodeWorkspaceBudget;
};

class ImageDecodeWorkspaceBudget final
{
public:
    ImageDecodeWorkspaceBudget(qsizetype aggregateByteLimit, qsizetype perOperationByteLimit);

    [[nodiscard]] ImageDecodeWorkspaceLease startLease() const;
    [[nodiscard]] qsizetype aggregateByteLimit() const;
    [[nodiscard]] qsizetype perOperationByteLimit() const;
    [[nodiscard]] qsizetype reservedByteCount() const;

private:
    std::shared_ptr<ImageDecodeWorkspaceDetail::BudgetState> m_state;
};

ImageDecodeWorkspaceBudgetLimits resolvedImageDecodeWorkspaceBudgetLimits(
    ImageDecodeWorkspaceBudgetRequest request, SystemMemorySnapshot systemMemory);
std::shared_ptr<ImageDecodeWorkspaceBudget> defaultImageDecodeWorkspaceBudget(
    ImageDecodeWorkspaceBudgetRequest request = {}, SystemMemorySnapshot systemMemory = {});
QString imageDecodeWorkspaceResourceLimitDiagnostic();
}

#endif
