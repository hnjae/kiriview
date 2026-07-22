// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailscheduler.h"

#include <QFile>
#include <QSet>
#include <algorithm>
#include <limits>
#include <map>
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
    ThumbnailSourceAdapter sourceAdapter, std::size_t foregroundCapacity)
    : m_sourceAdapter(std::move(sourceAdapter))
    , m_foregroundCapacity(foregroundCapacity)
{
}

std::optional<std::vector<ActiveNavigationThumbnailScheduleEffect>>
ActiveNavigationThumbnailScheduler::reset(ActiveNavigationThumbnailSchedulingSnapshot snapshot)
{
    if (snapshot.navigationGeneration == 0) {
        return std::nullopt;
    }
    QSet<ThumbnailSourceRevisionKey> sourceIdentities;
    QSet<ThumbnailDemandKey> demandIdentities;
    for (const ThumbnailSourceRevisionKey& sourceKey : snapshot.rows) {
        const ThumbnailDemandKey demandIdentity = thumbnailDemandKey(
            sourceKey.row.rowNumber, sourceKey.sourceUrl, sourceKey.navigationGeneration);
        if (!isValidThumbnailSourceRevisionKey(sourceKey)
            || sourceKey.navigationGeneration != snapshot.navigationGeneration
            || !isValidThumbnailDemandKey(demandIdentity) || sourceIdentities.contains(sourceKey)
            || demandIdentities.contains(demandIdentity)) {
            return std::nullopt;
        }
        sourceIdentities.insert(sourceKey);
        demandIdentities.insert(demandIdentity);
    }
    auto effects = invalidate();
    m_rows.reserve(snapshot.rows.size());
    for (ThumbnailSourceRevisionKey& sourceKey : snapshot.rows) {
        RowState state;
        state.sourceKey = std::move(sourceKey);
        const std::size_t row = m_rows.size();
        m_rowByDemandIdentity.insert(
            thumbnailDemandKey(state.sourceKey.row.rowNumber, state.sourceKey.sourceUrl,
                state.sourceKey.navigationGeneration),
            row);
        m_rowBySourceIdentity.insert(state.sourceKey, row);
        m_rowByNumber.insert(state.sourceKey.row.rowNumber, row);
        m_rows.push_back(std::move(state));
    }
    m_navigationGeneration = snapshot.navigationGeneration;
    return std::optional<std::vector<ActiveNavigationThumbnailScheduleEffect>>(std::move(effects));
}

bool ActiveNavigationThumbnailScheduler::refreshRows(
    ActiveNavigationThumbnailSchedulingSnapshot snapshot)
{
    if (snapshot.navigationGeneration == 0
        || snapshot.navigationGeneration != m_navigationGeneration
        || snapshot.rows.size() != m_rows.size()) {
        return false;
    }
    for (std::size_t row = 0; row < snapshot.rows.size(); ++row) {
        if (!isValidThumbnailSourceRevisionKey(snapshot.rows.at(row))
            || snapshot.rows.at(row) != m_rows.at(row).sourceKey) {
            return false;
        }
    }
    for (std::size_t row = 0; row < snapshot.rows.size(); ++row) {
        RowState& state = m_rows.at(row);
        state.sourceKey = std::move(snapshot.rows.at(row));
        if (state.acceptedDemand.has_value()) {
            state.acceptedDemand->sourceKey = state.sourceKey;
        }
        if (state.activeWork.has_value()) {
            state.activeWork->demand.sourceKey = state.sourceKey;
        }
    }
    return true;
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
    m_rowByNumber.clear();
    m_acceptedDemandRows.clear();
    m_highDemandRows.clear();
    m_nearbyDemandRows.clear();
    m_activeNearbyRows.clear();
    m_currentRow.reset();
    m_activeBackgroundRow.reset();
    m_navigationGeneration = 0;
    m_demandSnapshotEpoch = 0;
    m_currentNumber = 0;
    m_backgroundArmed = false;
    m_backgroundCursor = 0;
    m_backgroundRemaining = 0;
    m_continuationOutstanding = false;
    advanceAdmissionEpoch();
    return effects;
}

std::vector<ActiveNavigationThumbnailScheduleEffect>
ActiveNavigationThumbnailScheduler::setCurrentNumber(int currentNumber)
{
    std::vector<ActiveNavigationThumbnailScheduleEffect> effects;
    const std::optional<std::size_t> previousCurrentRow = m_currentRow;
    const auto nextCurrent = m_rowByNumber.constFind(currentNumber);
    m_currentNumber = currentNumber;
    m_currentRow = nextCurrent == m_rowByNumber.cend() ? std::nullopt
                                                       : std::optional<std::size_t>(*nextCurrent);
    if (previousCurrentRow == m_currentRow) {
        admit(effects);
        return effects;
    }
    if (previousCurrentRow.has_value()) {
        RowState& previous = m_rows.at(*previousCurrentRow);
        if (previous.acceptedDemand.has_value()
            && previous.demandSnapshotEpoch != m_demandSnapshotEpoch) {
            expireDemand(*previousCurrentRow, effects);
        } else {
            reclassifyCurrentRow(*previousCurrentRow, effects);
        }
    }
    if (m_currentRow.has_value()) {
        reclassifyCurrentRow(*m_currentRow, effects);
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
    std::map<std::size_t, ActiveNavigationThumbnailDemand> normalized;
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
        auto [iterator, inserted] = normalized.try_emplace(*row, std::move(fact));
        if (inserted) {
            continue;
        }
        ActiveNavigationThumbnailDemand& accepted = iterator->second;
        if (static_cast<int>(fact.bucket) > static_cast<int>(accepted.bucket)) {
            accepted.bucket = fact.bucket;
        }
        if (fact.priority == ActiveNavigationThumbnailDemandPriority::Visible) {
            accepted.priority = ActiveNavigationThumbnailDemandPriority::Visible;
        }
    }

    std::map<std::size_t, Demand> demands;
    for (const auto& [row, fact] : normalized) {
        ThumbnailSourceAdapterPlan plan;
        if (m_sourceAdapter) {
            plan = m_sourceAdapter({ m_rows.at(row).sourceKey, fact.bucket, fact.priority });
        }
        demands.emplace(
            row, Demand { m_rows.at(row).sourceKey, fact.bucket, fact.priority, std::move(plan) });
    }

    std::vector<ActiveNavigationThumbnailScheduleEffect> effects;
    ++m_demandSnapshotEpoch;
    if (m_demandSnapshotEpoch == 0) {
        ++m_demandSnapshotEpoch;
    }
    std::set<std::size_t> missingRows = m_acceptedDemandRows;
    for (auto& [row, demand] : demands) {
        missingRows.erase(row);
        RowState& state = m_rows.at(row);
        state.demandSnapshotEpoch = m_demandSnapshotEpoch;
        m_acceptedDemandRows.insert(row);
        if (state.acceptedDemand.has_value() && sameDemand(*state.acceptedDemand, demand)) {
            refreshDemandTier(row);
            continue;
        }
        if (state.acceptedDemand.has_value()
            && sameDemandExceptPriority(*state.acceptedDemand, demand)) {
            state.acceptedDemand = demand;
            if (state.activeWork.has_value()) {
                state.activeWork->demand = demand;
            }
            reclassifyCurrentRow(row, effects);
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
        refreshDemandTier(row);
    }
    for (const std::size_t row : missingRows) {
        if (m_currentRow.has_value() && row == *m_currentRow) {
            continue;
        }
        expireDemand(row, effects);
    }
    armBackgroundSweep();
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
    if (claim.tier == Tier::Nearby) {
        m_activeNearbyRows.erase(*row);
    } else if (claim.tier == Tier::Background) {
        m_activeBackgroundRow.reset();
    }
    state.activeWork.reset();
    if (claim.kind == ActiveNavigationThumbnailWorkKind::Background) {
        if (!std::ranges::contains(state.completedBackgroundBuckets, completion.bucket)) {
            state.completedBackgroundBuckets.push_back(completion.bucket);
        }
    } else {
        state.completedDemandBucket = completion.bucket;
        refreshDemandTier(*row);
    }
    const auto retention = claim.tier == Tier::Current
        ? ActiveNavigationThumbnailRetentionClass::Visible
        : retentionClass(claim.demand.priority);
    effects.emplace_back(
        ActiveNavigationThumbnailAcceptCompletionEffect { std::move(completion), retention });
    admit(effects);
    return effects;
}

std::vector<ActiveNavigationThumbnailScheduleEffect>
ActiveNavigationThumbnailScheduler::continueAdmission(quint64 admissionEpoch)
{
    if (admissionEpoch == 0 || admissionEpoch != m_admissionEpoch || !m_continuationOutstanding) {
        return {};
    }
    m_continuationOutstanding = false;
    std::vector<ActiveNavigationThumbnailScheduleEffect> effects;
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
    return std::ranges::contains(state.completedBackgroundBuckets, bucket);
}

void ActiveNavigationThumbnailScheduler::advanceAdmissionEpoch()
{
    ++m_admissionEpoch;
    if (m_admissionEpoch == 0) {
        ++m_admissionEpoch;
    }
}

void ActiveNavigationThumbnailScheduler::armBackgroundSweep()
{
    const std::size_t bucketCount = backgroundBuckets().size();
    if (m_rows.empty() || bucketCount == 0
        || m_rows.size() > std::numeric_limits<std::size_t>::max() / bucketCount) {
        m_backgroundArmed = false;
        m_backgroundRemaining = 0;
        return;
    }
    const std::size_t candidateCount = m_rows.size() * bucketCount;
    m_backgroundArmed = candidateCount != 0;
    m_backgroundRemaining = candidateCount;
    if (candidateCount != 0) {
        m_backgroundCursor %= candidateCount;
    }
}

void ActiveNavigationThumbnailScheduler::refreshDemandTier(std::size_t row)
{
    m_highDemandRows.erase(row);
    m_nearbyDemandRows.erase(row);
    const RowState& state = m_rows.at(row);
    if (!state.acceptedDemand.has_value() || demandComplete(state)) {
        return;
    }
    const Tier tier = tierFor(row, *state.acceptedDemand);
    if (tier == Tier::Current || tier == Tier::Visible) {
        m_highDemandRows.insert(row);
    } else {
        m_nearbyDemandRows.insert(row);
    }
}

void ActiveNavigationThumbnailScheduler::expireDemand(
    std::size_t row, std::vector<ActiveNavigationThumbnailScheduleEffect>& effects)
{
    RowState& state = m_rows.at(row);
    if (!state.acceptedDemand.has_value()) {
        return;
    }
    cancel(row, effects);
    state.acceptedDemand.reset();
    state.completedDemandBucket.reset();
    state.demandSnapshotEpoch = 0;
    m_acceptedDemandRows.erase(row);
    refreshDemandTier(row);
    effects.emplace_back(ActiveNavigationThumbnailUpdateRetentionEffect {
        state.sourceKey, ActiveNavigationThumbnailRetentionClass::Background });
}

void ActiveNavigationThumbnailScheduler::reclassifyCurrentRow(
    std::size_t row, std::vector<ActiveNavigationThumbnailScheduleEffect>& effects)
{
    RowState& state = m_rows.at(row);
    if (!state.acceptedDemand.has_value()) {
        return;
    }
    const Tier tier = tierFor(row, *state.acceptedDemand);
    if (state.activeWork.has_value()
        && state.activeWork->kind == ActiveNavigationThumbnailWorkKind::Foreground) {
        m_activeNearbyRows.erase(row);
        state.activeWork->tier = tier;
        if (tier == Tier::Nearby) {
            m_activeNearbyRows.insert(row);
        }
    }
    refreshDemandTier(row);
    effects.emplace_back(ActiveNavigationThumbnailUpdateRetentionEffect { state.sourceKey,
        tier == Tier::Current ? ActiveNavigationThumbnailRetentionClass::Visible
                              : retentionClass(state.acceptedDemand->priority) });
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
    if (state.activeWork->tier == Tier::Nearby) {
        m_activeNearbyRows.erase(row);
    } else if (state.activeWork->tier == Tier::Background) {
        m_activeBackgroundRow.reset();
    }
    state.activeWork.reset();
}

void ActiveNavigationThumbnailScheduler::start(std::size_t row,
    ActiveNavigationThumbnailWorkKind kind, Tier tier, const Demand& demand,
    std::vector<ActiveNavigationThumbnailScheduleEffect>& effects)
{
    RowState& state = m_rows.at(row);
    Claim claim { nextWorkId(), kind, demand, tier };
    state.activeWork = claim;
    if (tier == Tier::Nearby) {
        m_activeNearbyRows.insert(row);
    } else if (tier == Tier::Background) {
        m_activeBackgroundRow = row;
    }
    effects.emplace_back(ActiveNavigationThumbnailStartWorkEffect {
        { claim.id, demand.sourceKey, demand.bucket, kind, demand.sourcePlan } });
}

std::size_t ActiveNavigationThumbnailScheduler::activeForegroundCount() const
{
    return static_cast<std::size_t>(
        std::count_if(m_rows.cbegin(), m_rows.cend(), [](const RowState& state) {
            return state.activeWork.has_value()
                && state.activeWork->kind == ActiveNavigationThumbnailWorkKind::Foreground;
        }));
}

void ActiveNavigationThumbnailScheduler::admit(
    std::vector<ActiveNavigationThumbnailScheduleEffect>& effects)
{
    if (!m_highDemandRows.empty()) {
        const std::set<std::size_t> activeNearby = m_activeNearbyRows;
        for (const std::size_t row : activeNearby) {
            cancel(row, effects);
        }
        if (m_activeBackgroundRow.has_value()) {
            cancel(*m_activeBackgroundRow, effects);
        }

        std::size_t activeForeground = activeForegroundCount();
        if (m_currentRow.has_value()
            && m_highDemandRows.find(*m_currentRow) != m_highDemandRows.cend()) {
            RowState& currentState = m_rows.at(*m_currentRow);
            if (!currentState.activeWork.has_value() && currentState.acceptedDemand.has_value()
                && supportsGeneratedThumbnail(currentState.acceptedDemand->sourcePlan)
                && m_foregroundCapacity != 0) {
                while (activeForeground >= m_foregroundCapacity) {
                    const auto activeVisible = std::ranges::find_if(m_highDemandRows.crbegin(),
                        m_highDemandRows.crend(), [this](std::size_t row) {
                            const RowState& state = m_rows.at(row);
                            return (!m_currentRow.has_value() || row != *m_currentRow)
                                && state.activeWork.has_value()
                                && state.activeWork->tier == Tier::Visible;
                        });
                    if (activeVisible == m_highDemandRows.crend()) {
                        break;
                    }
                    cancel(*activeVisible, effects);
                    --activeForeground;
                }
                if (activeForeground < m_foregroundCapacity) {
                    start(*m_currentRow, ActiveNavigationThumbnailWorkKind::Foreground,
                        Tier::Current, *currentState.acceptedDemand, effects);
                    ++activeForeground;
                }
            }
        }
        for (const std::size_t row : m_highDemandRows) {
            if (m_currentRow.has_value() && row == *m_currentRow) {
                continue;
            }
            if (activeForeground >= m_foregroundCapacity) {
                break;
            }
            RowState& state = m_rows.at(row);
            if (!state.activeWork.has_value() && state.acceptedDemand.has_value()
                && supportsGeneratedThumbnail(state.acceptedDemand->sourcePlan)) {
                start(row, ActiveNavigationThumbnailWorkKind::Foreground,
                    tierFor(row, *state.acceptedDemand), *state.acceptedDemand, effects);
                ++activeForeground;
            }
        }
        return;
    }
    if (!m_nearbyDemandRows.empty()) {
        if (m_activeBackgroundRow.has_value()) {
            cancel(*m_activeBackgroundRow, effects);
        }
        std::size_t activeForeground = activeForegroundCount();
        for (const std::size_t row : m_nearbyDemandRows) {
            if (activeForeground >= m_foregroundCapacity) {
                break;
            }
            RowState& state = m_rows.at(row);
            if (!state.activeWork.has_value() && state.acceptedDemand.has_value()
                && supportsGeneratedThumbnail(state.acceptedDemand->sourcePlan)) {
                start(row, ActiveNavigationThumbnailWorkKind::Foreground, Tier::Nearby,
                    *state.acceptedDemand, effects);
                ++activeForeground;
            }
        }
        return;
    }
    if (!m_backgroundArmed || m_activeBackgroundRow.has_value() || activeForegroundCount() != 0) {
        return;
    }
    constexpr std::size_t BackgroundScanBudget = 16;
    const auto buckets = backgroundBuckets();
    const std::size_t candidateCount = m_rows.size() * buckets.size();
    std::size_t scanned = 0;
    while (m_backgroundRemaining != 0 && scanned < BackgroundScanBudget) {
        const std::size_t candidate = m_backgroundCursor;
        m_backgroundCursor = (m_backgroundCursor + 1) % candidateCount;
        --m_backgroundRemaining;
        ++scanned;
        const std::size_t row = candidate % m_rows.size();
        const auto bucket = buckets.at(candidate / m_rows.size());
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
        Demand demand { state.sourceKey, bucket, ActiveNavigationThumbnailDemandPriority::Nearby,
            std::move(plan) };
        start(
            row, ActiveNavigationThumbnailWorkKind::Background, Tier::Background, demand, effects);
        return;
    }
    if (m_backgroundRemaining == 0) {
        m_backgroundArmed = false;
        return;
    }
    if (!m_continuationOutstanding) {
        m_continuationOutstanding = true;
        effects.emplace_back(
            ActiveNavigationThumbnailScheduleContinuationEffect { m_admissionEpoch });
    }
}
}
