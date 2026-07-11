// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILSCHEDULER_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILSCHEDULER_H

#include "session/activenavigationthumbnailwork.h"

#include <QHash>
#include <optional>
#include <variant>
#include <vector>

namespace kiriview {
enum class ActiveNavigationThumbnailRetentionClass {
    Visible,
    Nearby,
    Background,
};

struct ActiveNavigationThumbnailCancelWorkEffect
{
    ActiveNavigationThumbnailWorkId workId;
};

struct ActiveNavigationThumbnailStartWorkEffect
{
    ActiveNavigationThumbnailWorkRequest request;
};

struct ActiveNavigationThumbnailApplyPendingEffect
{
    ThumbnailSourceKey sourceKey;
};

struct ActiveNavigationThumbnailApplyUnsupportedEffect
{
    ThumbnailSourceKey sourceKey;
};

struct ActiveNavigationThumbnailUpdateRetentionEffect
{
    ThumbnailSourceKey sourceKey;
    ActiveNavigationThumbnailRetentionClass retentionClass
        = ActiveNavigationThumbnailRetentionClass::Background;
};

struct ActiveNavigationThumbnailAcceptCompletionEffect
{
    ActiveNavigationThumbnailWorkCompletion completion;
    ActiveNavigationThumbnailRetentionClass retentionClass
        = ActiveNavigationThumbnailRetentionClass::Background;
};

using ActiveNavigationThumbnailScheduleEffect
    = std::variant<ActiveNavigationThumbnailCancelWorkEffect,
        ActiveNavigationThumbnailStartWorkEffect, ActiveNavigationThumbnailApplyPendingEffect,
        ActiveNavigationThumbnailApplyUnsupportedEffect,
        ActiveNavigationThumbnailUpdateRetentionEffect,
        ActiveNavigationThumbnailAcceptCompletionEffect>;

class ActiveNavigationThumbnailScheduler final
{
public:
    explicit ActiveNavigationThumbnailScheduler(ThumbnailSourceAdapter sourceAdapter);

    std::vector<ActiveNavigationThumbnailScheduleEffect> reset(
        std::vector<ThumbnailSourceKey> rows, quint64 navigationGeneration);
    std::vector<ActiveNavigationThumbnailScheduleEffect> invalidate();
    std::vector<ActiveNavigationThumbnailScheduleEffect> setCurrentNumber(int currentNumber);
    std::optional<std::vector<ActiveNavigationThumbnailScheduleEffect>> replaceDemandSnapshot(
        ActiveNavigationThumbnailDemandSnapshot snapshot);
    std::vector<ActiveNavigationThumbnailScheduleEffect> acceptCompletion(
        ActiveNavigationThumbnailWorkCompletion completion);

private:
    enum class Tier {
        Current,
        Visible,
        Nearby,
        Background,
    };

    struct Demand
    {
        ThumbnailSourceKey sourceKey;
        ActiveNavigationThumbnailDemandBucket bucket = ActiveNavigationThumbnailDemandBucket::None;
        ActiveNavigationThumbnailDemandPriority priority
            = ActiveNavigationThumbnailDemandPriority::Nearby;
        ThumbnailSourceAdapterPlan sourcePlan;
    };

    struct Claim
    {
        ActiveNavigationThumbnailWorkId id;
        ActiveNavigationThumbnailWorkKind kind = ActiveNavigationThumbnailWorkKind::Foreground;
        Demand demand;
        Tier tier = Tier::Nearby;
    };

    struct RowState
    {
        ThumbnailSourceKey sourceKey;
        std::optional<Demand> acceptedDemand;
        std::optional<Claim> activeWork;
        std::optional<ActiveNavigationThumbnailDemandBucket> completedDemandBucket;
        std::vector<ActiveNavigationThumbnailDemandBucket> completedBackgroundBuckets;
    };

    static bool sameDemand(const Demand& left, const Demand& right);
    static bool sameDemandExceptPriority(const Demand& left, const Demand& right);
    static bool supportsGeneratedThumbnail(const ThumbnailSourceAdapterPlan& plan);
    static ActiveNavigationThumbnailRetentionClass retentionClass(
        ActiveNavigationThumbnailDemandPriority priority);
    static std::vector<ActiveNavigationThumbnailDemandBucket> backgroundBuckets();
    std::optional<std::size_t> rowForIdentity(
        int number, const QUrl& url, quint64 generation) const;
    std::optional<std::size_t> rowForSourceKey(const ThumbnailSourceKey& sourceKey) const;
    Tier tierFor(std::size_t row, const Demand& demand) const;
    bool demandComplete(const RowState& state) const;
    bool backgroundComplete(
        const RowState& state, ActiveNavigationThumbnailDemandBucket bucket) const;
    ActiveNavigationThumbnailWorkId nextWorkId();
    void cancel(std::size_t row, std::vector<ActiveNavigationThumbnailScheduleEffect>& effects);
    void start(std::size_t row, ActiveNavigationThumbnailWorkKind kind, Tier tier,
        const Demand& demand, std::vector<ActiveNavigationThumbnailScheduleEffect>& effects);
    void admit(std::vector<ActiveNavigationThumbnailScheduleEffect>& effects);

    ThumbnailSourceAdapter m_sourceAdapter;
    std::vector<RowState> m_rows;
    quint64 m_navigationGeneration = 0;
    quint64 m_nextWorkId = 1;
    int m_currentNumber = 0;
    bool m_backgroundArmed = false;
    QHash<QString, std::size_t> m_rowByDemandIdentity;
    QHash<QString, std::size_t> m_rowBySourceIdentity;
};
}

#endif
