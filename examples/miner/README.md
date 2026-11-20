# Miner

**Purpose:** Build and run the SHA-256 stratum miner, with the anchor early-out on the CPU and on the device.
**Scope:** `examples/miner/`

| path                          | what it holds                                                               |
| ----------------------------- | --------------------------------------------------------------------------- |
| `core/sha256_core.c`          | the compression function, the eight-lane AVX2 arm and the anchor            |
| `core/anchor_sift.c`          | the search arms of the anchor sift and their dispatcher                     |
| `format/json_value.h`         | the JSON reader for stratum                                                 |
| `device/cuda_miner.cu`        | the device arm, v1                                                          |
| `device/v2/cuda_miner.cu`     | the device arm, v2, with a choice of scan arm                               |
| `host/btc_miner.cpp`          | the stratum client, v1                                                      |
| `host/v2/btc_miner.cpp`       | the stratum client, v2, with `version-rolling` (BIP310, BIP320)             |
| `test/kat_validation.cpp`     | known-answer tests against the NIST digests and two real blocks             |
| `build_miner.ps1`             | builds a client into `build/miner`                                          |
| `run_miner.ps1`               | starts the client, detached, with the settings in `miner.json`              |
| `relaunch_miner.ps1`          | stops the client and starts it again                                        |
| `miner_log.ps1`               | summarizes the newest log in `build/miner/logs`                             |
| `miner.example.json`          | the template for `miner.json`                                               |

## Setup

1. Build a client. The device arm needs nvcc and the Visual Studio build tools.

   ```
   pwsh examples/miner/build_miner.ps1 v2
   ```

2. Copy `miner.example.json` to `miner.json` in this directory, and fill it in. Git ignores `miner.json`.
   A pool that pays an address (ckpool, Ocean) needs `address`. An account pool (Braiins) needs `user`, and
   must not get the address. `miner_config.ps1` describes every field.

3. Start it, and read the log.

   ```
   pwsh examples/miner/run_miner.ps1
   pwsh examples/miner/miner_log.ps1
   ```

Set `dry_run` to `true` to subscribe and parse a live job with no authorize, mine or submit. The v2 client
also runs its `version-rolling` checks offline with `build/miner/btc_miner2.exe --check`.

## The known-answer tests

```
gcc -O2 -mavx2 -std=c11 -c examples/miner/core/sha256_core.c -o build/miner/sha256_core.o
g++ -O2 -mavx2 -std=c++17 -I examples/miner/core examples/miner/test/kat_validation.cpp build/miner/sha256_core.o -o build/miner/kat_validation.exe
build/miner/kat_validation.exe
```

`utils/maint/audit/audit_compiler.ps1` builds the same test three ways and requires the three outputs to agree.
