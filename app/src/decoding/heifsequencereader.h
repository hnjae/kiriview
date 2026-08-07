// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_HEIFSEQUENCEREADER_H
#define KIRIVIEW_HEIFSEQUENCEREADER_H

#include "animationframe.h"
#include "imagedecodeworkspace.h"

#include <QByteArray>
#include <QString>
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

class HeifSequenceReader final
{
public:
    HeifSequenceReader();
    explicit HeifSequenceReader(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget);
    ~HeifSequenceReader();

    HeifSequenceReader(const HeifSequenceReader&) = delete;
    HeifSequenceReader& operator=(const HeifSequenceReader&) = delete;
    HeifSequenceReader(HeifSequenceReader&&) noexcept;
    HeifSequenceReader& operator=(HeifSequenceReader&&) noexcept;

    HeifSequenceOpenResult open(QByteArray data);
    AnimationFrameReadResult readNextFrame();
    [[nodiscard]] bool lastReadResourceLimitExceeded() const;
    void close();

private:
    class Private;
    std::unique_ptr<Private> d;
};

QString heifSequenceDecodeErrorString();
}

#endif
