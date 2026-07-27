// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionvideodocumentcommandruntime.h"

#include <QObject>
#include <utility>

namespace kiriview {
DocumentSessionVideoDocumentCommandRuntime::DocumentSessionVideoDocumentCommandRuntime(
    DocumentSessionVideoDocumentCommandPort commands,
    DocumentSessionVideoOutputClearPort clearVideoOutput)
    : m_commands(std::move(commands))
    , m_clearVideoOutput(std::move(clearVideoOutput))
{
}

DocumentSessionVideoDocumentCommandRuntime::~DocumentSessionVideoDocumentCommandRuntime()
{
    m_callbackLifetime.reset();
}

bool DocumentSessionVideoDocumentCommandRuntime::setSource(const ResolvedNavigationSource& source)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const quint64 commandRevision = m_commandAdmission.next();
    const auto setSource = m_commands.source.setSource;
    if (setSource) {
        setSource(source);
    }
    return !lifetime.expired() && m_commandAdmission.accepts(commandRevision);
}

bool DocumentSessionVideoDocumentCommandRuntime::setSourceDevice(
    const QUrl& sourceUrl, VideoPlaybackSourceDevice sourceDevice)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const quint64 commandRevision = m_commandAdmission.next();
    const auto setSourceDevice = m_commands.source.setSourceDevice;
    if (setSourceDevice) {
        setSourceDevice(sourceUrl, std::move(sourceDevice));
    }
    return !lifetime.expired() && m_commandAdmission.accepts(commandRevision);
}

bool DocumentSessionVideoDocumentCommandRuntime::leaveMode(const QUrl& currentSourceUrl)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const quint64 commandRevision = m_commandAdmission.next();
    const auto videoOutput = m_commands.output.videoOutput;
    const DocumentSessionVideoOutputAttachmentPort attachmentPort = outputAttachmentPort();
    const auto clearVideoOutput = m_clearVideoOutput;
    const auto stop = m_commands.playback.stop;
    const auto clearSource = m_commands.source.clearSource;

    QObject* attachedVideoOutput = videoOutput ? videoOutput() : nullptr;
    if (lifetime.expired() || !m_commandAdmission.accepts(commandRevision)) {
        return false;
    }
    const bool cleanupRequired = !currentSourceUrl.isEmpty() || attachedVideoOutput != nullptr;

    if (clearVideoOutput) {
        clearVideoOutput();
    } else if (cleanupRequired) {
        if (attachmentPort.setVideoOutputAttachment) {
            attachmentPort.setVideoOutputAttachment(nullptr, {}, {});
        }
    }
    if (lifetime.expired() || !m_commandAdmission.accepts(commandRevision)) {
        return false;
    }
    if (!cleanupRequired) {
        return true;
    }

    if (stop) {
        stop();
    }
    if (lifetime.expired() || !m_commandAdmission.accepts(commandRevision)) {
        return false;
    }

    if (clearSource) {
        clearSource();
    }
    return !lifetime.expired() && m_commandAdmission.accepts(commandRevision);
}

DocumentSessionVideoOutputAttachmentPort
DocumentSessionVideoDocumentCommandRuntime::outputAttachmentPort() const
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const auto setVideoOutputAttachment = m_commands.output.setVideoOutputAttachment;
    return DocumentSessionVideoOutputAttachmentPort {
        [lifetime, setVideoOutputAttachment](
            QObject* videoOutput, const QRectF& contentRect, const QRectF& sourceRect) {
            if (lifetime.expired()) {
                return;
            }
            const auto callback = setVideoOutputAttachment;
            if (callback) {
                callback(videoOutput, contentRect, sourceRect);
            }
        },
    };
}
}
