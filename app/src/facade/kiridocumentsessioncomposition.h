// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_KIRIDOCUMENTSESSIONCOMPOSITION_H
#define KIRIVIEW_KIRIDOCUMENTSESSIONCOMPOSITION_H

#include "async/timerscheduler.h"
#include "document/imagedocumentruntimedependencies.h"
#include "facade/kiridocumentsession.h"
#include "session/documentsessionruntimedependencies.h"
#include "session/mediainformationeffectruntime.h"
#include "system/systemmemory.h"
#include "video/videomediabackend.h"

#include <QString>
#include <functional>
#include <optional>

namespace kiriview {
struct KiriImageDocumentComposition
{
    ImageDocumentRuntimeDependencyOverrides runtimeDependencies;
    std::function<void(const QString&)> fileDeletionFailed;
};

struct KiriVideoDocumentComposition
{
    TimerScheduler playbackControlTimerScheduler;
    VideoMediaBackendFactory mediaBackendFactory;
};

struct KiriMediaInformationComposition
{
    MediaInformationEffectCommandPort effectCommands;
};

struct KiriDocumentSessionDependencies
{
    std::optional<SystemMemorySnapshot> systemMemorySnapshot;
    DocumentSessionRuntimeDependencies sessionRuntime;
    ImageDocumentRuntimeDependencyOverrides imageDocument;
    TimerScheduler videoPlaybackControlTimerScheduler;
    VideoMediaBackendFactory videoMediaBackendFactory;
    MediaInformationEffects mediaInformationEffects;
};

class KiriDocumentSessionFactory final
{
public:
    static KiriDocumentSession* create(
        KiriDocumentSessionDependencies dependencies, QObject* parent = nullptr);
};

KiriDocumentSessionDependencies resolveKiriDocumentSessionDependencies(
    KiriDocumentSessionDependencies dependencies);
}

#endif
