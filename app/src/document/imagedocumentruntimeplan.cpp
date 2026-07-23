// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentruntimeplan.h"

namespace kiriview {
ImageDocumentRuntimePlan imageDocumentClearImagePlan()
{
    return {
        ClearMediaEntrySourceOperation {},
        ClearPredecodeOperation {},
        ClearSecondaryPageOperation {},
        CancelPageNavigationUpdateOperation {},
        ClearPresentationImageOperation {},
        ClearPageNavigationOperation {},
        NotifyRightToLeftReadingChangedOperation {},
    };
}

ImageDocumentRuntimePlan imageDocumentClearDeletedImagePlan()
{
    return {
        ClearMediaEntrySourceOperation {},
        CancelAllNavigationOperation {},
        CancelPredecodeOperation {},
        CancelOpenOperation {},
        ClearSecondaryPageOperation {},
        SetSourceUrlOperation { ImageDocumentPageTarget {} },
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
