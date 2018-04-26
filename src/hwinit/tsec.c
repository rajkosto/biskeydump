#include <string.h>
#include "tsec.h"
#include "clock.h"
#include "t210.h"
static const u8 _tsec_fw[0xF00] __attribute__((aligned(0x100))) = 
{
	#include "tsecfw.inl"
};

static int _tsec_dma_wait_idle()
{
	u32 timeout = TMR(0x10) + 10000000;

	while (!(TSEC(0x1118) & 2))
		if (TMR(0x10) > timeout)
			return 0;

	return 1;
}

static int _tsec_dma_pa_to_internal_100(int not_imem, int i_offset, int pa_offset)
{
	u32 cmd;

	if (not_imem)
		cmd = 0x600; // DMA 0x100 bytes
	else
		cmd = 0x10; // dma imem

	TSEC(0x1114) = i_offset; // tsec_dmatrfmoffs_r
	TSEC(0x111C) = pa_offset; // tsec_dmatrffboffs_r
	TSEC(0x1118) = cmd; // tsec_dmatrfcmd_r

	return _tsec_dma_wait_idle();
}

#include "../lib/printk.h"
#include "../lib/miniz.h"

int tsec_query(u32 carveout, u8 *dst, u32 rev)
{
	int res = 0;

	//Enable clocks.
	clock_enable_host1x();
	clock_enable_tsec();
	clock_enable_sor_safe(); //same behaviour when disabled
	clock_enable_sor0();
	clock_enable_sor1();
	clock_enable_kfuse();

	//Configure Falcon.
	TSEC(0x110C) = 0; // tsec_dmactl_r
	TSEC(0x1010) = 0xFFF2; // tsec_irqmset_r
	TSEC(0x101C) = 0xFFF0; // tsec_irqdest_r
	TSEC(0x1048) = 3; // tsec_itfen_r
	if (!_tsec_dma_wait_idle())
	{
		res = -1;
		goto out;
	}

	//Load firmware.
	memcpy((void *)carveout, _tsec_fw, sizeof(_tsec_fw));
	u32 theCrc = mz_crc32(MZ_CRC32_INIT, (void*)carveout, sizeof(_tsec_fw));
	printk("TSEC FW CRC32: %08x - %s\n", theCrc, (theCrc == 0xB035021F) ? "CORRECT" : "INCORRECT");

#ifdef TEST_TSEC_RESET
    static const u32 bit5_CPU_STOPPED = 1u << 5;
    static const u32 bit4_CPU_HALTED = 1u << 4;
    static const u32 bit3_HARD_RESET = 1u << 3;
    static const u32 bit2_SOFT_RESET = 1u << 2;
    static const u32 bit1_START_CPU = 1u << 1;
    static const u32 bit0_IINVAL = 1u << 0;

    u32 cpuState = TSEC(0x1100);
    printk("TSEC CPU STATE: STOPPED: %02x HALTED: %02x HRESET: %02x SRESET: %02x START: %02x IINVAL: %02x\n", 
        (cpuState & bit5_CPU_STOPPED) ? 1 : 0, 
        (cpuState & bit4_CPU_HALTED) ? 1 : 0,
        (cpuState & bit3_HARD_RESET) ? 1 : 0,
        (cpuState & bit2_SOFT_RESET) ? 1 : 0,
        (cpuState & bit1_START_CPU) ? 1 : 0,
        (cpuState & bit0_IINVAL) ? 1 : 0);

    cpuState |= bit3_HARD_RESET;
    TSEC(0x1100) = cpuState;
    while (cpuState & bit3_HARD_RESET)
    {
        cpuState = TSEC(0x1100);
        printk("TSEC CPU STATE: STOPPED: %02x HALTED: %02x HRESET: %02x SRESET: %02x START: %02x IINVAL: %02x\n", 
            (cpuState & bit5_CPU_STOPPED) ? 1 : 0, 
            (cpuState & bit4_CPU_HALTED) ? 1 : 0,
            (cpuState & bit3_HARD_RESET) ? 1 : 0,
            (cpuState & bit2_SOFT_RESET) ? 1 : 0,
            (cpuState & bit1_START_CPU) ? 1 : 0,
            (cpuState & bit0_IINVAL) ? 1 : 0);
    }
#endif

	TSEC(0x1110) = carveout >> 8;// tsec_dmatrfbase_r
	for (u32 addr = 0; addr < sizeof(_tsec_fw); addr += 0x100)
    {
		if (!_tsec_dma_pa_to_internal_100(0, addr, addr))
		{
			res = -2;
			goto out;
		}        
    }

	//Execute firmware.
	HOST1X(0x3300) = 0x34C2E1DA;
	TSEC(0x1044) = 0;
	TSEC(0x1040) = rev;
	TSEC(0x1104) = 0; // tsec_bootvec_r
    TSEC(0x1100) = 2; // tsec_cpuctl_r

	if (!_tsec_dma_wait_idle())
	{
		res = -3;
		goto out;
	}
	u32 timeout = TMR(0x10) + 2000000;
	while (!TSEC(0x1044))
		if (TMR(0x10) > timeout)
		{
			res = -4;
			goto out;
		}
	if (TSEC(0x1044) != 0xB0B0B0B0)
	{
		res = -5;
		goto out;
	}

	//Fetch result.
	HOST1X(0x3300) = 0;
	u32 buf[4];
	buf[0] = SOR1(0x1E8);
	buf[1] = SOR1(0x21C);
	buf[2] = SOR1(0x208);
	buf[3] = SOR1(0x20C);
	SOR1(0x1E8) = 0;
	SOR1(0x21C) = 0;
	SOR1(0x208) = 0;
	SOR1(0x20C) = 0;
	memcpy(dst, &buf, 0x10);

out:;

	//Disable clocks.
	clock_disable_kfuse();
	clock_disable_sor1();
	clock_disable_sor0();
	clock_disable_sor_safe();
	clock_disable_tsec();
	clock_disable_host1x();

	return res;
}
