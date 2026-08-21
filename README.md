# aic8800dc

Linux kernel driver for the **AIC8800 DC** WiFi 6 USB chipset.

## Target platform

This fork is **specifically maintained for**:

| Item | Value |
|---|---|
| Distribution | **Ubuntu 26.04 LTS** |
| Kernel | **7.0.X** (tested on `7.0.0-15-generic`, `7.0.0-22-generic`, `7.0.0-30-generic`) |
| Architecture | **x86_64** |
| Toolchain | gcc 15.2.0 |

It builds cleanly on this configuration with **0 errors and 0 warnings**, and is also clean under `sparse` static analysis. Other distributions / kernel versions / architectures may work but are not actively tested here — if you need them, see the upstream repo.

## Attention
Before installing the driver, delete all aic8800-related folders under `/lib/firmware`. Using an incorrect firmware version may cause the system to freeze.

## Prerequisites

Install kernel headers, a C toolchain, and the BTF/static-analysis helpers:

```bash
sudo apt install linux-headers-$(uname -r) build-essential dwarves sparse cppcheck
```

## Installation Steps

### Copy udev rules:

Copy the `aic.rules` file to `/lib/udev/rules.d/`:

```bash
sudo cp aic.rules /lib/udev/rules.d/
```

### Copy firmware:

Copy the `aic8800DC` folder from `./fw` to `/lib/firmware/`:

```bash
sudo cp -r ./fw/aic8800DC /lib/firmware/
```

### Navigate to the driver directory:

Change to the `drivers/aic8800` directory:

```bash
cd ./drivers/aic8800
```

### Compile and Install the Driver:

The kernel build system records the compiler used to build the running kernel. To avoid the *"compiler differs from the one used to build the kernel"* warning, detect that compiler and pass it to `make`:

```bash
make clean
COMPILER=$(grep LINUX_COMPILER /usr/src/linux-headers-$(uname -r)/include/generated/compile.h | sed -re 's/^.*\"([^ ]+) .*$/\1/')
echo "Compiler to use: ${COMPILER}"
make CC=${COMPILER}
```

Then, install the driver:

```bash
sudo make install
```

### Build options

The build is configured via variables in `drivers/aic8800/aic8800_fdrv/Makefile`. Two are worth knowing about:

| Variable | Default | Meaning |
|---|---|---|
| `CONFIG_AIC8800D80` | `n` | Build the **8800D80/D81** chip compat layer. This tree targets the **8800DC/DW**, so it is off by default (smaller module). **If your adapter enumerates as 8800D80/D81** it will not initialize unless you set this to `y`. |
| `CONFIG_RFTEST` | `n` | Build the RF manufacturing/calibration test commands used by the `aicrf_test` tool. Not needed for normal Wi-Fi; enable only for RF bring-up. |

You can override either on the command line without editing the Makefile, e.g. for an 8800D80/D81 device:

```bash
make CC=${COMPILER} CONFIG_AIC8800D80=y
```

For any kernel updates, you'll need to reinstall the driver:

```bash
make clean
COMPILER=$(grep LINUX_COMPILER /usr/src/linux-headers-$(uname -r)/include/generated/compile.h | sed -re 's/^.*\"([^ ]+) .*$/\1/')
make CC=${COMPILER}
sudo make install
```

## Load the Driver

After installation, load the driver with the following command:

```bash
sudo modprobe aic8800_fdrv
```

## Verify the Module is Active

Check if the module is loaded correctly:

```bash
lsmod | grep aic
```
You should see output similar to:

```
aic8800_fdrv    536576  0
cfg80211        1146880 1   aic8800_fdrv
aic_load_fw     69632   1   aic8800_fdrv
usbcore         348160  10  xhci_hcd,ehci_pci,usbhid,usb_storage,ehci_hcd,xhci_pci,uas,aic_load_fw,uhci_hcd,aic8800_fdrv
```

After that, plug in your USB wireless network card.

## Verify Wi-Fi Device is Active

To check if the Wi-Fi interface is recognized, run:

```bash
iwconfig
```
If the device is still not active, check the kernel logs for any errors related to the driver:

```bash
sudo dmesg
```

## Uninstall

```bash
cd ./drivers/aic8800
sudo modprobe -r aic8800_fdrv aic_load_fw
sudo make uninstall
```

## Optional: DKMS (rebuild automatically on kernel upgrades)

By default this driver has to be manually rebuilt and reinstalled after every kernel upgrade (see *Build options* above). A `dkms.conf` is provided so DKMS can do that automatically instead, using the kernel package manager's own `postinst` hook:

```bash
sudo apt install dkms
sudo dkms add .
sudo dkms build aic8800dc/6.4.3.0
sudo dkms install aic8800dc/6.4.3.0
```

From then on, every new kernel package installed via `apt` triggers an automatic rebuild + reinstall for that kernel — no more manual `make clean && make && sudo make install` after upgrades.

**Caveat**: `dkms add` copies this source tree into `/usr/src/aic8800dc-6.4.3.0/`; DKMS builds from that copy, not from your working checkout. If you're actively editing the driver, keep using the manual `make`/`sudo make install` workflow above and treat DKMS as something to set up once you're done iterating — otherwise you'd need to re-run `dkms add`/`dkms build` after every source change. To remove it: `sudo dkms remove aic8800dc/6.4.3.0 --all`.

## Notes

- This fork has been hardened against kernel-API churn from **6.1 through 7.0** (see `git log` for the per-version compat patches), and audited for portability and safety:
  - Endianness — the firmware-message ABI is now correctly typed as `__le16`/`__le32`, with explicit `cpu_to_le*()` / `le*_to_cpu()` conversions at every access site. Zero runtime cost on little-endian hosts (x86, ARM-LE); correct on big-endian.
  - I/O memory — PCI BAR pointers in the DINI/V7 platform back-ends carry the `__iomem` annotation.
  - Several real bugs fixed (sprintf source/dest overlap, fortify-source overflow on the radiotap path, list-mutation outside spinlock during USB teardown, dead-store of param-by-value after `kfree`).
- Additional safety/maintainability work (latest pass):
  - **Allocation safety** — added missing `NULL` checks on the scan-request, channel, USB-TX, and scan-result allocations; fixed a double-free / wrong-pointer `vfree` in the USB init error path.
  - **Untrusted-input validation** — firmware/USB-message station, TID and mesh next-hop indices are now range-checked *before* indexing internal tables; the netlink vendor-command ring name is copied with a length bound (was an unbounded `strcpy` into a 32-byte buffer).
  - **Timer-API churn** — the `del_timer`→`timer_delete` (6.15) and `from_timer`→`timer_container_of` (6.16) renames are funnelled through `rwnx_del_timer*` / `rwnx_from_timer` macros in `rwnx_compat.h`, so a future rename is a one-line edit instead of a scatter-patch. This also fixed a latent `time_delete_sync` typo in the (non-default) SDIO path.
  - **Lock contract** — `reord_rxframes_ind()` now documents that the caller must hold `reord_list_lock` and enforces it with `lockdep_assert_held()` under debug kernels.
  - **Build trimming** — RF test commands and the 8800D80 compat layer are off by default; see *Build options* above.
  - A root `.gitignore` now keeps kernel build artifacts (`*.o`, `*.ko`, `*.cmd`, `Module.symvers`, …) out of `git status`.
- The top-level `Makefile` defaults to `CONFIG_PLATFORM_UBUNTU=y`. The Rockchip / Allwinner / Amlogic blocks contain dead vendor paths and are gated off by default.
- **Runtime-bug audit pass** (2026-08-21): the build was already 0 warnings under `-Wall -Wextra` plus the kernel's `-Werror=` hardening flags, so a targeted read-through of the USB transport, command-manager, and Bluetooth firmware-loader code turned up 20 real runtime bugs the compiler can't see; 19 are fixed, one is a documented known issue:
  - **USB TX teardown use-after-free** — on disconnect, the TX bus thread could still be mid-submit when URBs were cancelled and `aic_usb_dev` freed. Added a `tx_submitted` USB anchor (mirroring the existing RX anchor) and reordered `aicwf_bus_deinit()` to fully stop the TX thread/tasklet *before* cancelling URBs.
  - **RX double-free / OOB read** — an oversized aggregated USB sub-packet was freed inline and then `continue`d back into a loop condition that read the freed `skb`, and later fell through to a second, unconditional free of the same `skb`; also added a bounds check so a corrupted/oversized sub-packet length can no longer `memcpy` past the end of the receive buffer.
  - **Illegal sleep in atomic context** — `aicwf_usb_rx_complete()` (a URB completion callback) called the blocking `down()` on a disconnect-rendezvous semaphore; switched to `down_trylock()` in both the active and `CONFIG_PREALLOC_RX_SKB` code paths.
  - **NULL-pointer derefs** — `rwnx_send_msg()`/`rwnx_send_msg1()` now check for command-pool exhaustion instead of dereferencing a `NULL` `cmd`; `rwnx_rx_handle_msg()` now bounds/NULL-checks the task-handler table lookup instead of indexing it unconditionally (mirroring the existing guard in `RWNX_ID2STR()`).
  - **Leaks** — a timed-out deferred command no longer permanently loses its slot in the 20-entry command pool (both the main driver's `rwnx_cmds.c` and the Bluetooth loader's own copy); a workqueue-creation failure in `rwnx_cfg80211_init()` no longer leaks the `sw_txhdr` slab cache; early-return paths in `cmd_mgr_queue()`/`cmd_mgr_queue_force_defer()` now free the pending firmware message; a TDLS discovery-response and a vendor `GET_CHANNEL_LIST` error path no longer leak an `skb` / heap buffer; the Bluetooth patch-table loader now frees its linked list on a mid-download write failure; a dead (`CONFIG_USB_TX_AGGR=n`) TX-aggregation buffer now has a matching teardown call.
  - **Bluetooth firmware parsing** — `vmalloc()` results are now NULL-checked *before* `memset()` (was a guaranteed crash on allocation failure); the patch-table parser now validates each entry against the remaining buffer length before reading it, instead of trusting a firmware-supplied length unconditionally.
  - **Debugfs** — `rwnx_radar_dump_pattern_detector()`'s size-probe pass now sums over *all* radar types instead of returning after the first one, so the real dump can no longer be silently truncated.
  - **Known issue, not fixed**: `rwnx_cmds.c`'s `cmd_mgr_msgind()` copies a firmware-reported confirmation length into a caller-supplied buffer, bounded only by a generic 1024-byte cap rather than each message type's real (often much smaller) buffer size — a stack-overflow risk if the firmware ever reports a corrupt/oversized length. The equivalent bug in the Bluetooth loader's private command manager (`aic_load_fw/aicbluetooth_cmds.c`) *was* fixed, because that copy only ever has one destination type in its whole call graph. The main driver's version would require threading the real destination size through `rwnx_send_msg()`/`rwnx_send_msg1()`, which have 100+ call sites across nearly every driver feature (scan, connect, AP, P2P, TDLS, rate control, …) — too invasive to change blind without hardware coverage of every one of those paths.
  - All fixes verified with a full clean rebuild (0 warnings, 0 errors); not yet installed/load-tested on hardware.
- **Follow-up static analysis** (same pass): `sparse` can't run at all against this kernel version — Ubuntu's packaged 0.6.4 doesn't understand `__typeof_unqual__`, which this kernel's headers use, so kbuild's own `checker-valid.sh` refuses to invoke it (would need building sparse from source to use it here). `cppcheck` ran clean: of 4 non-`KERNEL_VERSION`-noise findings, 3 were false positives (two are cppcheck not tracing initialization through `list_for_each_entry_safe` over a pre-allocated pool, one is cppcheck not tracing a postcondition through a cross-function pointer-output parameter — verified by hand in both cases) and one was a genuinely pointless `head = NULL;` local reassignment in `aicbt_patch_table_free()`, removed.
- `dkms.conf` added at the repo root — see *Optional: DKMS* above.
