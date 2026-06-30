// blackout.hpp -- BlackoutAvoider core solver (header-only).
//
// Models a festival night as a sequence of discrete 30-minute slots, each with a
// known enjoyment value. At every slot the attendee picks an action (DANCE, DRINK,
// WATER, REST) or LEAVE. A memoized dynamic program over the state
//
//         (t, energy, BAC, hydration)
//
// maximizes total enjoyment subject to HARD safety constraints: blood-alcohol must
// never exceed a ceiling and hydration must never fall below a floor. Infeasible
// transitions are pruned. The optimal stopping time is emergent -- LEAVE competes
// against every continuation, so the solver returns the slot where leaving wins.
//
// All resource quantities are integers, so the state space is finite and hashable.
#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace blackout {

// --------------------------------------------------------------------------- //
// Model parameters (justified in README.md)
// --------------------------------------------------------------------------- //
constexpr int DRINK_BAC = 27;   // +0.027 BAC per standard drink (Widmark; ~75 kg, r=0.68, 14 g EtOH)
constexpr int DECAY_BAC = 8;    // -0.008 BAC per 30-min slot (0.015 / hr metabolic elimination)
constexpr int BAC_MAX   = 120;  // hard safety ceiling = 0.120 ("medical-tent line")
constexpr int E0 = 10, EMAX = 10;   // starting / maximum energy
constexpr int H0 = 6,  HMAX = 8;    // starting / maximum hydration
constexpr int H_MIN = 2;            // hard hydration floor

// (enjoyment multiplier on slot value, d_energy, d_bac, d_hydration)
struct Action { const char* name; double mult; int de, db, dh; };

inline const std::vector<Action>& actions() {
    static const std::vector<Action> A = {
        {"DANCE", 1.0, -2,        0, -1},
        {"DRINK", 0.6, -1, DRINK_BAC, -1},
        {"WATER", 0.0,  0,        0, +2},
        {"REST",  0.0, +2,        0,  0},
    };
    return A;
}

// Value of going home now: reward leftover energy/hydration, penalize residual BAC
// (a "how you feel tomorrow" proxy). This is what makes stopping a real choice.
inline double terminal(int e, int b, int h) { return 0.4 * e + 0.3 * h - 0.05 * b; }

// Apply one action. Writes the next state and returns whether it is feasible:
// energy >= 0, BAC <= ceiling, hydration >= floor.
inline bool step(int e, int b, int h, const Action& a, int& ne, int& nb, int& nh) {
    ne = std::min(EMAX, e + a.de);
    nb = std::max(0,     b + a.db - DECAY_BAC);   // a drink raises BAC; metabolism decays it
    nh = std::min(HMAX, h + a.dh);
    return (ne >= 0 && nb <= BAC_MAX && nh >= H_MIN);
}

struct DPResult {
    double value;                       // optimal total enjoyment
    std::size_t states;                 // distinct states evaluated (memo size)
    std::vector<std::string> path;      // optimal action sequence (empty => leave at slot 0)
};

// Exact solver via top-down DP with memoization.
class Solver {
public:
    explicit Solver(const std::vector<int>& enjoy)
        : enjoy_(enjoy), T_(static_cast<int>(enjoy.size())) {}

    DPResult solve() {
        memo_.clear();
        choice_.clear();
        double value = V(0, E0, 0, H0);

        // Reconstruct the optimal action sequence by walking the recorded choices.
        std::vector<std::string> path;
        int t = 0, e = E0, b = 0, h = H0;
        while (t < T_) {
            const std::string& act = choice_[key(t, e, b, h)];
            if (act == "LEAVE") break;
            path.push_back(act);
            int ne, nb, nh;
            apply(e, b, h, act, ne, nb, nh);
            e = ne; b = nb; h = nh; ++t;
        }
        return {value, memo_.size(), path};
    }

private:
    const std::vector<int>& enjoy_;
    int T_;
    std::unordered_map<long long, double> memo_;
    std::unordered_map<long long, std::string> choice_;

    // Pack (t,e,b,h) into one key; ranges are bounded, so this is collision-free.
    static long long key(int t, int e, int b, int h) {
        return (((long long)t * (EMAX + 1) + e) * (BAC_MAX + 1) + b) * (HMAX + 1) + h;
    }

    static void apply(int e, int b, int h, const std::string& name,
                      int& ne, int& nb, int& nh) {
        for (const auto& a : actions())
            if (name == a.name) { step(e, b, h, a, ne, nb, nh); return; }
    }

    double V(int t, int e, int b, int h) {
        long long k = key(t, e, b, h);
        auto it = memo_.find(k);
        if (it != memo_.end()) return it->second;     // reuse solved subproblem

        double best = terminal(e, b, h);              // option: LEAVE now (emergent stopping)
        std::string best_act = "LEAVE";
        if (t < T_) {
            for (const auto& a : actions()) {          // else weigh DANCE / DRINK / WATER / REST
                int ne, nb, nh;
                if (!step(e, b, h, a, ne, nb, nh)) continue;   // prune unsafe branch
                double val = enjoy_[t] * a.mult + V(t + 1, ne, nb, nh);
                if (val > best) { best = val; best_act = a.name; }
            }
        }
        choice_[k] = best_act;
        memo_[k] = best;
        return best;
    }
};

// Exhaustive search over every feasible action sequence -- the trusted oracle used
// to validate the DP. Returns {optimal_value, number_of_path_nodes_explored}.
inline std::pair<double, long long> solve_brute(const std::vector<int>& enjoy) {
    int T = static_cast<int>(enjoy.size());
    long long nodes = 0;
    std::function<double(int, int, int, int, double)> rec =
        [&](int t, int e, int b, int h, double acc) -> double {
            ++nodes;
            double best = acc + terminal(e, b, h);          // option: LEAVE now
            if (t < T)
                for (const auto& a : actions()) {
                    int ne, nb, nh;
                    if (!step(e, b, h, a, ne, nb, nh)) continue;
                    best = std::max(best, rec(t + 1, ne, nb, nh, acc + enjoy[t] * a.mult));
                }
            return best;
        };
    double v = rec(0, E0, 0, H0, 0.0);
    return {v, nodes};
}

struct State { int t, e, b, h; };

// Replay an action path from the start state. Fills `trace`, returns the total value,
// and sets `ok=false` if any step is infeasible. Used by the test suite to verify safety.
inline double simulate(const std::vector<int>& enjoy, const std::vector<std::string>& path,
                       std::vector<State>& trace, bool& ok) {
    ok = true;
    int e = E0, b = 0, h = H0;
    trace.clear();
    trace.push_back({0, e, b, h});
    double total = 0.0;
    for (int t = 0; t < static_cast<int>(path.size()); ++t) {
        const Action* chosen = nullptr;
        for (const auto& a : actions())
            if (path[t] == a.name) { chosen = &a; break; }
        total += enjoy[t] * chosen->mult;
        int ne, nb, nh;
        if (!step(e, b, h, *chosen, ne, nb, nh)) { ok = false; return total; }
        e = ne; b = nb; h = nh;
        trace.push_back({t + 1, e, b, h});
    }
    total += terminal(e, b, h);
    return total;
}

}  // namespace blackout
