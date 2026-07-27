// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentdeletionstate.h"

namespace kiriview {
ImageDocumentDeletionState::ImageDocumentDeletionState(quint64 nextOperationId)
    : m_fileDeletion(nextOperationId)
{
}

bool ImageDocumentDeletionState::inProgress() const { return m_inProgress; }

ImageDocumentDeletionFileOperationStart ImageDocumentDeletionState::startFileDeletion()
{
    if (m_inProgress) {
        return {};
    }

    const quint64 operationId = m_fileDeletion.start();
    m_claimedFileDeletionOperationId = 0;
    return ImageDocumentDeletionFileOperationStart {
        true,
        operationId,
        setInProgress(true),
    };
}

bool ImageDocumentDeletionState::acceptsFileDeletion(quint64 operationId) const
{
    return m_fileDeletion.accepts(operationId);
}

ImageDocumentDeletionFileOperationClaim ImageDocumentDeletionState::claimFileDeletion(
    quint64 operationId)
{
    if (!m_fileDeletion.finish(operationId)) {
        return {};
    }

    m_claimedFileDeletionOperationId = operationId;
    return { true };
}

bool ImageDocumentDeletionState::acceptsClaimedFileDeletion(quint64 operationId) const
{
    return operationId != 0 && operationId == m_claimedFileDeletionOperationId;
}

bool ImageDocumentDeletionState::settleClaimedFileDeletion(quint64 operationId)
{
    if (!acceptsClaimedFileDeletion(operationId)) {
        return false;
    }

    m_claimedFileDeletionOperationId = 0;
    return setInProgress(false);
}

bool ImageDocumentDeletionState::cancelFileDeletion()
{
    m_fileDeletion.cancel();
    m_claimedFileDeletionOperationId = 0;
    return setInProgress(false);
}

bool ImageDocumentDeletionState::setInProgress(bool inProgress)
{
    if (m_inProgress == inProgress) {
        return false;
    }

    m_inProgress = inProgress;
    return true;
}
}
