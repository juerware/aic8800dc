/**
 ****************************************************************************************
 *
 * @file co_types.h
 *
 * @brief This file replaces the need to include stdint or stdbool typical headers,
 *        which may not be available in all toolchains, and adds new types
 *
 * Copyright (C) RivieraWaves 2009-2019
 *
 * $Rev: $
 *
 ****************************************************************************************
 */

#ifndef _LMAC_INT_H_
#define _LMAC_INT_H_


/**
 ****************************************************************************************
 * @addtogroup CO_INT
 * @ingroup COMMON
 * @brief Common integer standard types (removes use of stdint)
 *
 * @{
 ****************************************************************************************
 */


/*
 * DEFINES
 ****************************************************************************************
 */

#include <linux/version.h>
#include <linux/types.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
#include <linux/bits.h>
#else
#include <linux/bitops.h>
#endif

#ifdef CONFIG_RWNX_TL4
typedef uint16_t u8_l;
typedef int16_t s8_l;
typedef uint16_t bool_l;
#else
typedef uint8_t u8_l;
typedef int8_t s8_l;
typedef bool bool_l;
#endif
/*
 * The "_l" suffix marks types that travel on-the-wire to/from firmware
 * messages (a firmware module is internally called "lmac"). These messages
 * are always little-endian, so use the kernel's __le types — sparse will
 * then flag any access site that mixes native and on-wire byte order.
 *
 * On little-endian hosts (x86, ARM-LE) these are bit-identical to the plain
 * uint types, so the runtime cost is zero. On big-endian hosts the explicit
 * cpu_to_le* / le*_to_cpu conversions in code become real byte-swaps.
 */
typedef __le16 u16_l;
typedef int16_t s16_l;
typedef __le32 u32_l;
typedef int32_t s32_l;
typedef __le64 u64_l;



/// @} CO_INT
#endif // _LMAC_INT_H_
