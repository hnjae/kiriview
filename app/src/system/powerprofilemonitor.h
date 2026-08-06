// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_POWERPROFILEMONITOR_H
#define KIRIVIEW_POWERPROFILEMONITOR_H

#include "powerprofilemonitorstate.h"
#include "powersaverprovider.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <cstdint>
#include <functional>

namespace kiriview {
using PowerProfilePortalReplyCallback = std::function<void(QVariantList)>;
using PowerProfilePortalReader = std::function<void(QObject*, PowerProfilePortalReplyCallback)>;
using PowerProfilePortalSubscription = std::function<void(QObject*)>;

struct PowerProfileMonitorRuntime
{
    PowerProfilePortalReader readPowerSaverEnabled;
    PowerProfilePortalSubscription subscribePropertiesChanged;
};

class PowerProfileMonitor final : public QObject, public PowerSaverStateMonitor
{
    Q_OBJECT

public:
    explicit PowerProfileMonitor(PowerSaverChangedCallback callback = {});
    PowerProfileMonitor(PowerSaverChangedCallback callback, PowerProfileMonitorRuntime runtime);

    [[nodiscard]] bool powerSaverEnabled() const override;

private Q_SLOTS:
    void handlePropertiesChanged(const QString& interfaceName, const QVariantMap& changedProperties,
        const QStringList& invalidatedProperties);

    // Keep ordinary private methods outside the Qt slot section.
    // NOLINTNEXTLINE(readability-redundant-access-specifiers)
private:
    void refreshPowerSaverEnabled();
    void finishPowerSaverRefresh(quint64 revision, QVariantList arguments);
    void applyPlan(PowerProfileMonitorPlan plan);

    PowerSaverChangedCallback m_callback;
    PowerProfileMonitorRuntime m_runtime;
    PowerProfileMonitorState m_state;
    quint64 m_refreshRevision = 0;
};

PowerProfileMonitorRuntime defaultPowerProfileMonitorRuntime();
PowerProfileMonitorRuntime powerProfileMonitorRuntimeWithDefaults(
    PowerProfileMonitorRuntime runtime);
}

#endif
