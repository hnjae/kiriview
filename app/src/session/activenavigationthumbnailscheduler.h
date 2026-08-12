// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILSCHEDULER_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILSCHEDULER_H

#include "session/activenavigationthumbnailwork.h"

#include <QHash>
#include <cstddef>
#include <optional>
#include <set>
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
    ThumbnailSourceRevisionKey sourceKey;
};

struct ActiveNavigationThumbnailApplyUnsupportedEffect
{
    ThumbnailSourceRevisionKey sourceKey;
};

struct ActiveNavigationThumbnailUpdateRetentionEffect
{
    ThumbnailSourceRevisionKey sourceKey;
    ActiveNavigationThumbnailRetentionClass retentionClass
        = ActiveNavigationThumbnailRetentionClass::Background;
};

struct ActiveNavigationThumbnailAcceptCompletionEffect
{
    ActiveNavigationThumbnailWorkCompletion completion;
    ActiveNavigationThumbnailRetentionClass retentionClass
        = ActiveNavigationThumbnailRetentionClass::Background;
};

struct ActiveNavigationThumbnailScheduleContinuationEffect
{
    quint64 admissionEpoch = 0;
};

using ActiveNavigationThumbnailScheduleEffect = std::variant<
    ActiveNavigationThumbnailCancelWorkEffect, ActiveNavigationThumbnailStartWorkEffect,
    ActiveNavigationThumbnailApplyPendingEffect, ActiveNavigationThumbnailApplyUnsupportedEffect,
    ActiveNavigationThumbnailUpdateRetentionEffect, ActiveNavigationThumbnailAcceptCompletionEffect,
    ActiveNavigationThumbnailScheduleContinuationEffect>;

class ActiveNavigationThumbnailScheduler final
{
public:
    ActiveNavigationThumbnailScheduler(
        ThumbnailSourceAdapter sourceAdapter, std::size_t foregroundCapacity);

    std::optional<std::vector<ActiveNavigationThumbnailScheduleEffect>> reset(
        ActiveNavigationThumbnailSchedulingSnapshot snapshot);
    std::optional<std::vector<ActiveNavigationThumbnailScheduleEffect>> refreshRows(
        ActiveNavigationThumbnailSchedulingSnapshot snapshot);
    std::vector<ActiveNavigationThumbnailScheduleEffect> invalidate();
    std::vector<ActiveNavigationThumbnailScheduleEffect> setCurrentNumber(int currentNumber);
    std::optional<std::vector<ActiveNavigationThumbnailScheduleEffect>> replaceDemandSnapshot(
        const ActiveNavigationThumbnailDemandSnapshot& snapshot);
    std::vector<ActiveNavigationThumbnailScheduleEffect> reconcileImageResidency(
        const std::vector<ThumbnailSourceRevisionKey>& residencyLosses, bool admissionOpportunity);
    std::vector<ActiveNavigationThumbnailScheduleEffect> acceptCompletion(
        ActiveNavigationThumbnailWorkCompletion completion);
    std::vector<ActiveNavigationThumbnailScheduleEffect> acceptRetirement(
        ActiveNavigationThumbnailWorkId workId);
    std::vector<ActiveNavigationThumbnailScheduleEffect> continueAdmission(quint64 admissionEpoch);

private:
    enum class Tier {
        Current,
        Visible,
        Nearby,
        Background,
    };

    struct Demand
    {
        ThumbnailSourceRevisionKey sourceKey;
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
        ThumbnailSourceRevisionKey sourceKey;
        std::optional<Demand> acceptedDemand;
        std::optional<Claim> activeWork;
        std::optional<ActiveNavigationThumbnailDemandBucket> completedDemandBucket;
        std::vector<ActiveNavigationThumbnailDemandBucket> completedBackgroundBuckets;
        quint64 demandSnapshotEpoch = 0;
        bool residencyBlocked = false;
    };

    static bool sameDemand(const Demand& left, const Demand& right);
    static bool sameDemandExceptPriority(const Demand& left, const Demand& right);
    static bool supportsGeneratedThumbnail(const ThumbnailSourceAdapterPlan& plan);
    static ActiveNavigationThumbnailRetentionClass retentionClass(
        ActiveNavigationThumbnailDemandPriority priority);
    static std::vector<ActiveNavigationThumbnailDemandBucket> backgroundBuckets();
    [[nodiscard]] std::optional<std::size_t> rowForIdentity(
        int number, const QUrl& url, quint64 generation) const;
    [[nodiscard]] std::optional<std::size_t> rowForSourceKey(
        const ThumbnailSourceRevisionKey& sourceKey) const;
    [[nodiscard]] Tier tierFor(std::size_t row, const Demand& demand) const;
    [[nodiscard]] bool demandComplete(const RowState& state) const;
    [[nodiscard]] bool backgroundComplete(
        const RowState& state, ActiveNavigationThumbnailDemandBucket bucket) const;
    bool invalidateImageBackedCompletion(const ThumbnailSourceRevisionKey& sourceKey,
        std::vector<ActiveNavigationThumbnailScheduleEffect>& effects);
    bool releaseResidencyBlock(
        std::size_t row, std::vector<ActiveNavigationThumbnailScheduleEffect>& effects);
    void advanceAdmissionEpoch();
    void armBackgroundSweep();
    void refreshDemandTier(std::size_t row);
    void expireDemand(
        std::size_t row, std::vector<ActiveNavigationThumbnailScheduleEffect>& effects);
    void reclassifyCurrentRow(
        std::size_t row, std::vector<ActiveNavigationThumbnailScheduleEffect>& effects);
    ActiveNavigationThumbnailWorkId nextWorkId();
    [[nodiscard]] bool workIdInUse(ActiveNavigationThumbnailWorkId workId) const;
    void cancel(std::size_t row, std::vector<ActiveNavigationThumbnailScheduleEffect>& effects);
    void start(std::size_t row, ActiveNavigationThumbnailWorkKind kind, Tier tier,
        const Demand& demand, std::vector<ActiveNavigationThumbnailScheduleEffect>& effects);
    [[nodiscard]] std::size_t activeForegroundCount() const;
    void admit(std::vector<ActiveNavigationThumbnailScheduleEffect>& effects);

    ThumbnailSourceAdapter m_sourceAdapter;
    std::size_t m_foregroundCapacity = 0;
    std::vector<RowState> m_rows;
    quint64 m_navigationGeneration = 0;
    quint64 m_demandSnapshotEpoch = 0;
    quint64 m_admissionEpoch = 1;
    quint64 m_nextWorkId = 1;
    int m_currentNumber = 0;
    bool m_backgroundArmed = false;
    bool m_continuationOutstanding = false;
    std::size_t m_backgroundCursor = 0;
    std::size_t m_backgroundRemaining = 0;
    std::optional<std::size_t> m_currentRow;
    std::optional<std::size_t> m_activeBackgroundRow;
    std::set<std::size_t> m_acceptedDemandRows;
    std::set<std::size_t> m_highDemandRows;
    std::set<std::size_t> m_nearbyDemandRows;
    std::set<std::size_t> m_activeNearbyRows;
    QHash<ThumbnailDemandKey, std::size_t> m_rowByDemandIdentity;
    QHash<ThumbnailSourceRevisionKey, std::size_t> m_rowBySourceIdentity;
    QHash<int, std::size_t> m_rowByNumber;
    QHash<quint64, Claim> m_retiringWork;
};
}

#endif
