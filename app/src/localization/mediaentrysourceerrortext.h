// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAENTRYSOURCEERRORTEXT_H
#define KIRIVIEW_MEDIAENTRYSOURCEERRORTEXT_H

#include "archive/mediaentrysourceerror.h"

#include <QString>

namespace kiriview {
QString mediaEntrySourceErrorText(const MediaEntrySourceError& error);
}

#endif
