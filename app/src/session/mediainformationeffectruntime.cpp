// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/mediainformationeffectruntime.h"

#include <KIO/OpenFileManagerWindowJob>
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
    if (!effects.openContainingFolder) {
        effects.openContainingFolder = [](const QUrl& targetUrl) -> QObject* {
            return KIO::highlightInFileManager(QList<QUrl> { targetUrl });
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
    if (!snapshot.available || !snapshot.canOpenContainingFolder) {
        return;
    }

    const std::function<QObject*(QUrl)> effect = m_effects.openContainingFolder;
    const QUrl targetUrl = snapshot.targetUrl;
    const QPointer<QObject> owner(this);
    QObject* const job = effect(targetUrl);
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
