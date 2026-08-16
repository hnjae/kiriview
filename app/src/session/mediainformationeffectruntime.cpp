// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/mediainformationeffectruntime.h"

#include <KIO/OpenFileManagerWindowJob>
#include <KIO/OpenUrlJob>
#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QList>
#include <QPointer>
#include <utility>

namespace {
kiriview::MediaInformationEffects effectsWithDefaults(kiriview::MediaInformationEffects effects)
{
    if (!effects.copyText) {
        effects.copyText = [](const QString& text) {
            if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) == nullptr) {
                return;
            }
            QClipboard* clipboard = QGuiApplication::clipboard();
            if (clipboard != nullptr) {
                clipboard->setText(text);
            }
        };
    }
    if (!effects.revealInFileManager) {
        effects.revealInFileManager = [](const QUrl& targetUrl) -> QObject* {
            return KIO::highlightInFileManager(QList<QUrl> { targetUrl });
        };
    }
    if (!effects.openLocation) {
        effects.openLocation = [](const QUrl& locationUrl) -> QObject* {
            auto* job = new KIO::OpenUrlJob(locationUrl, QStringLiteral("inode/directory"));
            job->start();
            return job;
        };
    }
    return effects;
}
}

namespace kiriview {
MediaInformationEffectRuntime::MediaInformationEffectRuntime(
    SnapshotProvider snapshotProvider, MediaInformationEffects effects, QObject* parent)
    : QObject(parent)
    , m_snapshotProvider(std::move(snapshotProvider))
    , m_effects(effectsWithDefaults(std::move(effects)))
{
}

MediaInformationEffectCommandPort MediaInformationEffectRuntime::commandPort()
{
    const QPointer<MediaInformationEffectRuntime> runtime(this);
    return {
        [runtime]() {
            if (runtime != nullptr) {
                runtime->copyFilePath();
            }
        },
        [runtime]() {
            if (runtime != nullptr) {
                runtime->openContainingFolder();
            }
        },
    };
}

void MediaInformationEffectRuntime::copyFilePath()
{
    const MediaInformationProjectionSnapshot snapshot
        = m_snapshotProvider ? m_snapshotProvider() : MediaInformationProjectionSnapshot {};
    if (!snapshot.available || !snapshot.canCopyFilePath) {
        return;
    }

    const std::function<void(QString)> effect = m_effects.copyText;
    const QString displayPath = mediaInformationDisplayPathForUrl(snapshot.targetUrl);
    effect(displayPath);
}

void MediaInformationEffectRuntime::openContainingFolder()
{
    const MediaInformationProjectionSnapshot snapshot
        = m_snapshotProvider ? m_snapshotProvider() : MediaInformationProjectionSnapshot {};
    if (!snapshot.available || !snapshot.canOpenContainingFolder
        || !snapshot.openContainingFolderRequest.has_value()) {
        return;
    }

    const MediaInformationOpenContainingFolderRequest request
        = *snapshot.openContainingFolderRequest;
    if (request.targetUrl.isEmpty()) {
        return;
    }

    std::function<QObject*(QUrl)> effect;
    switch (request.kind) {
    case MediaInformationOpenContainingFolderKind::RevealTarget:
        effect = m_effects.revealInFileManager;
        break;
    case MediaInformationOpenContainingFolderKind::OpenLocation:
        effect = m_effects.openLocation;
        break;
    }

    const QPointer<QObject> owner(this);
    QObject* const job = effect(request.targetUrl);
    if (job == nullptr) {
        return;
    }
    if (owner == nullptr) {
        delete job;
        return;
    }
    job->setParent(owner.data());
}
}
