// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "powerprofilemonitor.h"

#include "async/imagecallback.h"
#include "powerprofileportalevents.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QPointer>
#include <QVariant>
#include <utility>

namespace {
constexpr auto portalService = "org.freedesktop.portal.Desktop";
constexpr auto portalPath = "/org/freedesktop/portal/desktop";
constexpr auto dbusPropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr auto powerProfileMonitorInterface = "org.freedesktop.portal.PowerProfileMonitor";
constexpr auto powerSaverEnabledProperty = "power-saver-enabled";

void readPortalPowerSaverEnabled(
    QObject* receiver, kiriview::PowerProfilePortalReplyCallback callback)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        portalService, portalPath, dbusPropertiesInterface, QStringLiteral("Get"));
    message << QString::fromLatin1(powerProfileMonitorInterface)
            << QString::fromLatin1(powerSaverEnabledProperty);

    auto* watcher
        = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), receiver);
    QObject* context = receiver != nullptr ? receiver : watcher;
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context,
        [watcher, callback = std::move(callback)]() mutable {
            QVariantList arguments;
            const QDBusMessage reply = watcher->reply();
            if (reply.type() == QDBusMessage::ReplyMessage) {
                arguments = reply.arguments();
            }
            watcher->deleteLater();
            if (callback) {
                callback(std::move(arguments));
            }
        });
}

void subscribePortalPowerSaverChanges(QObject* receiver)
{
    // clang-format off
    QDBusConnection::sessionBus().connect(portalService, portalPath, dbusPropertiesInterface,
        QStringLiteral("PropertiesChanged"), receiver,
        SLOT(handlePropertiesChanged(QString,QVariantMap,QStringList)));
    // clang-format on
}
}

namespace kiriview {
PowerProfileMonitor::PowerProfileMonitor(PowerSaverChangedCallback callback)
    : PowerProfileMonitor(std::move(callback), {})
{
}

PowerProfileMonitor::PowerProfileMonitor(
    PowerSaverChangedCallback callback, PowerProfileMonitorRuntime runtime)
    : m_callback(std::move(callback))
    , m_runtime(powerProfileMonitorRuntimeWithDefaults(std::move(runtime)))
{
    m_runtime.subscribePropertiesChanged(this);
    refreshPowerSaverEnabled();
}

bool PowerProfileMonitor::powerSaverEnabled() const { return m_state.powerSaverEnabled(); }

void PowerProfileMonitor::handlePropertiesChanged(const QString& interfaceName,
    const QVariantMap& changedProperties, const QStringList& invalidatedProperties)
{
    ++m_refreshRevision;
    const PowerProfileMonitorPlan plan
        = m_state.applyEvent(powerProfileMonitorEventFromPropertiesChanged(
            interfaceName, changedProperties, invalidatedProperties));
    applyPlan(plan);
    if (plan.refreshPowerSaverEnabled) {
        refreshPowerSaverEnabled();
    }
}

void PowerProfileMonitor::refreshPowerSaverEnabled()
{
    const quint64 revision = ++m_refreshRevision;
    const QPointer<PowerProfileMonitor> self(this);
    m_runtime.readPowerSaverEnabled(this, [self, revision](QVariantList arguments) mutable {
        if (self != nullptr) {
            self->finishPowerSaverRefresh(revision, std::move(arguments));
        }
    });
}

void PowerProfileMonitor::finishPowerSaverRefresh(quint64 revision, QVariantList arguments)
{
    if (revision != m_refreshRevision) {
        return;
    }
    applyPlan(m_state.applyEvent(powerProfileMonitorEventFromRefreshReply(arguments)));
}

void PowerProfileMonitor::applyPlan(PowerProfileMonitorPlan plan)
{
    if (!plan.powerSaverChanged) {
        return;
    }

    invokeIfSet(m_callback, m_state.powerSaverEnabled());
}

PowerProfileMonitorRuntime defaultPowerProfileMonitorRuntime()
{
    return PowerProfileMonitorRuntime {
        readPortalPowerSaverEnabled,
        subscribePortalPowerSaverChanges,
    };
}

PowerProfileMonitorRuntime powerProfileMonitorRuntimeWithDefaults(
    PowerProfileMonitorRuntime runtime)
{
    PowerProfileMonitorRuntime defaults = defaultPowerProfileMonitorRuntime();
    if (!runtime.readPowerSaverEnabled) {
        runtime.readPowerSaverEnabled = std::move(defaults.readPowerSaverEnabled);
    }
    if (!runtime.subscribePropertiesChanged) {
        runtime.subscribePropertiesChanged = std::move(defaults.subscribePropertiesChanged);
    }

    return runtime;
}
}
