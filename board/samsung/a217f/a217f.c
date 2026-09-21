// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2024, Linaro Ltd.
 * Author: Sam Protsenko <semen.protsenko@linaro.org>
 */

#include <env.h>
#include <init.h>
#include <mapmem.h>
#include <net.h>
#include <asm/io.h>
#include "pmic.h"

/* OTP Controller base address and register offsets */
#define EXYNOS850_OTP_BASE		0x10000000
#define OTP_CHIPID0			0x4
#define OTP_CHIPID1			0x8

/* ACPM and PMIC definitions */
#define EXYNOS850_MBOX_APM2AP_BASE	0x11900000
#define EXYNOS850_APM_SRAM_BASE		0x02039000	/* in iRAM */
#define EXYNOS850_APM_SHMEM_OFFSET	0x3200
#define EXYNOS850_IPC_AP_I3C		10

static struct acpm acpm = {
	.mbox_base	= (void __iomem *)EXYNOS850_MBOX_APM2AP_BASE,
	.sram_base	= (void __iomem *)(EXYNOS850_APM_SRAM_BASE +
					   EXYNOS850_APM_SHMEM_OFFSET),
	.ipc_ch		= EXYNOS850_IPC_AP_I3C,
};

/* Read the unique SoC ID from OTP registers */
static u64 get_chip_id(void)
{
	void __iomem *otp_base;
	u64 val;

	otp_base = map_sysmem(EXYNOS850_OTP_BASE, 12);
	val = readl(otp_base + OTP_CHIPID0);
	val |= (u64)readl(otp_base + OTP_CHIPID1) << 32UL;
	unmap_sysmem(otp_base);

	return val;
}

static void setup_serial(void)
{
	char serial_str[17] = { 0 };
	u64 serial_num;

	if (env_get("serial#"))
		return;

	serial_num = get_chip_id();
	snprintf(serial_str, sizeof(serial_str), "%016llx", serial_num);
	env_set("serial#", serial_str);
}

static void setup_ethaddr(void)
{
	u64 serial_num;
	u32 mac_hi, mac_lo;
	u8 mac_addr[6];

	if (env_get("ethaddr"))
		return;

	serial_num = get_chip_id();
	mac_lo = (u32)serial_num;		/* OTP_CHIPID0 */
	mac_hi = (u32)(serial_num >> 32UL);	/* OTP_CHIPID1 */
	mac_addr[0] = (mac_hi >> 8) & 0xff;
	mac_addr[1] = mac_hi & 0xff;
	mac_addr[2] = (mac_lo >> 24) & 0xff;
	mac_addr[3] = (mac_lo >> 16) & 0xff;
	mac_addr[4] = (mac_lo >> 8) & 0xff;
	mac_addr[5] = mac_lo & 0xff;
	mac_addr[0] &= ~0x1; /* make sure it's not a multicast address */
	if (is_valid_ethaddr(mac_addr))
		eth_env_set_enetaddr("ethaddr", mac_addr);
}

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

int board_late_init(void)
{
	setup_serial();
	setup_ethaddr();

	return 0;
}

int power_init_board(void)
{
	int err;

	err = pmic_init(&acpm);
	if (err)
		printf("ERROR: Failed to configure PMIC (%d)\n", err);

	return 0;
}
