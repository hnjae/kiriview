// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONDIRECTMEDIASCOPEPORT_H
#define KIRIVIEW_DOCUMENTSESSIONDIRECTMEDIASCOPEPORT_H

#include "session/directmediacursor.h"

#include <QUrl>
#include <optional>

namespace kiriview {
class DocumentSessionState;

class DocumentSessionDirectMediaScopePort final
{
public:
    explicit DocumentSessionDirectMediaScopePort(const DocumentSessionState* state);

    [[nodiscard]] std::optional<DirectMediaScope> currentScope() const;
    [[nodiscard]] QUrl activeCursorUrl() const;
    [[nodiscard]] bool cursorMatches(const DirectMediaScope& scope) const;

private:
    const DocumentSessionState* m_state = nullptr;
};
}

#endif
