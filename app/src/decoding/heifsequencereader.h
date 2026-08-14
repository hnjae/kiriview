// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_HEIFSEQUENCEREADER_H
#define KIRIVIEW_HEIFSEQUENCEREADER_H

#include "animationframe.h"
#include "imagedecodeworkspace.h"

#include <QByteArray>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <cstdint>
#include <memory>
#include <optional>

namespace kiriview {
enum class HeifSequenceOpenStatus {
    NotHeif,
    NotSequence,
    Success,
    Error,
    ResourceLimitExceeded,
};

struct HeifSequenceOpenResult
{
    HeifSequenceOpenStatus status = HeifSequenceOpenStatus::NotHeif;
    QString errorString;
    int repeatCount = 0;
};

inline constexpr qsizetype heifSequenceProbeWorkspaceByteCount = qsizetype { 8 } * 1024 * 1024;

struct HeifSequenceWorkspacePlan
{
    QSize imageSize;
    qsizetype transientByteCount = 0;
    qsizetype outputByteCount = 0;
    std::uint64_t decoderByteLimit = 0;
    std::uint64_t pixelLimit = 0;
};

struct HeifSequenceWorkspacePlanResult
{
    HeifSequenceOpenStatus status = HeifSequenceOpenStatus::NotHeif;
    HeifSequenceWorkspacePlan plan;
    QString errorString;
};

[[nodiscard]] std::optional<HeifSequenceWorkspacePlan> heifSequenceWorkspacePlan(QSize imageSize);
[[nodiscard]] HeifSequenceWorkspacePlanResult planHeifSequenceOpen(const QByteArray& data,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {},
    qsizetype perOperationBaselineByteCount = 0);

class HeifSequenceReader final
{
public:
    HeifSequenceReader();
    explicit HeifSequenceReader(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        qsizetype perOperationBaselineByteCount = 0);
    ~HeifSequenceReader();

    HeifSequenceReader(const HeifSequenceReader&) = delete;
    HeifSequenceReader& operator=(const HeifSequenceReader&) = delete;
    HeifSequenceReader(HeifSequenceReader&&) noexcept;
    HeifSequenceReader& operator=(HeifSequenceReader&&) noexcept;

    HeifSequenceOpenResult open(QByteArray data);
    HeifSequenceOpenResult open(QByteArray data, const HeifSequenceWorkspacePlan& plan);
    AnimationFrameReadResult readNextFrame();
    AnimationFrameReadResult readNextFrame(
        const std::shared_ptr<ImageDecodeWorkspaceBudget>& outputWorkspaceBudget);
    [[nodiscard]] bool lastReadResourceLimitExceeded() const;
    void close();

private:
    class Private;
    std::unique_ptr<Private> d;
};

QString heifSequenceDecodeErrorString();
}

#endif
