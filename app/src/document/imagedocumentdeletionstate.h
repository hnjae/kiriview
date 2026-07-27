// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTDELETIONSTATE_H
#define KIRIVIEW_IMAGEDOCUMENTDELETIONSTATE_H

#include "async/imageasyncoperationstate.h"

namespace kiriview {
struct ImageDocumentDeletionFileOperationStart
{
    bool accepted = false;
    quint64 operationId = 0;
    bool inProgressChanged = false;
};

struct ImageDocumentDeletionFileOperationClaim
{
    bool accepted = false;
};

class ImageDocumentDeletionState final
{
public:
    explicit ImageDocumentDeletionState(quint64 nextOperationId = 0);

    [[nodiscard]] bool inProgress() const;
    ImageDocumentDeletionFileOperationStart startFileDeletion();
    [[nodiscard]] bool acceptsFileDeletion(quint64 operationId) const;
    ImageDocumentDeletionFileOperationClaim claimFileDeletion(quint64 operationId);
    [[nodiscard]] bool acceptsClaimedFileDeletion(quint64 operationId) const;
    bool settleClaimedFileDeletion(quint64 operationId);
    bool cancelFileDeletion();

private:
    bool setInProgress(bool inProgress);

    bool m_inProgress = false;
    quint64 m_claimedFileDeletionOperationId = 0;
    ImageAsyncOperationState m_fileDeletion;
};
}

#endif
