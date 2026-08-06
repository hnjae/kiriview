// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_POWERSAVERPROVIDER_H
#define KIRIVIEW_POWERSAVERPROVIDER_H

#include <QObject>
#include <QtGlobal>

#include <functional>
#include <memory>

namespace kiriview {
struct PowerProfileMonitorRuntime;

class PowerSaverStateMonitor
{
public:
    PowerSaverStateMonitor() = default;
    virtual ~PowerSaverStateMonitor() = default;
    [[nodiscard]] virtual bool powerSaverEnabled() const = 0;
    Q_DISABLE_COPY_MOVE(PowerSaverStateMonitor)
};

using PowerSaverChangedCallback = std::function<void(bool)>;
using PowerSaverMonitorFactory
    = std::function<std::unique_ptr<PowerSaverStateMonitor>(PowerSaverChangedCallback)>;

struct PowerSaverProvider
{
    PowerSaverMonitorFactory monitor;
};

class PowerSaverRuntime final : public QObject
{
public:
    explicit PowerSaverRuntime(QObject* parent = nullptr);
    PowerSaverRuntime(PowerProfileMonitorRuntime runtime, QObject* parent = nullptr);
    ~PowerSaverRuntime() override;
    Q_DISABLE_COPY_MOVE(PowerSaverRuntime)

    [[nodiscard]] PowerSaverProvider provider() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};
}

#endif
