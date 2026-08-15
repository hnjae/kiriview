// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_SVGWORKERLIMITS_H
#define KIRIVIEW_SVGWORKERLIMITS_H

#include <QtGlobal>

namespace kiriview {
inline constexpr qsizetype svgWorkerAddressSpaceByteLimit = qsizetype { 512 } * 1024 * 1024;
inline constexpr int svgWorkerDecodeErrorExitCode = 4;
inline constexpr int svgWorkerResourceExhaustedExitCode = 6;
}

#endif
