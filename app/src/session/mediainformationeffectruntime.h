// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAINFORMATIONEFFECTRUNTIME_H
#define KIRIVIEW_MEDIAINFORMATIONEFFECTRUNTIME_H

#include "session/mediainformationprojection.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>

namespace kiriview {
struct MediaInformationEffects
{
    std::function<void(QString)> copyText;
    std::function<QObject*(QUrl)> openContainingFolder;
};

struct MediaInformationEffectCommandPort
{
    std::function<void()> copyFilePath;
    std::function<void()> openContainingFolder;
};

class MediaInformationEffectRuntime final : public QObject
{
public:
    using SnapshotProvider = std::function<MediaInformationProjectionSnapshot()>;

    explicit MediaInformationEffectRuntime(SnapshotProvider snapshotProvider,
        MediaInformationEffects effects = {}, QObject* parent = nullptr);
    ~MediaInformationEffectRuntime() override = default;
    Q_DISABLE_COPY_MOVE(MediaInformationEffectRuntime)

    [[nodiscard]] MediaInformationEffectCommandPort commandPort();

private:
    void copyFilePath();
    void openContainingFolder();

    SnapshotProvider m_snapshotProvider;
    MediaInformationEffects m_effects;
};
}

#endif
