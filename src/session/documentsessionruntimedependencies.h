// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONRUNTIMEDEPENDENCIES_H
#define KIRIVIEW_DOCUMENTSESSIONRUNTIMEDEPENDENCIES_H

#include "navigation/directmedianavigationcandidateprovider.h"
#include "session/documentsessionmediapredecoderuntime.h"
#include "session/documentsessionthumbnailruntime.h"
#include "session/mediaopenwith.h"
#include "system/filedeletion.h"

namespace kiriview {
struct DocumentSessionRuntimeDependencies
{
    DirectMediaNavigationCandidateProvider directMediaNavigationCandidateProvider;
    FileDeletionProvider fileDeletionProvider;
    MediaOpenWithProvider mediaOpenWithProvider;
    ActiveNavigationThumbnailRuntimeDependencies activeNavigationThumbnails;
    MediaPredecodeDependencyOverrides directMediaPredecodeDependencies;
};
}

#endif
