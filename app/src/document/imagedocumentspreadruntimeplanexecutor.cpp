// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentspreadruntimeplanexecutor.h"

#include <utility>
#include <variant>

namespace kiriview {
namespace {
    template <typename Operation, typename... Args>
    void run(const Operation& operation, Args&&... args)
    {
        if (operation) {
            operation(std::forward<Args>(args)...);
        }
    }
}

ImageDocumentSpreadRuntimePlanExecutor::ImageDocumentSpreadRuntimePlanExecutor(
    ImageDocumentSpreadRuntimeOperations operations)
    : m_operations(std::move(operations))
{
}

bool ImageDocumentSpreadRuntimePlanExecutor::dispatchOperation(
    const ImageDocumentRuntimeOperation& operation)
{
    if (std::holds_alternative<ResetRightToLeftReadingOperation>(operation)) {
        run(m_operations.resetRightToLeftReading);
        return true;
    }
    if (std::holds_alternative<ClearSecondaryPageOperation>(operation)) {
        run(m_operations.clearSecondaryPage);
        return true;
    }
    if (std::holds_alternative<NotifyRightToLeftReadingChangedOperation>(operation)) {
        run(m_operations.notifyRightToLeftReadingChanged);
        return true;
    }
    return false;
}
}
