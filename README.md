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
- The top-level `Makefile` defaults to `CONFIG_PLATFORM_UBUNTU=y`. The Rockchip / Allwinner / Amlogic blocks contain dead vendor paths and are gated off by default.
