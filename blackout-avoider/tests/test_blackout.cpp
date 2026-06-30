// test_blackout.cpp -- automated test suite for BlackoutAvoider.
//
// Strategy: an exhaustive brute-force search is the trusted oracle ("expected");
// the dynamic program is the system under test ("actual"). Tests are data-driven
// from data/lineups.txt, so adding a lineup adds coverage automatically. No external
// framework -- a tiny CHECK macro tallies failures and the process exits nonzero on
// any failure. Build & run with:  make test
//
// NOTE: run from the repository root so data/lineups.txt resolves.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "blackout.hpp"
#include "lineups.hpp"

using namespace blackout;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        ++g_checks;                                                           \
        if (!(cond)) {                                                        \
            ++g_failures;                                                     \
            std::printf("  FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__);  \
        }                                                                     \
    } while (0)

static bool approx(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main() {
    const std::vector<Lineup> all = load_lineups("data/lineups.txt");
    if (all.empty()) {
        std::fprintf(stderr, "error: could not read data/lineups.txt (run from repo root)\n");
        return 2;
    }

    for (const auto& lp : all) {
        const std::string& name = lp.first;
        const std::vector<int>& enjoy = lp.second;

        // 1. DP optimum must equal the exhaustive-search optimum.
        auto bf = solve_brute(enjoy);
        Solver solver(enjoy);
        DPResult dp = solver.solve();
        CHECK(approx(dp.value, bf.first), ("DP matches brute force: " + name).c_str());

        // 2. The reconstructed optimal plan must be safe at every slot.
        std::vector<State> trace;
        bool ok = false;
        double replay = simulate(enjoy, dp.path, trace, ok);
        CHECK(ok, ("optimal path is feasible: " + name).c_str());
        for (const auto& s : trace) {
            CHECK(s.b <= BAC_MAX, ("BAC within ceiling: " + name).c_str());
            CHECK(s.h >= H_MIN,  ("hydration above floor: " + name).c_str());
        }

        // 3. Replaying the optimal path reproduces the DP's reported value.
        CHECK(approx(replay, dp.value), ("reconstructed value matches solver: " + name).c_str());

        // 4. The solver is deterministic.
        Solver solver2(enjoy);
        DPResult dp2 = solver2.solve();
        CHECK(approx(dp.value, dp2.value) && dp.states == dp2.states && dp.path == dp2.path,
              ("determinism: " + name).c_str());
    }

    // 5. An empty night must leave immediately; value is the start-state terminal
    //    reward (computed independently, not via brute force).
    {
        Solver solver(std::vector<int>{});
        DPResult dp = solver.solve();
        CHECK(dp.path.empty(), "empty night leaves immediately");
        CHECK(approx(dp.value, 0.4 * E0 + 0.3 * H0 - 0.05 * 0), "empty-night value correct");
    }

    // 6. A lineup that rewards only drinking must still never breach the BAC ceiling.
    {
        std::vector<int> enjoy(12, 20);
        Solver solver(enjoy);
        DPResult dp = solver.solve();
        std::vector<State> trace;
        bool ok = false;
        simulate(enjoy, dp.path, trace, ok);
        int max_bac = 0;
        for (const auto& s : trace) max_bac = std::max(max_bac, s.b);
        CHECK(max_bac <= BAC_MAX, "overdrinking is pruned");
    }

    // 7. Memoization must collapse the search: DP states < brute-force path nodes.
    {
        const std::vector<int>& enjoy = find_lineup(all, "late_peak");
        Solver solver(enjoy);
        DPResult dp = solver.solve();
        auto bf = solve_brute(enjoy);
        CHECK(dp.states < (std::size_t)bf.second, "DP explores fewer nodes than brute force");
    }

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    std::printf("%s\n", g_failures == 0 ? "ALL PASS" : "TESTS FAILED");
    return g_failures == 0 ? 0 : 1;
}
