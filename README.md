# aic8800dc

Linux kernel driver for the **AIC8800 DC** WiFi 6 USB chipset.

## Target platform

This fork is **specifically maintained for**:

| Item | Value |
|---|---|
| Distribution | **Ubuntu 26.04 LTS** |
| Kernel | **7.0.X** (tested on `7.0.0-15-generic`) |
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
