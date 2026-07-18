// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_p.h"

ImageViewportStateSnapshot ImageViewportPrivate::state() const { return engine.snapshot(); }

ImageViewportCommandResult ImageViewportPrivate::commandResult(
    CommandOutcome outcome, const ImageViewportStateSnapshot& snapshot) const
{
    return ImageViewportCommandResult(outcome, snapshot.diagnostics().commandReason(),
        snapshot.revisions().command(), snapshot.revisions().snapshot());
}
