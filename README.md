# BlackoutAvoider

**Optimal stopping for a festival night under tolerance constraints.** (C++17)

A festival night is modeled as a sequence of discrete 30-minute slots, each with a
known *enjoyment value*. At every slot the attendee chooses an action — **DANCE,
DRINK, WATER, REST** — or **LEAVE**. A memoized dynamic program over the state
`(time, energy, BAC, hydration)` maximizes total enjoyment subject to **hard safety
constraints**: blood-alcohol concentration must never exceed a ceiling, and hydration
must never fall below a floor. Infeasible transitions are pruned. The stopping time is
*emergent*: `LEAVE` competes against every continuation, so the solver returns the slot
where leaving beats staying.

The central design choice is modeling consumption as a **constraint**, not an
objective — the optimizer maximizes fun while a hard BAC ceiling keeps every solution
safe.

---

## Repository layout

```
blackout-avoider/
├── README.md
├── Makefile
├── LICENSE
├── data/
│   └── lineups.txt           # representative enjoyment profiles (reproducible inputs)
├── src/
│   ├── blackout.hpp          # header-only core: DP + brute-force oracle + simulation
│   ├── lineups.hpp           # loader for data/lineups.txt
│   └── main.cpp              # CLI: solve / validate / benchmark
└── tests/
    └── test_blackout.cpp     # automated test suite (no external framework)
```

## Requirements

- A C++17 compiler (`g++` 7+ or `clang++` 5+). No third-party libraries.
- `make` (optional; the compile commands are shown below if you prefer not to use it).

## Build & run

```bash
make build       # compile the solver -> ./blackout
make test        # build and run the automated test suite
make validate    # cross-check the DP against brute force on every lineup
make run         # solve every lineup and print optimal plans
make bench       # runtime / memory benchmark on the representative lineup
make clean       # remove build artifacts
```

Without `make` (run from the repository root so `data/lineups.txt` resolves):

```bash
g++ -O2 -std=c++17 -Wall -Isrc -o blackout src/main.cpp
./blackout                 # solve all lineups
./blackout late_peak       # solve one named lineup
./blackout --validate      # DP vs brute-force check
./blackout --bench         # benchmark

g++ -O2 -std=c++17 -Wall -Isrc -o test_blackout tests/test_blackout.cpp && ./test_blackout
```

## Reproducing the reported results

All inputs live in `data/lineups.txt`; all parameters live at the top of
`src/blackout.hpp`. `make validate` regenerates the expected-vs-actual table (the DP
checked against an independent exhaustive search), and `make bench` regenerates the
runtime/memory figures. The test suite verifies optimality, per-slot safety, path
replay, and determinism across every lineup.

## Model parameters

| Symbol | Value | Meaning |
|---|---|---|
| `DRINK_BAC` | +0.027 / drink | BAC rise per standard drink (Widmark; ~75 kg, r = 0.68, 14 g ethanol) |
| `DECAY_BAC` | -0.008 / slot | metabolic elimination (0.015 / hr over a 30-min slot) |
| `BAC_MAX` | 0.120 | hard blood-alcohol ceiling |
| `H_MIN` | 2 units | hard hydration floor |
| `E0`, `EMAX` | 10 | starting / maximum energy |
| `H0`, `HMAX` | 6 / 8 | starting / maximum hydration |

Actions modify resources per slot as `(enjoyment x, dEnergy, dBAC, dHydration)`:
`DANCE (1.0, -2, 0, -1)`, `DRINK (0.6, -1, +0.027, -1)`, `WATER (0, 0, 0, +2)`,
`REST (0, +2, 0, 0)`. The terminal "go home" reward is
`0.4*energy + 0.3*hydration - 0.05*BAC`.

## Complexity

With `T` slots and energy/BAC/hydration resolutions `E`, `B`, `H`, and a constant
action set, the DP runs in **O(T*E*B*H)** time and **O(T*E*B*H)** space (reducible to
**O(E*B*H)** with a rolling layer, since transitions only advance `t -> t+1`).

## License

MIT — see [LICENSE](LICENSE).
