// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailscheduler.h"

#include <QFile>
#include <algorithm>
#include <utility>

namespace kiriview {
namespace {
    QString demandIdentity(int number, const QUrl& url, quint64 generation)
    {
        return QStringLiteral("%1\x1f%2\x1f%3")
            .arg(number)
            .arg(url.toString(QUrl::FullyEncoded))
            .arg(generation);
    }

    QString sourceIdentity(const ThumbnailSourceKey& sourceKey)
    {
        return QStringLiteral("%1\x1f%2\x1f%3\x1f%4\x1f%5\x1f%6\x1f%7")
            .arg(sourceKey.rowNumber)
            .arg(sourceKey.url.toString(QUrl::FullyEncoded), sourceKey.label, sourceKey.pageKind,
                sourceKey.sourceKind, sourceKey.rowIdentity)
            .arg(sourceKey.navigationGeneration);
    }
}

ThumbnailSourceAdapter defaultThumbnailSourceAdapter()
{
    return [](ThumbnailSourceAdapterRequest request) {
        const QString directImage = activeNavigationThumbnailSourceKindIdentity(
            ActiveNavigationThumbnailSourceKind::DirectImage);
        const QString directVideo = activeNavigationThumbnailSourceKindIdentity(
            ActiveNavigationThumbnailSourceKind::DirectVideo);
        if ((request.sourceKey.sourceKind != directImage
                && request.sourceKey.sourceKind != directVideo)
            || !request.sourceKey.url.isLocalFile()) {
            return ThumbnailSourceAdapterPlan {};
        }
        const QByteArray path = QFile::encodeName(request.sourceKey.url.toLocalFile());
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
    std::vector<ThumbnailSourceKey> rows, quint64 navigationGeneration)
{
    auto effects = invalidate();
    m_rows.reserve(rows.size());
    for (ThumbnailSourceKey& sourceKey : rows) {
        RowState state;
        state.sourceKey = std::move(sourceKey);
        const std::size_t row = m_rows.size();
        m_rowByDemandIdentity.insert(demandIdentity(state.sourceKey.rowNumber, state.sourceKey.url,
                                         state.sourceKey.navigationGeneration),
            row);
        m_rowBySourceIdentity.insert(sourceIdentity(state.sourceKey), row);
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
    m_windowOpen = false;
    m_windowGeneration = 0;
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
            ActiveNavigationThumbnailScheduleEffect effect;
            effect.kind = ActiveNavigationThumbnailScheduleEffectKind::UpdateRetention;
            effect.sourceKey = state.sourceKey;
            effect.retentionPriority = state.sourceKey.rowNumber == m_currentNumber
                ? ThumbnailImageRetentionPriority::Visible
                : retentionPriority(state.acceptedDemand->priority);
            effects.push_back(std::move(effect));
        }
        if (state.activeWork.has_value() && state.acceptedDemand.has_value()) {
            state.activeWork->tier = tierFor(row, *state.acceptedDemand);
        }
    }
    admit(effects);
    return effects;
}

bool ActiveNavigationThumbnailScheduler::beginDemandWindow(quint64 navigationGeneration)
{
    if (navigationGeneration == 0 || navigationGeneration != m_navigationGeneration) {
        return false;
    }
    for (RowState& state : m_rows) {
        state.stagedDemand.reset();
    }
    m_windowOpen = true;
    m_windowGeneration = navigationGeneration;
    return true;
}

bool ActiveNavigationThumbnailScheduler::reportDemand(int number, const QUrl& url,
    ActiveNavigationThumbnailDemandBucket bucket, ActiveNavigationThumbnailDemandPriority priority,
    quint64 navigationGeneration)
{
    if (!m_windowOpen || navigationGeneration != m_windowGeneration
        || bucket == ActiveNavigationThumbnailDemandBucket::None) {
        return false;
    }
    const auto row = rowForIdentity(number, url, navigationGeneration);
    if (!row.has_value()) {
        return false;
    }
    RowState& state = m_rows.at(*row);
    ThumbnailSourceAdapterPlan plan;
    if (m_sourceAdapter) {
        plan = m_sourceAdapter({ state.sourceKey, bucket, priority });
    }
    Demand demand { state.sourceKey, bucket, priority, std::move(plan) };
    if (state.stagedDemand.has_value()) {
        if (static_cast<int>(state.stagedDemand->bucket) > static_cast<int>(demand.bucket)) {
            demand.bucket = state.stagedDemand->bucket;
            demand.sourcePlan = state.stagedDemand->sourcePlan;
        }
        if (state.stagedDemand->priority == ActiveNavigationThumbnailDemandPriority::Visible) {
            demand.priority = ActiveNavigationThumbnailDemandPriority::Visible;
        }
        if (sameDemand(*state.stagedDemand, demand)) {
            return false;
        }
    }
    state.stagedDemand = std::move(demand);
    return true;
}

std::vector<ActiveNavigationThumbnailScheduleEffect>
ActiveNavigationThumbnailScheduler::finishDemandWindow(quint64 navigationGeneration)
{
    std::vector<ActiveNavigationThumbnailScheduleEffect> effects;
    if (!m_windowOpen || navigationGeneration != m_windowGeneration) {
        return effects;
    }
    m_windowOpen = false;
    m_windowGeneration = 0;
    m_backgroundArmed = true;
    for (std::size_t row = 0; row < m_rows.size(); ++row) {
        RowState& state = m_rows.at(row);
        if (!state.stagedDemand.has_value()) {
            if (state.sourceKey.rowNumber != m_currentNumber) {
                cancel(row, effects);
                state.acceptedDemand.reset();
                state.completedDemandBucket.reset();
                ActiveNavigationThumbnailScheduleEffect effect;
                effect.kind = ActiveNavigationThumbnailScheduleEffectKind::UpdateRetention;
                effect.sourceKey = state.sourceKey;
                effect.retentionPriority = ThumbnailImageRetentionPriority::Background;
                effects.push_back(std::move(effect));
            }
            continue;
        }
        Demand demand = std::move(*state.stagedDemand);
        state.stagedDemand.reset();
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
            ActiveNavigationThumbnailScheduleEffect effect;
            effect.kind = ActiveNavigationThumbnailScheduleEffectKind::UpdateRetention;
            effect.sourceKey = state.sourceKey;
            effect.retentionPriority = retentionPriority(demand.priority);
            effects.push_back(std::move(effect));
            continue;
        }
        cancel(row, effects);
        state.acceptedDemand = demand;
        state.completedDemandBucket.reset();
        if (supportsGeneratedThumbnail(demand.sourcePlan)) {
            ActiveNavigationThumbnailScheduleEffect effect;
            effect.kind = ActiveNavigationThumbnailScheduleEffectKind::ApplyPending;
            effect.sourceKey = state.sourceKey;
            effects.push_back(std::move(effect));
        } else {
            ActiveNavigationThumbnailScheduleEffect effect;
            effect.kind = ActiveNavigationThumbnailScheduleEffectKind::ApplyUnsupported;
            effect.sourceKey = state.sourceKey;
            effects.push_back(std::move(effect));
            state.completedDemandBucket = demand.bucket;
        }
    }
    admit(effects);
    return effects;
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
    ActiveNavigationThumbnailScheduleEffect effect;
    effect.kind = ActiveNavigationThumbnailScheduleEffectKind::AcceptCompletion;
    effect.sourceKey = completion.sourceKey;
    effect.completion = std::move(completion);
    effect.demandPriority = claim.tier == Tier::Current
        ? ActiveNavigationThumbnailDemandPriority::Visible
        : claim.demand.priority;
    effects.push_back(std::move(effect));
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
    return sameThumbnailSourceKey(left.sourceKey, right.sourceKey)
        && left.sourceKey.navigationGeneration == right.sourceKey.navigationGeneration
        && left.bucket == right.bucket && left.sourcePlan.kind == right.sourcePlan.kind
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

ThumbnailImageRetentionPriority ActiveNavigationThumbnailScheduler::retentionPriority(
    ActiveNavigationThumbnailDemandPriority priority)
{
    return priority == ActiveNavigationThumbnailDemandPriority::Visible
        ? ThumbnailImageRetentionPriority::Visible
        : ThumbnailImageRetentionPriority::Nearby;
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
    const auto iterator = m_rowByDemandIdentity.constFind(demandIdentity(number, url, generation));
    return iterator == m_rowByDemandIdentity.cend() ? std::nullopt
                                                    : std::optional<std::size_t>(*iterator);
}

std::optional<std::size_t> ActiveNavigationThumbnailScheduler::rowForSourceKey(
    const ThumbnailSourceKey& sourceKey) const
{
    const auto iterator = m_rowBySourceIdentity.constFind(sourceIdentity(sourceKey));
    return iterator == m_rowBySourceIdentity.cend() ? std::nullopt
                                                    : std::optional<std::size_t>(*iterator);
}

ActiveNavigationThumbnailScheduler::Tier ActiveNavigationThumbnailScheduler::tierFor(
    std::size_t row, const Demand& demand) const
{
    if (m_rows.at(row).sourceKey.rowNumber == m_currentNumber) {
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
    ActiveNavigationThumbnailScheduleEffect effect;
    effect.kind = ActiveNavigationThumbnailScheduleEffectKind::CancelWork;
    effect.workId = state.activeWork->id;
    effects.push_back(std::move(effect));
    state.activeWork.reset();
}

void ActiveNavigationThumbnailScheduler::start(std::size_t row,
    ActiveNavigationThumbnailWorkKind kind, Tier tier, const Demand& demand,
    std::vector<ActiveNavigationThumbnailScheduleEffect>& effects)
{
    RowState& state = m_rows.at(row);
    Claim claim { nextWorkId(), kind, demand, tier };
    state.activeWork = claim;
    ActiveNavigationThumbnailScheduleEffect effect;
    effect.kind = ActiveNavigationThumbnailScheduleEffectKind::StartWork;
    effect.workId = claim.id;
    effect.workRequest = { claim.id, demand.sourceKey, demand.bucket, kind, demand.sourcePlan };
    effects.push_back(std::move(effect));
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
