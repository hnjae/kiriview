// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentruntimeplan.h"

#include <iterator>

namespace kiriview {
ImageDocumentRuntimePlan imageDocumentClearPresentationPlan()
{
    return {
        ClearPredecodeOperation {},
        ClearSecondaryPageOperation {},
        CancelPageNavigationUpdateOperation {},
        ClearPresentationImageOperation {},
        ClearPageNavigationOperation {},
        NotifyRightToLeftReadingChangedOperation {},
    };
}

ImageDocumentRuntimePlan imageDocumentClearImagePlan()
{
    ImageDocumentRuntimePlan plan { ClearMediaEntrySourceOperation {} };
    ImageDocumentRuntimePlan presentationPlan = imageDocumentClearPresentationPlan();
    plan.insert(plan.end(), std::make_move_iterator(presentationPlan.begin()),
        std::make_move_iterator(presentationPlan.end()));
    return plan;
}

ImageDocumentRuntimePlan imageDocumentClearDeletedImagePlan()
{
    return {
        ClearMediaEntrySourceOperation {},
        CancelAllNavigationOperation {},
        CancelPredecodeOperation {},
        CancelOpenOperation {},
        ClearSecondaryPageOperation {},
        SelectImageTargetOperation { ImageDocumentSelectedTarget {} },
        SetErrorStringOperation { QString() },
        FinishEmptySourceLoadOperation {},
    };
}

ImageDocumentRuntimePlan imageDocumentShutdownPlan()
{
    return {
        CancelFileDeletionOperation {},
        ShutdownSpreadOperation {},
        CancelPredecodeOperation {},
        CancelAllNavigationOperation {},
        CancelOpenOperation {},
        ClearMediaEntrySourceOperation {},
    };
}
}
