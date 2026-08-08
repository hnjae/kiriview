// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGESOURCEDATA_H
#define KIRIVIEW_IMAGESOURCEDATA_H

#include "system/systemmemory.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QtGlobal>
#include <memory>

class QIODevice;

namespace kiriview {
namespace ImageSourceDataDetail {
    struct BudgetState;
    struct LeaseState;
}

struct ImageSourceDataBudgetRequest
{
    qsizetype aggregateByteLimit = 0;
    qsizetype perSourceByteLimit = 0;
};

struct ImageSourceDataBudgetLimits
{
    qsizetype aggregateByteLimit = 0;
    qsizetype perSourceByteLimit = 0;
};

class ImageSourceDataLease final
{
public:
    ImageSourceDataLease() = default;

    [[nodiscard]] bool tryReserve(qsizetype additionalByteCount);
    [[nodiscard]] qsizetype reservedByteCount() const;
    [[nodiscard]] bool isManaged() const;

private:
    explicit ImageSourceDataLease(std::shared_ptr<ImageSourceDataDetail::LeaseState> state);

    std::shared_ptr<ImageSourceDataDetail::LeaseState> m_state;
    friend class ImageSourceDataBudget;
};

class ImageSourceDataBudget final
{
public:
    ImageSourceDataBudget(qsizetype aggregateByteLimit, qsizetype perSourceByteLimit);

    [[nodiscard]] ImageSourceDataLease startLease() const;
    [[nodiscard]] qsizetype aggregateByteLimit() const;
    [[nodiscard]] qsizetype perSourceByteLimit() const;
    [[nodiscard]] qsizetype reservedByteCount() const;

private:
    std::shared_ptr<ImageSourceDataDetail::BudgetState> m_state;
};

struct ImageSourceData
{
    ImageSourceData() = default;
    ImageSourceData(QByteArray data, ImageSourceDataLease lease = {});

    [[nodiscard]] bool tryReserveExpectedByteCount(qint64 expectedByteCount);
    [[nodiscard]] bool tryAppend(QByteArrayView chunk);

    QByteArray data;
    ImageSourceDataLease lease;
};

enum class ImageSourceDataReadStatus {
    Ready,
    ReadFailed,
    ResourceLimitExceeded,
};

struct ImageSourceDataReadResult
{
    ImageSourceDataReadStatus status = ImageSourceDataReadStatus::ReadFailed;
    ImageSourceData sourceData;
    QString diagnosticDetail;
};

ImageSourceDataBudgetLimits resolvedImageSourceDataBudgetLimits(
    ImageSourceDataBudgetRequest request, SystemMemorySnapshot systemMemory);
std::shared_ptr<ImageSourceDataBudget> imageSourceDataBudgetForSystemMemory(
    ImageSourceDataBudgetRequest request, SystemMemorySnapshot systemMemory);
std::shared_ptr<ImageSourceDataBudget> defaultImageSourceDataBudget(
    ImageSourceDataBudgetRequest request = {}, SystemMemoryRuntime runtime = {});
ImageSourceDataReadResult readImageSourceData(
    QIODevice& device, ImageSourceDataLease lease, qint64 expectedByteCount = -1);
QString imageSourceDataResourceLimitDiagnostic();
}

#endif
