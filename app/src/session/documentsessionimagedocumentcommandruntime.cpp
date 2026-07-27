// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionimagedocumentcommandruntime.h"

#include <QString>
#include <utility>

namespace kiriview {
DocumentSessionImageDocumentCommandRuntime::DocumentSessionImageDocumentCommandRuntime(
    DocumentSessionImageDocumentCommandPort commands)
    : m_commands(std::move(commands))
{
}

DocumentSessionImageDocumentCommandRuntime::~DocumentSessionImageDocumentCommandRuntime()
{
    m_callbackLifetime.reset();
}

bool DocumentSessionImageDocumentCommandRuntime::setSource(const ResolvedNavigationSource& source)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const quint64 commandRevision = m_sourceCommandAdmission.next();
    const auto setSource = m_commands.source.setSource;
    if (setSource) {
        setSource(source);
    }
    return !lifetime.expired() && m_sourceCommandAdmission.accepts(commandRevision);
}

bool DocumentSessionImageDocumentCommandRuntime::clearSourceUrl()
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const quint64 commandRevision = m_sourceCommandAdmission.next();
    const auto clearSource = m_commands.source.clearSource;
    if (clearSource) {
        clearSource();
    }
    return !lifetime.expired() && m_sourceCommandAdmission.accepts(commandRevision);
}

std::optional<MediaEntrySourceVideoPlaybackDeviceResult>
DocumentSessionImageDocumentCommandRuntime::loadOpenedCollectionVideoPlaybackDevice(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const quint64 commandRevision = m_sourceCommandAdmission.next();
    const auto loadPlaybackDevice = m_commands.source.loadOpenedCollectionVideoPlaybackDevice;
    if (loadPlaybackDevice) {
        MediaEntrySourceVideoPlaybackDeviceResult result
            = loadPlaybackDevice(openedCollectionScope, videoUrl);
        if (lifetime.expired() || !m_sourceCommandAdmission.accepts(commandRevision)) {
            return std::nullopt;
        }
        return result;
    }

    return MediaEntrySourceVideoPlaybackDeviceResult(std::unexpected(MediaEntrySourceError {
        MediaEntrySourceErrorCause::ProviderUnavailable,
        MediaEntrySourceBackendKind::Unknown,
        MediaEntrySourceOperation::OpenVideoPlaybackDevice,
        openedCollectionScope.fileUrl(),
        {},
        QStringLiteral("document session has no opened collection video command"),
    }));
}

void DocumentSessionImageDocumentCommandRuntime::openPreviousPage()
{
    const auto openPreviousPage = m_commands.pageNavigation.openPreviousPage;
    if (openPreviousPage) {
        openPreviousPage();
    }
}

void DocumentSessionImageDocumentCommandRuntime::openNextPage()
{
    const auto openNextPage = m_commands.pageNavigation.openNextPage;
    if (openNextPage) {
        openNextPage();
    }
}

void DocumentSessionImageDocumentCommandRuntime::openImageAtPage(int number)
{
    const auto openImageAtPage = m_commands.pageNavigation.openImageAtPage;
    if (openImageAtPage) {
        openImageAtPage(number);
    }
}

void DocumentSessionImageDocumentCommandRuntime::deleteDisplayedFile(FileDeletionMode mode)
{
    const auto deleteDisplayedFile = m_commands.deletion.deleteDisplayedFile;
    if (deleteDisplayedFile) {
        deleteDisplayedFile(mode);
    }
}
}
