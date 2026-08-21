# aic8800dc — Manual Build Steps

## Prerequisites

Install kernel headers and a C toolchain:

```bash
sudo apt install linux-headers-$(uname -r) build-essential dwarves sparse cppcheck
```

## Build

```bash
cd /home/aramirez/aramirez-Projects/WIFI-6-aic8800dc/aic8800dc/drivers/aic8800;

make clean;

COMPILER=$(grep LINUX_COMPILER /usr/src/linux-headers-$(uname -r)/include/generated/compile.h | sed -re 's/^.*\"([^ ]+) .*$/\1/');
echo "Compiler to use: ${COMPILER}";

make CC=${COMPILER};
```

This produces:

- `aic8800_fdrv/aic8800_fdrv.ko`
- `aic_load_fw/aic_load_fw.ko`

## Build Options

Configured in `drivers/aic8800/aic8800_fdrv/Makefile`; override on the command line if needed:

| Variable | Default | Meaning |
|---|---|---|
| `CONFIG_AIC8800D80` | `n` | 8800D80/D81 chip compat layer. Off by default (this is a DC/DW tree). Set `=y` **only** if the adapter enumerates as 8800D80/D81, otherwise it won't init. |
| `CONFIG_RFTEST` | `n` | RF test commands for the `aicrf_test` tool. Off by default; enable only for RF bring-up. |

```bash
# Example: build for an 8800D80/D81 device
make CC=${COMPILER} CONFIG_AIC8800D80=y
```

## Install

> Only required if you want to load the driver on boot via `modprobe`.

### Firmware blobs (one-time, before first load)

```bash
sudo cp -r /home/aramirez/aramirez-Projects/WIFI-6-aic8800dc/aic8800dc/fw/aic8800DC /lib/firmware/
```

### udev rule (one-time)

Ejects the chipset's mass-storage personality so it switches to WiFi mode:

```bash
sudo cp /home/aramirez/aramirez-Projects/WIFI-6-aic8800dc/aic8800dc/aic.rules /lib/udev/rules.d/
sudo udevadm control --reload-rules
```

### Modules (after every kernel upgrade or driver rebuild)

```bash
cd /home/aramirez/aramirez-Projects/WIFI-6-aic8800dc/aic8800dc/drivers/aic8800
sudo make install
```

Installs both `.ko` files under `/lib/modules/$(uname -r)/kernel/drivers/net/wireless/aic8800/` and runs `depmod`.

## Load and Verify

```bash
sudo modprobe aic8800_fdrv
lsmod | grep aic
iwconfig                # or: ip link
dmesg | tail -50
```

## Rebuild After a Kernel Upgrade

```bash
cd /home/aramirez/aramirez-Projects/WIFI-6-aic8800dc/aic8800dc/drivers/aic8800;

make clean;

COMPILER=$(grep LINUX_COMPILER /usr/src/linux-headers-$(uname -r)/include/generated/compile.h | sed -re 's/^.*\"([^ ]+) .*$/\1/');
echo "Compiler to use: ${COMPILER}";

make CC=${COMPILER};

sudo make install;

sudo modprobe aic8800_fdrv;
```

## Uninstall

```bash
cd /home/aramirez/aramirez-Projects/WIFI-6-aic8800dc/aic8800dc/drivers/aic8800
sudo modprobe -r aic8800_fdrv aic_load_fw
sudo make uninstall
```

## Notes

- The `Makefile` defaults to `CONFIG_PLATFORM_UBUNTU=y` and picks up `/lib/modules/$(uname -r)/build`, so no extra flags are needed.
- Tested clean build (0 errors, 0 warnings) against kernel **7.0.0-15-generic** with **gcc 15.2.0**; also rebuilt clean against **7.0.0-22-generic** and **7.0.0-30-generic**.
- The default build is **DC/DW-only** and excludes RF test commands (`CONFIG_AIC8800D80=n`, `CONFIG_RFTEST=n`). The `.ko` is ~675 KB smaller as a result. See *Build Options* if you need the D80/D81 or RF-test paths.
- Build artifacts are now covered by a root `.gitignore`, so `git status` stays clean after a build.
- `aic8800_fdrv/Makefile`'s `make clean` used to print `test: -lt: unexpected operator` — the old-kernel version check ran `$(VERSION)` even when this Makefile is invoked standalone (outside a real `M=...` kbuild pass, where `VERSION` is never set). Guarded it with `test -n "$(VERSION)"`; `make clean` is now silent.
- **2026-08-21 bug-fixing pass**: found and fixed 19 real runtime bugs (use-after-free in USB TX teardown, RX double-free, illegal sleep in URB-completion context, several NULL derefs and leaks, Bluetooth patch-table parsing OOB reads) via a subagent audit + adversarial verification, then hand-fixed each one and confirmed a clean rebuild. One bug (a firmware-length-into-fixed-buffer stack overflow in `rwnx_cmds.c`) was left unfixed — full details and the reasoning for leaving it alone are in `README.md`'s Notes section. **Not yet installed or load-tested** — the new `.ko` files are built but the currently loaded module is still the old one.
- Same pass, follow-up: `sparse` doesn't work on this system at all (Ubuntu's packaged 0.6.4 predates a C construct this kernel's headers use — would need building sparse from source to fix). `cppcheck` ran clean — 3 false positives (verified by hand) and one harmless dead-code line removed.
- Added `dkms.conf` at the repo root so kernel upgrades can rebuild/reinstall automatically instead of the manual dance above — see `README.md`'s new *Optional: DKMS* section for setup and the caveat about it building from a copied source tree, not your live checkout.
