// main.cpp -- BlackoutAvoider command-line runner.
//
// Usage:
//   ./blackout                 solve every lineup in data/lineups.txt
//   ./blackout late_peak       solve a single named lineup
//   ./blackout --validate      cross-check the DP against brute force on all lineups
//   ./blackout --bench         runtime / memory benchmark on the representative lineup

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "blackout.hpp"
#include "lineups.hpp"

using namespace blackout;

static const char* DATA = "data/lineups.txt";

static std::string abbr(const std::string& a) {
    if (a == "DANCE") return "DAN";
    if (a == "DRINK") return "DRK";
    if (a == "WATER") return "WAT";
    if (a == "REST")  return "RST";
    return a;
}

static void report(const std::string& name, const std::vector<int>& enjoy) {
    Solver solver(enjoy);
    DPResult r = solver.solve();
    std::vector<State> trace;
    bool ok;
    simulate(enjoy, r.path, trace, ok);
    int max_bac = 0, min_hyd = HMAX;
    for (const auto& s : trace) {
        if (s.b > max_bac) max_bac = s.b;
        if (s.h < min_hyd) min_hyd = s.h;
    }
    std::string plan;
    for (std::size_t i = 0; i < r.path.size(); ++i)
        plan += (i ? " -> " : "") + abbr(r.path[i]);
    if (plan.empty()) plan = "(LEAVE at slot 0)";

    std::printf("[%s]  T=%zu\n", name.c_str(), enjoy.size());
    std::printf("  optimal value    : %.4f\n", r.value);
    std::printf("  optimal plan      : %s\n", plan.c_str());
    std::printf("  states evaluated  : %zu\n", r.states);
    std::printf("  peak BAC / min hyd: %.3f / %d%s\n\n",
                max_bac / 1000.0, min_hyd,
                (min_hyd <= H_MIN ? "   [hydration floor binding]" : ""));
}

static int validate(const std::vector<Lineup>& all) {
    bool ok_all = true;
    std::printf("%-16s%12s%12s%8s\n", "lineup", "expected", "actual", "match");
    for (const auto& lp : all) {
        auto bf = solve_brute(lp.second);            // independent exhaustive-search oracle
        Solver solver(lp.second);
        DPResult dp = solver.solve();
        bool match = std::fabs(bf.first - dp.value) < 1e-9;
        ok_all = ok_all && match;
        std::printf("%-16s%12.4f%12.4f%8s\n", lp.first.c_str(), bf.first, dp.value,
                    match ? "true" : "false");
    }
    std::printf("\n%s\n", ok_all ? "ALL PASS" : "FAILURES PRESENT");
    return ok_all ? 0 : 1;
}

static void bench(const std::vector<Lineup>& all, const std::string& name = "late_peak") {
    const std::vector<int>& enjoy = find_lineup(all, name);
    { Solver warm(enjoy); warm.solve(); }            // warm up

    const int runs = 1000;
    std::size_t states = 0;
    double value = 0;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < runs; ++i) {
        Solver s(enjoy);
        DPResult r = s.solve();
        value = r.value;
        states = r.states;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / runs;

    long long nominal = (long long)enjoy.size() * (EMAX + 1) * (BAC_MAX + 1) * (HMAX + 1);
    double payload_kib = states * (sizeof(long long) + sizeof(double)) / 1024.0;

    std::printf("benchmark on '%s' (T=%zu):\n", name.c_str(), enjoy.size());
    std::printf("  optimal value      : %.4f\n", value);
    std::printf("  reachable states   : %zu  (nominal T*E*B*H = %lld)\n", states, nominal);
    std::printf("  runtime (avg)      : %.3f ms over %d runs  [std::chrono, -O2]\n", ms, runs);
    std::printf("  memo payload       : %.1f KiB  (states * 16 B/entry)\n", payload_kib);
}

int main(int argc, char** argv) {
    std::vector<Lineup> all = load_lineups(DATA);
    if (all.empty()) {
        std::fprintf(stderr, "error: could not read %s (run from the repo root)\n", DATA);
        return 2;
    }

    std::string arg = (argc > 1) ? argv[1] : "";
    if (arg == "--bench") {
        bench(all);
    } else if (arg == "--validate") {
        return validate(all);
    } else if (!arg.empty()) {
        const std::vector<int>& enjoy = find_lineup(all, arg);
        if (enjoy.empty()) { std::fprintf(stderr, "unknown lineup: %s\n", arg.c_str()); return 2; }
        report(arg, enjoy);
    } else {
        for (const auto& lp : all) report(lp.first, lp.second);
    }
    return 0;
}
