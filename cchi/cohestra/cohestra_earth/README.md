# cohestra_earth

Pure-C++ CCHI harness — no Verilator. Binds the cohestra instance kernel
directly to `EarthModel`, a software CCHI home node (I/S/U directory, memory,
snoop generation with EVT>SNP>REQ priority, configurable
parallelism/latencies, denial counters) behind `EarthCCHIInterface`. Runs the
same stimulators (FuzzARI, Core2Core) and checkers (CacheLineDataMonitor,
JointProfiler, SimPerfMeter) as `cohestra_v3`. Not wired with the transaction
watchdog; the Earth model does not service CMO requests, so the bundled
configurations keep `fuzz.ari.cmo.enable = 0`.

## Build

```bash
cd cchi/cohestra/cohestra_earth
cmake -S . -B build
cmake --build build -j$(nproc)
```

## Command-line arguments

Usage: `cohestra_earth [--config <path>] [--cycles <n>] [--ports <n>] [--verbose] [--help]`

| Argument | Default | Meaning |
|---|---|---|
| `-h`, `--help` | — | Print the usage line and exit (code 0). |
| `--config <path>` | (none) | INI configuration file; without it the executable runs on compiled-in defaults. |
| `--cycles <n>` | `10000000` | Cycle limit / stop condition. |
| `--ports <n>` | `2` | Number of Type-1 upstream ports to instantiate, 1..8. Must cover the node IDs listed by the config's `upstream.type1.nodeid` and stimulator upstreams. |
| `-v`, `--verbose` | off | Debug-level logging. |

Exit codes: `0` completed; `1` argument or startup failure (unknown/invalid argument, unreadable or rejected configuration).

## Configurations

| File | Purpose |
|---|---|
| `config.ini` | Reference configuration; 2 upstreams (node IDs 0, 1). |
| `config_4p.ini` | 4-port configuration; 4 upstreams (node IDs 0..3) — run with `--ports 4`. |
| `config_stress.ini` | Stress configuration; 2 upstreams (node IDs 0, 1). |

## Examples

```bash
./build/cohestra_earth --config config.ini --cycles 1000000
./build/cohestra_earth --config config_4p.ini --ports 4 --cycles 10000000
./build/cohestra_earth --config config_stress.ini --ports 2 --verbose
```
