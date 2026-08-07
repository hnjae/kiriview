// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "system/powerprofilemonitor.h"

#include <QDBusVariant>
#include <QMetaObject>
#include <QObject>
#include <QTest>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <utility>
#include <vector>

namespace {
struct FakePowerProfilePortal
{
    int readCount = 0;
    int subscriptionCount = 0;
    QObject* subscriber = nullptr;
    std::vector<QString> effects;
    std::vector<kiriview::PowerProfilePortalReplyCallback> pendingReads;

    kiriview::PowerProfileMonitorRuntime runtime()
    {
        return kiriview::PowerProfileMonitorRuntime {
            [this](QObject*, kiriview::PowerProfilePortalReplyCallback callback) {
                ++readCount;
                effects.push_back(QStringLiteral("read"));
                pendingReads.push_back(std::move(callback));
            },
            [this](QObject* receiver) {
                ++subscriptionCount;
                effects.push_back(QStringLiteral("subscribe"));
                subscriber = receiver;
            },
        };
    }

    void completeRead(std::size_t index, QVariantList arguments)
    {
        QVERIFY(index < pendingReads.size());
        kiriview::PowerProfilePortalReplyCallback callback = std::move(pendingReads.at(index));
        QVERIFY(callback);
        callback(std::move(arguments));
    }

    bool emitPropertiesChanged(const QString& interfaceName, const QVariantMap& changedProperties,
        const QStringList& invalidatedProperties) const
    {
        if (subscriber == nullptr) {
            return false;
        }

        return QMetaObject::invokeMethod(subscriber, "handlePropertiesChanged",
            Qt::DirectConnection, Q_ARG(QString, interfaceName),
            Q_ARG(QVariantMap, changedProperties), Q_ARG(QStringList, invalidatedProperties));
    }
};
}

class TestPowerProfileMonitor : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initialRefreshSubscribesThenReadsPortalAsynchronously();
    void ignoredPropertyChangeDoesNotSupersedePendingRefresh();
    void changedPropertySupersedesPendingRefresh();
    void invalidatedPropertyRequestsAsynchronousRefresh();
    void sharedRuntimeProvidesOnePortalObserverToAllConsumers();
    void runtimeDefaultsFillMissingEffectsAndPreserveOverrides();
};

void TestPowerProfileMonitor::initialRefreshSubscribesThenReadsPortalAsynchronously()
{
    FakePowerProfilePortal portal;
    std::vector<bool> changes;

    kiriview::PowerProfileMonitor monitor(
        [&changes](bool enabled) { changes.push_back(enabled); }, portal.runtime());

    QCOMPARE(portal.readCount, 1);
    QCOMPARE(portal.subscriptionCount, 1);
    QCOMPARE(portal.subscriber, &monitor);
    QCOMPARE(portal.effects,
        std::vector<QString>({ QStringLiteral("subscribe"), QStringLiteral("read") }));
    QVERIFY(!monitor.powerSaverEnabled());
    QVERIFY(changes.empty());

    portal.completeRead(0, { QVariant::fromValue(QDBusVariant(QVariant(true))) });

    QVERIFY(monitor.powerSaverEnabled());
    QCOMPARE(changes.size(), std::size_t(1));
    QCOMPARE(changes.at(0), true);
}

void TestPowerProfileMonitor::ignoredPropertyChangeDoesNotSupersedePendingRefresh()
{
    FakePowerProfilePortal portal;
    std::vector<bool> changes;
    kiriview::PowerProfileMonitor monitor(
        [&changes](bool enabled) { changes.push_back(enabled); }, portal.runtime());

    QVariantMap changedProperties;
    changedProperties.insert(QStringLiteral("unrelated-property"), true);
    QVERIFY(portal.emitPropertiesChanged(
        QStringLiteral("org.freedesktop.portal.PowerProfileMonitor"), changedProperties, {}));

    QCOMPARE(portal.readCount, 1);
    portal.completeRead(0, { QVariant::fromValue(QDBusVariant(QVariant(true))) });

    QVERIFY(monitor.powerSaverEnabled());
    QCOMPARE(changes, std::vector<bool>({ true }));
}

void TestPowerProfileMonitor::changedPropertySupersedesPendingRefresh()
{
    FakePowerProfilePortal portal;
    std::vector<bool> changes;
    kiriview::PowerProfileMonitor monitor(
        [&changes](bool enabled) { changes.push_back(enabled); }, portal.runtime());

    QVariantMap changedProperties;
    changedProperties.insert(
        QStringLiteral("power-saver-enabled"), QVariant::fromValue(QDBusVariant(QVariant(true))));

    QVERIFY(portal.emitPropertiesChanged(
        QStringLiteral("org.freedesktop.portal.PowerProfileMonitor"), changedProperties, {}));

    QCOMPARE(portal.readCount, 1);
    QVERIFY(monitor.powerSaverEnabled());
    QCOMPARE(changes.size(), std::size_t(1));
    QCOMPARE(changes.at(0), true);

    portal.completeRead(0, { false });

    QVERIFY(monitor.powerSaverEnabled());
    QCOMPARE(changes, std::vector<bool>({ true }));
}

void TestPowerProfileMonitor::invalidatedPropertyRequestsAsynchronousRefresh()
{
    FakePowerProfilePortal portal;
    std::vector<bool> changes;
    kiriview::PowerProfileMonitor monitor(
        [&changes](bool enabled) { changes.push_back(enabled); }, portal.runtime());

    QVERIFY(
        portal.emitPropertiesChanged(QStringLiteral("org.freedesktop.portal.PowerProfileMonitor"),
            {}, { QStringLiteral("power-saver-enabled") }));

    QCOMPARE(portal.readCount, 2);
    QVERIFY(!monitor.powerSaverEnabled());
    QVERIFY(changes.empty());

    portal.completeRead(1, { true });

    QVERIFY(monitor.powerSaverEnabled());
    QCOMPARE(changes.size(), std::size_t(1));
    QCOMPARE(changes.at(0), true);

    portal.completeRead(0, {});

    QVERIFY(monitor.powerSaverEnabled());
    QCOMPARE(changes, std::vector<bool>({ true }));
}

void TestPowerProfileMonitor::sharedRuntimeProvidesOnePortalObserverToAllConsumers()
{
    FakePowerProfilePortal portal;
    kiriview::PowerSaverRuntime runtime(portal.runtime());
    kiriview::PowerSaverProvider provider = runtime.provider();
    std::vector<bool> firstChanges;
    std::vector<bool> secondChanges;

    std::unique_ptr<kiriview::PowerSaverStateMonitor> first
        = provider.monitor([&firstChanges](bool enabled) { firstChanges.push_back(enabled); });
    std::unique_ptr<kiriview::PowerSaverStateMonitor> second
        = provider.monitor([&secondChanges](bool enabled) { secondChanges.push_back(enabled); });

    QCOMPARE(portal.subscriptionCount, 1);
    QCOMPARE(portal.readCount, 1);
    QVERIFY(first);
    QVERIFY(second);
    portal.completeRead(0, { true });
    QCOMPARE(firstChanges, std::vector<bool>({ true }));
    QCOMPARE(secondChanges, std::vector<bool>({ true }));
    QVERIFY(first->powerSaverEnabled());
    QVERIFY(second->powerSaverEnabled());

    first.reset();
    QVariantMap changedProperties;
    changedProperties.insert(
        QStringLiteral("power-saver-enabled"), QVariant::fromValue(QDBusVariant(QVariant(false))));
    QVERIFY(portal.emitPropertiesChanged(
        QStringLiteral("org.freedesktop.portal.PowerProfileMonitor"), changedProperties, {}));

    QCOMPARE(firstChanges, std::vector<bool>({ true }));
    QCOMPARE(secondChanges, std::vector<bool>({ true, false }));
    QVERIFY(!second->powerSaverEnabled());
}

void TestPowerProfileMonitor::runtimeDefaultsFillMissingEffectsAndPreserveOverrides()
{
    int readCount = 0;
    kiriview::PowerProfileMonitorRuntime runtime;
    runtime.readPowerSaverEnabled
        = [&readCount](QObject*, kiriview::PowerProfilePortalReplyCallback callback) {
              ++readCount;
              callback(QVariantList { true });
          };

    kiriview::PowerProfileMonitorRuntime resolved
        = kiriview::powerProfileMonitorRuntimeWithDefaults(std::move(runtime));
    QVERIFY(resolved.readPowerSaverEnabled);
    QVERIFY(resolved.subscribePropertiesChanged);
    QVariantList arguments;
    resolved.readPowerSaverEnabled(
        nullptr, [&arguments](QVariantList reply) { arguments = std::move(reply); });
    QCOMPARE(arguments, QVariantList({ true }));
    QCOMPARE(readCount, 1);
}

QTEST_GUILESS_MAIN(TestPowerProfileMonitor)

#include "tst_powerprofilemonitor.moc"
