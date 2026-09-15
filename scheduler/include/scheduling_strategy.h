// ============================================================
// scheduler/include/scheduling_strategy.h
// Scheduling Strategy Interface (Strategy Design Pattern)
// Owner: Rishita Ramola
//
// OS Concept: Demonstrates pluggable scheduling algorithms —
//   analogous to OS scheduling policies (FCFS, SJF, priority).
// ============================================================
#pragma once

namespace webnotifier {

/**
 * @brief Abstract base for pluggable scheduling algorithms.
 *
 * Concrete strategies implement should_check() to decide
 * whether a website is due for a health check, based on its
 * last check time and configured interval.
 */
class SchedulingStrategy {
public:
    virtual ~SchedulingStrategy() = default;

    /**
     * @param website_id   ID of the website (may be used for stateful lookup).
     * @param last_checked Unix epoch seconds of the last completed check.
     * @param interval_min Configured check interval in minutes.
     * @param now          Current Unix epoch seconds.
     * @return true if the website should be checked now.
     */
    virtual bool should_check(int       website_id,
                              long long last_checked,
                              int       interval_min,
                              long long now) const = 0;
};

// ── Concrete Strategy 1: Fixed Interval ─────────────────────────────────────
/**
 * @brief Check exactly every N minutes based on last check time.
 *
 * OS analogy: Round-Robin scheduling with fixed time quanta.
 *
 * Condition: (now - last_checked) >= interval_min * 60
 */
class FixedIntervalStrategy : public SchedulingStrategy {
public:
    bool should_check(int       website_id,
                      long long last_checked,
                      int       interval_min,
                      long long now) const override;
};

// ── Concrete Strategy 2: Adaptive (Double-Frequency on Failure) ─────────────
/**
 * @brief Increase check frequency when the site has been failing.
 *
 * OS analogy: Priority aging / dynamic priority scheduling.
 *
 * If recent_failure_rate > 50%: effective_interval = interval / 2.
 * Otherwise: falls back to FixedIntervalStrategy behaviour.
 *
 * @note The failure rate lookup requires DB access and is owned by
 *       AnalyticsEngine.  Inject a callable via constructor to keep
 *       this class testable without a live DB.
 */
class AdaptiveStrategy : public SchedulingStrategy {
public:
    /**
     * @param failure_rate_fn A callable (website_id) → failure_rate [0.0, 1.0].
     *        Defaults to always-0 (no failures), making behaviour identical
     *        to FixedIntervalStrategy until a real function is injected.
     */
    using FailureRateFn = std::function<double(int website_id)>;
    explicit AdaptiveStrategy(FailureRateFn fn = nullptr);

    bool should_check(int       website_id,
                      long long last_checked,
                      int       interval_min,
                      long long now) const override;

private:
    FailureRateFn failure_rate_fn_;
};

} // namespace webnotifier
