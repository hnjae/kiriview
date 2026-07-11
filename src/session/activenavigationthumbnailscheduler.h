// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILSCHEDULER_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILSCHEDULER_H

#include "session/activenavigationthumbnailjobexecutor.h"
#include "session/thumbnailimagestore.h"

#include <QHash>
#include <optional>
#include <vector>

namespace kiriview {
enum class ActiveNavigationThumbnailScheduleEffectKind {
    CancelWork,
    StartWork,
    ApplyPending,
    ApplyUnsupported,
    UpdateRetention,
    AcceptCompletion,
};

struct ActiveNavigationThumbnailScheduleEffect
{
    ActiveNavigationThumbnailScheduleEffectKind kind
        = ActiveNavigationThumbnailScheduleEffectKind::ApplyPending;
    ActiveNavigationThumbnailWorkId workId;
    ActiveNavigationThumbnailWorkRequest workRequest;
    ThumbnailSourceKey sourceKey;
    ThumbnailImageRetentionPriority retentionPriority = ThumbnailImageRetentionPriority::Background;
    ActiveNavigationThumbnailWorkCompletion completion;
    ActiveNavigationThumbnailDemandPriority demandPriority
        = ActiveNavigationThumbnailDemandPriority::Nearby;
};

class ActiveNavigationThumbnailScheduler final
{
public:
    explicit ActiveNavigationThumbnailScheduler(ThumbnailSourceAdapter sourceAdapter);

    std::vector<ActiveNavigationThumbnailScheduleEffect> reset(
        std::vector<ThumbnailSourceKey> rows, quint64 navigationGeneration);
    std::vector<ActiveNavigationThumbnailScheduleEffect> invalidate();
    std::vector<ActiveNavigationThumbnailScheduleEffect> setCurrentNumber(int currentNumber);
    bool beginDemandWindow(quint64 navigationGeneration);
    bool reportDemand(int number, const QUrl& url, ActiveNavigationThumbnailDemandBucket bucket,
        ActiveNavigationThumbnailDemandPriority priority, quint64 navigationGeneration);
    std::vector<ActiveNavigationThumbnailScheduleEffect> finishDemandWindow(
        quint64 navigationGeneration);
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
        std::optional<Demand> stagedDemand;
        std::optional<Claim> activeWork;
        std::optional<ActiveNavigationThumbnailDemandBucket> completedDemandBucket;
        std::vector<ActiveNavigationThumbnailDemandBucket> completedBackgroundBuckets;
    };

    static bool sameDemand(const Demand& left, const Demand& right);
    static bool sameDemandExceptPriority(const Demand& left, const Demand& right);
    static bool supportsGeneratedThumbnail(const ThumbnailSourceAdapterPlan& plan);
    static ThumbnailImageRetentionPriority retentionPriority(
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
    bool m_windowOpen = false;
    quint64 m_windowGeneration = 0;
    bool m_backgroundArmed = false;
    QHash<QString, std::size_t> m_rowByDemandIdentity;
    QHash<QString, std::size_t> m_rowBySourceIdentity;
};
}

#endif
