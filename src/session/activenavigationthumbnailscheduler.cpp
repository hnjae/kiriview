// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailscheduler.h"

#include <QFile>
#include <algorithm>
#include <utility>

namespace kiriview {
ThumbnailSourceAdapter defaultThumbnailSourceAdapter()
{
    return [](ThumbnailSourceAdapterRequest request) {
        const QString directImage = activeNavigationThumbnailSourceKindIdentity(
            ActiveNavigationThumbnailSourceKind::DirectImage);
        const QString directVideo = activeNavigationThumbnailSourceKindIdentity(
            ActiveNavigationThumbnailSourceKind::DirectVideo);
        if ((request.sourceKey.row.sourceKind != directImage
                && request.sourceKey.row.sourceKind != directVideo)
            || !request.sourceKey.sourceUrl.isLocalFile()) {
            return ThumbnailSourceAdapterPlan {};
        }
        const QByteArray path = QFile::encodeName(request.sourceKey.sourceUrl.toLocalFile());
        return ThumbnailSourceAdapterPlan { ThumbnailSourceAdapterPlanKind::CacheableLocalFile,
            path, ThumbnailOriginalIdentity::fromLocalPathBytes(path), {} };
    };
}

ActiveNavigationThumbnailScheduler::ActiveNavigationThumbnailScheduler(
    ThumbnailSourceAdapter sourceAdapter)
    : m_sourceAdapter(std::move(sourceAdapter))
{
}

std::vector<ActiveNavigationThumbnailScheduleEffect> ActiveNavigationThumbnailScheduler::reset(
    std::vector<ThumbnailSourceRevisionKey> rows, quint64 navigationGeneration)
{
    auto effects = invalidate();
    m_rows.reserve(rows.size());
    for (ThumbnailSourceRevisionKey& sourceKey : rows) {
        RowState state;
        state.sourceKey = std::move(sourceKey);
        const std::size_t row = m_rows.size();
        m_rowByDemandIdentity.insert(
            thumbnailDemandKey(state.sourceKey.row.rowNumber, state.sourceKey.sourceUrl,
                state.sourceKey.navigationGeneration),
            row);
        m_rowBySourceIdentity.insert(state.sourceKey, row);
        m_rows.push_back(std::move(state));
    }
    m_navigationGeneration = navigationGeneration;
    return effects;
}

std::vector<ActiveNavigationThumbnailScheduleEffect>
ActiveNavigationThumbnailScheduler::invalidate()
{
    std::vector<ActiveNavigationThumbnailScheduleEffect> effects;
    for (std::size_t row = 0; row < m_rows.size(); ++row) {
        cancel(row, effects);
    }
    m_rows.clear();
    m_rowByDemandIdentity.clear();
    m_rowBySourceIdentity.clear();
    m_navigationGeneration = 0;
    m_currentNumber = 0;
    m_backgroundArmed = false;
    return effects;
}

std::vector<ActiveNavigationThumbnailScheduleEffect>
ActiveNavigationThumbnailScheduler::setCurrentNumber(int currentNumber)
{
    m_currentNumber = currentNumber;
    std::vector<ActiveNavigationThumbnailScheduleEffect> effects;
    for (std::size_t row = 0; row < m_rows.size(); ++row) {
        RowState& state = m_rows.at(row);
        if (state.acceptedDemand.has_value()) {
            const auto retention = state.sourceKey.row.rowNumber == m_currentNumber
                ? ActiveNavigationThumbnailRetentionClass::Visible
                : retentionClass(state.acceptedDemand->priority);
            effects.emplace_back(
                ActiveNavigationThumbnailUpdateRetentionEffect { state.sourceKey, retention });
        }
        if (state.activeWork.has_value() && state.acceptedDemand.has_value()) {
            state.activeWork->tier = tierFor(row, *state.acceptedDemand);
        }
    }
    admit(effects);
    return effects;
}

std::optional<std::vector<ActiveNavigationThumbnailScheduleEffect>>
ActiveNavigationThumbnailScheduler::replaceDemandSnapshot(
    ActiveNavigationThumbnailDemandSnapshot snapshot)
{
    if (snapshot.navigationGeneration == 0
        || snapshot.navigationGeneration != m_navigationGeneration) {
        return std::nullopt;
    }
    std::vector<std::optional<ActiveNavigationThumbnailDemand>> normalized(m_rows.size());
    for (ActiveNavigationThumbnailDemand& fact : snapshot.demands) {
        if (fact.bucket < ActiveNavigationThumbnailDemandBucket::Normal
            || fact.bucket > ActiveNavigationThumbnailDemandBucket::XXLarge
            || (fact.priority != ActiveNavigationThumbnailDemandPriority::Visible
                && fact.priority != ActiveNavigationThumbnailDemandPriority::Nearby)) {
            return std::nullopt;
        }
        const auto row = rowForIdentity(fact.number, fact.url, snapshot.navigationGeneration);
        if (!row.has_value()) {
            return std::nullopt;
        }
        auto& accepted = normalized.at(*row);
        if (!accepted.has_value()) {
            accepted = std::move(fact);
            continue;
        }
        if (static_cast<int>(fact.bucket) > static_cast<int>(accepted->bucket)) {
            accepted->bucket = fact.bucket;
        }
        if (fact.priority == ActiveNavigationThumbnailDemandPriority::Visible) {
            accepted->priority = ActiveNavigationThumbnailDemandPriority::Visible;
        }
    }

    std::vector<std::optional<Demand>> demands(m_rows.size());
    for (std::size_t row = 0; row < normalized.size(); ++row) {
        if (!normalized.at(row).has_value()) {
            continue;
        }
        const ActiveNavigationThumbnailDemand& fact = *normalized.at(row);
        ThumbnailSourceAdapterPlan plan;
        if (m_sourceAdapter) {
            plan = m_sourceAdapter({ m_rows.at(row).sourceKey, fact.bucket, fact.priority });
        }
        demands.at(row)
            = Demand { m_rows.at(row).sourceKey, fact.bucket, fact.priority, std::move(plan) };
    }

    std::vector<ActiveNavigationThumbnailScheduleEffect> effects;
    m_backgroundArmed = true;
    for (std::size_t row = 0; row < m_rows.size(); ++row) {
        RowState& state = m_rows.at(row);
        if (!demands.at(row).has_value()) {
            if (state.sourceKey.row.rowNumber != m_currentNumber) {
                cancel(row, effects);
                state.acceptedDemand.reset();
                state.completedDemandBucket.reset();
                effects.emplace_back(ActiveNavigationThumbnailUpdateRetentionEffect {
                    state.sourceKey, ActiveNavigationThumbnailRetentionClass::Background });
            }
            continue;
        }
        Demand demand = std::move(*demands.at(row));
        if (state.acceptedDemand.has_value() && sameDemand(*state.acceptedDemand, demand)) {
            continue;
        }
        if (state.acceptedDemand.has_value()
            && sameDemandExceptPriority(*state.acceptedDemand, demand)) {
            state.acceptedDemand = demand;
            if (state.activeWork.has_value()) {
                state.activeWork->demand = demand;
                state.activeWork->tier = tierFor(row, demand);
            }
            effects.emplace_back(ActiveNavigationThumbnailUpdateRetentionEffect {
                state.sourceKey, retentionClass(demand.priority) });
            continue;
        }
        cancel(row, effects);
        state.acceptedDemand = demand;
        state.completedDemandBucket.reset();
        if (supportsGeneratedThumbnail(demand.sourcePlan)) {
            effects.emplace_back(ActiveNavigationThumbnailApplyPendingEffect { state.sourceKey });
        } else {
            effects.emplace_back(
                ActiveNavigationThumbnailApplyUnsupportedEffect { state.sourceKey });
            state.completedDemandBucket = demand.bucket;
        }
    }
    admit(effects);
    return std::optional<std::vector<ActiveNavigationThumbnailScheduleEffect>>(std::move(effects));
}

std::vector<ActiveNavigationThumbnailScheduleEffect>
ActiveNavigationThumbnailScheduler::acceptCompletion(
    ActiveNavigationThumbnailWorkCompletion completion)
{
    std::vector<ActiveNavigationThumbnailScheduleEffect> effects;
    const auto row = rowForSourceKey(completion.sourceKey);
    if (!row.has_value()) {
        return effects;
    }
    RowState& state = m_rows.at(*row);
    if (!state.activeWork.has_value() || state.activeWork->id != completion.workId
        || state.activeWork->demand.bucket != completion.bucket
        || state.activeWork->kind != completion.workKind) {
        return effects;
    }
    const Claim claim = *state.activeWork;
    state.activeWork.reset();
    if (claim.kind == ActiveNavigationThumbnailWorkKind::Background) {
        if (std::find(state.completedBackgroundBuckets.cbegin(),
                state.completedBackgroundBuckets.cend(), completion.bucket)
            == state.completedBackgroundBuckets.cend()) {
            state.completedBackgroundBuckets.push_back(completion.bucket);
        }
    } else {
        state.completedDemandBucket = completion.bucket;
    }
    const auto retention = claim.tier == Tier::Current
        ? ActiveNavigationThumbnailRetentionClass::Visible
        : retentionClass(claim.demand.priority);
    effects.emplace_back(
        ActiveNavigationThumbnailAcceptCompletionEffect { std::move(completion), retention });
    admit(effects);
    return effects;
}

bool ActiveNavigationThumbnailScheduler::sameDemand(const Demand& left, const Demand& right)
{
    return sameDemandExceptPriority(left, right) && left.priority == right.priority;
}

bool ActiveNavigationThumbnailScheduler::sameDemandExceptPriority(
    const Demand& left, const Demand& right)
{
    return left.sourceKey == right.sourceKey && left.bucket == right.bucket
        && left.sourcePlan.kind == right.sourcePlan.kind
        && left.sourcePlan.localPathBytes == right.sourcePlan.localPathBytes
        && left.sourcePlan.originalIdentity.uri == right.sourcePlan.originalIdentity.uri
        && left.sourcePlan.originalIdentity.localPathBytes
        == right.sourcePlan.originalIdentity.localPathBytes
        && sameOpenedCollectionScopeLocation(
            left.sourcePlan.openedCollectionScope, right.sourcePlan.openedCollectionScope);
}

bool ActiveNavigationThumbnailScheduler::supportsGeneratedThumbnail(
    const ThumbnailSourceAdapterPlan& plan)
{
    return plan.kind == ThumbnailSourceAdapterPlanKind::InMemoryOnly
        || (plan.kind == ThumbnailSourceAdapterPlanKind::CacheableLocalFile
            && !plan.localPathBytes.isEmpty())
        || (plan.kind == ThumbnailSourceAdapterPlanKind::CacheableOpenedCollectionEntry
            && !plan.openedCollectionScope.isEmpty());
}

ActiveNavigationThumbnailRetentionClass ActiveNavigationThumbnailScheduler::retentionClass(
    ActiveNavigationThumbnailDemandPriority priority)
{
    return priority == ActiveNavigationThumbnailDemandPriority::Visible
        ? ActiveNavigationThumbnailRetentionClass::Visible
        : ActiveNavigationThumbnailRetentionClass::Nearby;
}

std::vector<ActiveNavigationThumbnailDemandBucket>
ActiveNavigationThumbnailScheduler::backgroundBuckets()
{
    return { ActiveNavigationThumbnailDemandBucket::Normal,
        ActiveNavigationThumbnailDemandBucket::Large, ActiveNavigationThumbnailDemandBucket::XLarge,
        ActiveNavigationThumbnailDemandBucket::XXLarge };
}

std::optional<std::size_t> ActiveNavigationThumbnailScheduler::rowForIdentity(
    int number, const QUrl& url, quint64 generation) const
{
    const auto iterator
        = m_rowByDemandIdentity.constFind(thumbnailDemandKey(number, url, generation));
    return iterator == m_rowByDemandIdentity.cend() ? std::nullopt
                                                    : std::optional<std::size_t>(*iterator);
}

std::optional<std::size_t> ActiveNavigationThumbnailScheduler::rowForSourceKey(
    const ThumbnailSourceRevisionKey& sourceKey) const
{
    const auto iterator = m_rowBySourceIdentity.constFind(sourceKey);
    return iterator == m_rowBySourceIdentity.cend() ? std::nullopt
                                                    : std::optional<std::size_t>(*iterator);
}

ActiveNavigationThumbnailScheduler::Tier ActiveNavigationThumbnailScheduler::tierFor(
    std::size_t row, const Demand& demand) const
{
    if (m_rows.at(row).sourceKey.row.rowNumber == m_currentNumber) {
        return Tier::Current;
    }
    return demand.priority == ActiveNavigationThumbnailDemandPriority::Visible ? Tier::Visible
                                                                               : Tier::Nearby;
}

bool ActiveNavigationThumbnailScheduler::demandComplete(const RowState& state) const
{
    return state.acceptedDemand.has_value() && state.completedDemandBucket.has_value()
        && static_cast<int>(*state.completedDemandBucket)
        >= static_cast<int>(state.acceptedDemand->bucket);
}

bool ActiveNavigationThumbnailScheduler::backgroundComplete(
    const RowState& state, ActiveNavigationThumbnailDemandBucket bucket) const
{
    if (state.acceptedDemand.has_value()
        && static_cast<int>(state.acceptedDemand->bucket) >= static_cast<int>(bucket)) {
        return true;
    }
    return std::find(state.completedBackgroundBuckets.cbegin(),
               state.completedBackgroundBuckets.cend(), bucket)
        != state.completedBackgroundBuckets.cend();
}

ActiveNavigationThumbnailWorkId ActiveNavigationThumbnailScheduler::nextWorkId()
{
    if (m_nextWorkId == 0) {
        ++m_nextWorkId;
    }
    return { m_nextWorkId++ };
}

void ActiveNavigationThumbnailScheduler::cancel(
    std::size_t row, std::vector<ActiveNavigationThumbnailScheduleEffect>& effects)
{
    RowState& state = m_rows.at(row);
    if (!state.activeWork.has_value()) {
        return;
    }
    effects.emplace_back(ActiveNavigationThumbnailCancelWorkEffect { state.activeWork->id });
    state.activeWork.reset();
}

void ActiveNavigationThumbnailScheduler::start(std::size_t row,
    ActiveNavigationThumbnailWorkKind kind, Tier tier, const Demand& demand,
    std::vector<ActiveNavigationThumbnailScheduleEffect>& effects)
{
    RowState& state = m_rows.at(row);
    Claim claim { nextWorkId(), kind, demand, tier };
    state.activeWork = claim;
    effects.emplace_back(ActiveNavigationThumbnailStartWorkEffect {
        { claim.id, demand.sourceKey, demand.bucket, kind, demand.sourcePlan } });
}

void ActiveNavigationThumbnailScheduler::admit(
    std::vector<ActiveNavigationThumbnailScheduleEffect>& effects)
{
    bool foregroundExists = false;
    for (std::size_t row = 0; row < m_rows.size(); ++row) {
        const RowState& state = m_rows.at(row);
        if (!state.acceptedDemand.has_value() || demandComplete(state)) {
            continue;
        }
        const Tier tier = tierFor(row, *state.acceptedDemand);
        if (tier == Tier::Current || tier == Tier::Visible) {
            foregroundExists = true;
        }
    }
    if (foregroundExists) {
        for (std::size_t row = 0; row < m_rows.size(); ++row) {
            RowState& state = m_rows.at(row);
            if (state.activeWork.has_value()
                && (state.activeWork->tier == Tier::Nearby
                    || state.activeWork->tier == Tier::Background)) {
                cancel(row, effects);
            }
            if (!state.acceptedDemand.has_value() || demandComplete(state)
                || state.activeWork.has_value()) {
                continue;
            }
            const Tier tier = tierFor(row, *state.acceptedDemand);
            if ((tier == Tier::Current || tier == Tier::Visible)
                && supportsGeneratedThumbnail(state.acceptedDemand->sourcePlan)) {
                start(row, ActiveNavigationThumbnailWorkKind::Foreground, tier,
                    *state.acceptedDemand, effects);
            }
        }
        return;
    }
    bool demandExists = false;
    for (std::size_t row = 0; row < m_rows.size(); ++row) {
        RowState& state = m_rows.at(row);
        if (!state.acceptedDemand.has_value() || demandComplete(state)) {
            continue;
        }
        demandExists = true;
        if (!state.activeWork.has_value()
            && supportsGeneratedThumbnail(state.acceptedDemand->sourcePlan)) {
            start(row, ActiveNavigationThumbnailWorkKind::Foreground, Tier::Nearby,
                *state.acceptedDemand, effects);
        }
    }
    if (demandExists || !m_backgroundArmed) {
        if (demandExists) {
            for (std::size_t row = 0; row < m_rows.size(); ++row) {
                if (m_rows.at(row).activeWork.has_value()
                    && m_rows.at(row).activeWork->tier == Tier::Background) {
                    cancel(row, effects);
                }
            }
        }
        return;
    }
    for (const RowState& state : m_rows) {
        if (state.activeWork.has_value()) {
            return;
        }
    }
    for (auto bucket : backgroundBuckets()) {
        for (std::size_t row = 0; row < m_rows.size(); ++row) {
            RowState& state = m_rows.at(row);
            if (backgroundComplete(state, bucket)) {
                continue;
            }
            ThumbnailSourceAdapterPlan plan;
            if (m_sourceAdapter) {
                plan = m_sourceAdapter(
                    { state.sourceKey, bucket, ActiveNavigationThumbnailDemandPriority::Nearby });
            }
            if (!supportsGeneratedThumbnail(plan)) {
                continue;
            }
            Demand demand { state.sourceKey, bucket,
                ActiveNavigationThumbnailDemandPriority::Nearby, std::move(plan) };
            start(row, ActiveNavigationThumbnailWorkKind::Background, Tier::Background, demand,
                effects);
            return;
        }
    }
}
}
