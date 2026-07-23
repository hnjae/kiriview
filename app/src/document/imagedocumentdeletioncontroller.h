// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTDELETIONCONTROLLER_H
#define KIRIVIEW_IMAGEDOCUMENTDELETIONCONTROLLER_H

#include "async/imageiojob.h"
#include "imagedocumentdeletionfallbackcontroller.h"
#include "imagedocumentdeletionstate.h"
#include "imagedocumentruntimeplan.h"
#include "navigation/imagedocumentpagecandidaterepository.h"
#include "navigation/imageremovalfallback.h"
#include "system/filedeletion.h"

#include <QString>
#include <QtGlobal>
#include <functional>

class QObject;

namespace kiriview {
class ImageDocumentState;

class ImageDocumentDeletionController final
{
public:
    using InProgressChangedCallback = std::function<void()>;
    using RuntimePlanCallback = std::function<void(ImageDocumentRuntimePlan)>;
    using FailedCallback = std::function<void(const QString&)>;
    using HasDisplayedImageCallback = std::function<bool()>;

    struct Callbacks
    {
        InProgressChangedCallback inProgressChanged;
        RuntimePlanCallback runtimePlan;
        FailedCallback failed;
    };

    ImageDocumentDeletionController(QObject* parent, ImageDocumentState& state,
        HasDisplayedImageCallback hasDisplayedImage,
        ImageDocumentPageCandidateProvider candidateProvider,
        FileDeletionProvider fileDeletionProvider, Callbacks callbacks,
        std::function<ResolvedNavigationSource(const QUrl&)> resolveExternalSource);
    ~ImageDocumentDeletionController();

    [[nodiscard]] bool inProgress() const;
    void deleteDisplayedFile(FileDeletionMode mode);
    void cancel();

private:
    void finishFileDeletion(quint64 operationId, const ImageRemovalFallbackPlan& fallbackPlan,
        FileDeletionResult result, const KioOperationFailure& failure);
    void notifyInProgressChangedIf(bool changed);
    void cancelFileDeletion();
    void reportRuntimePlan(ImageDocumentRuntimePlan plan);
    void reportFailure(const KioOperationFailure& failure);

    QObject* m_parent = nullptr;
    ImageDocumentState& m_state;
    HasDisplayedImageCallback m_hasDisplayedImage;
    Callbacks m_callbacks;
    FileDeletionProvider m_fileDeletionProvider;
    ImageIoJob m_fileDeletionJob;
    ImageDocumentDeletionState m_deletionState;
    ImageDocumentDeletionFallbackController m_fallbackController;
};
}

#endif
