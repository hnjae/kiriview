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

void DocumentSessionImageDocumentCommandRuntime::setSource(const ResolvedNavigationSource& source)
{
    if (m_commands.source.setSource) {
        m_commands.source.setSource(source);
    }
}

void DocumentSessionImageDocumentCommandRuntime::clearSourceUrl()
{
    if (m_commands.source.clearSource) {
        m_commands.source.clearSource();
    }
}

MediaEntrySourceVideoPlaybackDeviceResult
DocumentSessionImageDocumentCommandRuntime::loadOpenedCollectionVideoPlaybackDevice(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl)
{
    if (m_commands.source.loadOpenedCollectionVideoPlaybackDevice) {
        return m_commands.source.loadOpenedCollectionVideoPlaybackDevice(
            openedCollectionScope, videoUrl);
    }

    return std::unexpected(MediaEntrySourceError {
        MediaEntrySourceErrorCause::ProviderUnavailable,
        MediaEntrySourceBackendKind::Unknown,
        MediaEntrySourceOperation::OpenVideoPlaybackDevice,
        openedCollectionScope.fileUrl(),
        {},
        QStringLiteral("document session has no opened collection video command"),
    });
}

void DocumentSessionImageDocumentCommandRuntime::openPreviousPage()
{
    if (m_commands.pageNavigation.openPreviousPage) {
        m_commands.pageNavigation.openPreviousPage();
    }
}

void DocumentSessionImageDocumentCommandRuntime::openNextPage()
{
    if (m_commands.pageNavigation.openNextPage) {
        m_commands.pageNavigation.openNextPage();
    }
}

void DocumentSessionImageDocumentCommandRuntime::openImageAtPage(int number)
{
    if (m_commands.pageNavigation.openImageAtPage) {
        m_commands.pageNavigation.openImageAtPage(number);
    }
}

void DocumentSessionImageDocumentCommandRuntime::deleteDisplayedFile(FileDeletionMode mode)
{
    if (m_commands.deletion.deleteDisplayedFile) {
        m_commands.deletion.deleteDisplayedFile(mode);
    }
}
}
