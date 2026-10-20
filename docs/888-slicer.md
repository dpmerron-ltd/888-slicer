# 888 Slicer for macOS

888 Slicer is an OrcaSlicer fork with a portable JSON importer for Bambu Lab P1S
LAN printers. It retains Orca's slicer and uses its existing encrypted local
upload/print path. There is no account password, cloud token or account rebinding
in the imported configuration.

Fork source: [dpmerron-ltd/888-slicer](https://github.com/dpmerron-ltd/888-slicer).

## Install and print

1. Open the disk image for your Mac (`arm64` for Apple Silicon, `x86_64` for Intel;
   a `universal` build supports both) and drag **888 Slicer.app** to Applications.
   The build targets macOS 12 or later, subject to upstream hardware requirements.
2. On first launch, choose the Bambu Lab P1S preset with the correct nozzle size
   and install the optional Bambu network plugin when offered. Skip account login.
   The plugin is downloaded separately using Orca's existing installer; it is not
   bundled in this fork. Internet access is needed for that initial download.
3. Copy `printers.example.json` to `printers.json`. Replace the example IPs,
   serial numbers and access codes with values from your printers. Remove any
   example printer you do not need. Use different names for each printer.
4. Choose **File → Import → Import LAN Printers**, then select your JSON file.
5. Load a model, select the appropriate P1S nozzle/material settings, and slice.
   Choose **Print plate**, select the imported printer marked **LAN** with its serial number, check
   filament/AMS mapping and print options, and start the print.

Importing does not connect to new printers or start a print. Reimporting updates entries
with the same serial numbers; it does not remove other saved printers. An updated,
currently selected printer is disconnected so its next connection uses the new details. The app
remembers the imported entries across restarts, so the JSON file does not need
to remain in place. Remove an unwanted printer using Orca's normal device UI.
Connection details do not override slicing presets or nozzle/filament settings.

The Mac must be able to reach each printer on its local network. Use DHCP
reservations to keep printer IPs stable and allow local-network access when
macOS prompts. Direct printing generally needs the printer's storage card.

On firmware with Bambu authorization controls, enable **LAN-only mode and
Developer Mode on the printer**. A working status API or correct access code
alone does not establish permission to start a print. LAN-only mode disables
Bambu Handy/cloud access while enabled. The fork does not bypass those controls.
Actual firmware compatibility must be verified on a real P1S.

## Configuration

```json
{
  "version": 1,
  "printers": [
    {
      "name": "Workshop P1S",
      "model": "P1S",
      "ip": "192.168.1.50",
      "serial": "01P00EXAMPLE0001",
      "access_code": "12345678"
    }
  ]
}
```

All five printer fields are required strings. `model` must be `P1S`, the serial
must be 10–32 alphanumeric characters, and the access code must be eight
alphanumeric characters. Use the LAN access code, not a Bambu account password
or a pairing PIN.

IP addresses must be private IPv4 addresses in `10.0.0.0/8`, `172.16.0.0/12`,
`192.168.0.0/16`, or link-local `169.254.0.0/16`. Hostnames, URLs, ports and public
IP addresses are not accepted. Names, IPs and serials must be unique. Unknown
fields, duplicate JSON keys, unsupported schema versions, invalid entries, files
larger than 1 MiB and lists larger than 128 printers are rejected before import.

Access codes are secrets. The JSON and Orca's saved local-printer settings contain
them in plaintext; only distribute the file to people authorized to use those
printers. Do not commit real printer configs or include them in the app/DMG.
The repository ignores `printers.json` at its root and `*.printers.json` anywhere.
Parser errors do not echo supplied credentials.

The fork uses its own macOS bundle identity (`local.888slicer.app`) and settings
folder (`~/Library/Application Support/888Slicer/`). The settings file keeps the
upstream filename `OrcaSlicer.conf`. Upstream app-update prompts are disabled so
an Orca installer cannot replace the fork. Install new 888 Slicer builds manually.

## Build

The repository's `main` source is the baseline; `git log` records the exact
upstream revision. The slicing engine and printer protocols are unchanged.
The optional CAD tab is disabled in these builds.

```sh
brew install cmake ninja automake texinfo libtool pkgconf yasm nasm
bash scripts/build_888_macos.sh arm64
# Intel build:
bash scripts/build_888_macos.sh x86_64
# Both architectures combined:
bash scripts/build_888_macos.sh universal
```

Builds require Apple's command-line development tools, internet access and
substantial free disk space. `BUILD_JOBS` controls compilation parallelism
(default 4). The `.dmg` is written to `dist/`; the script also reports the
temporary staged `.app` path, kept outside synced folders for reliable signing.
To repackage an already built app, use `bash scripts/package_888_macos.sh arm64`.
GitHub Actions also provides a manually triggered **Build 888 Slicer for macOS**
workflow that produces separate Apple Silicon and Intel disk images.

Local builds are ad-hoc signed, not Apple-notarized. Gatekeeper may require
approval in System Settings → Privacy & Security on another Mac.
For trusted distribution, set `MACOS_SIGN_IDENTITY` to your Developer ID
Application identity and optionally `MACOS_NOTARY_PROFILE` to a configured
`notarytool` Keychain profile when packaging. No signing credentials are stored
in the repository.

## Verification

Run the importer tests without building the full slicer:

```sh
cmake -S tests/lan_printer_config -B build/lan-printer-config -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/lan-printer-config --parallel 4
ctest --test-dir build/lan-printer-config -C Release --output-on-failure
```

The same parser cases are included in the normal `libslic3r_tests` suite.
`slic3rutils_tests` includes device-manager tests for explicit Bambu agent
assignment, updated credentials/IP/name, and import before plugin installation.
Real printing still needs an integration test against an authorized P1S:
import → restart → slice → select printer → upload → verify the print starts.

## Upstream and license

Based on [OrcaSlicer](https://github.com/OrcaSlicer/OrcaSlicer), distributed under
AGPL-3.0. Keep its notices and provide corresponding source when distributing
modified binaries. The optional Bambu networking plugin has separate terms;
this package leaves its download/installation to upstream's normal plugin flow.
