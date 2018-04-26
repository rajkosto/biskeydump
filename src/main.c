#include "utils.h"
#include "hwinit.h"
#include "lib/printk.h"
#include "display/video_fb.h"

#include "fuse.h"
#include "se.h"
#include "hwinit/btn.h"
#include "hwinit/i2c.h"
#include "key_derivation.h"
#include "exocfg.h"
#define XVERSION 1

static void shutdown_using_pmic()
{
    const u8 MAX77620_I2C_PERIPH = I2C_5;
    const u8 MAX77620_I2C_ADDR = 0x3C;

    const u8 MAX77620_REG_ONOFFCNFG1 = 0x41;
    //const u8 MAX77620_REG_ONOFFCNFG2 = 0x42;

    //const u8 MAX77620_ONOFFCNFG1_SFT_RST = 1u << 7;
    const u8 MAX77620_ONOFFCNFG1_PWR_OFF = 1u << 1;

    u8 regVal = i2c_recv_byte(MAX77620_I2C_PERIPH, MAX77620_I2C_ADDR, MAX77620_REG_ONOFFCNFG1);
    regVal |= MAX77620_ONOFFCNFG1_PWR_OFF;
    i2c_send_byte(MAX77620_I2C_PERIPH, MAX77620_I2C_ADDR, MAX77620_REG_ONOFFCNFG1, regVal);
}

void load_sbk(const u8* sbkSrc) 
{
    uint32_t sbk[0x4];
    /* Load the SBK into the security engine, if relevant. */
    memcpy(sbk, sbkSrc, sizeof(sbk));
    for (unsigned int i = 0; i < 4; i++) 
    {
        if (sbk[i] != 0xFFFFFFFF) 
        {
            set_aes_keyslot(KEYSLOT_SWITCH_SBK, sbk, 0x10);
            break;
        }
    }
}

static __attribute__ ((noinline)) int tsec_key_readout(u8* outBuf)  //noinline so we get the stack space back
{
    u8 carveoutData[0x1000];
    u32 carveoutPtr = ((u32)carveoutData + 0xFF) & ~(0xFFu); //align to 0x100
    mc_enable_ahb_redirect(carveoutPtr, carveoutPtr+0xF00); //the values here dont matter ? it redirects a 1MiB block which covers all of IDATA it seems
    int retVal = 0;
    for (u32 rev=1; rev<2; rev++)
    {
        printk("trying carveout 0x%08x rev %u\n", carveoutPtr, rev);
        memset(outBuf,0,0x10);
        retVal = tsec_query(carveoutPtr, outBuf, rev);
        printk("TSEC KEY: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X retVal: %d\n",
            (u32)outBuf[0], (u32)outBuf[1], (u32)outBuf[2], (u32)outBuf[3], (u32)outBuf[4], (u32)outBuf[5], (u32)outBuf[6], (u32)outBuf[7], 
            (u32)outBuf[8], (u32)outBuf[9], (u32)outBuf[10], (u32)outBuf[11], (u32)outBuf[12], (u32)outBuf[13], (u32)outBuf[14], (u32)outBuf[15],
            retVal);
    }
    mc_disable_ahb_redirect();

    return retVal;
}
u8 g_tsec_key[16]; //to be used in other TUs

int main(void) {
    u32 *lfb_base;

    nx_hwinit();
    display_init();

    // Set up the display, and register it as a printk provider.
    lfb_base = display_init_framebuffer();
    video_init(lfb_base);

    printk("                                         biskeydump\n");
    printk("                                            v%d\n", XVERSION);

    /* Turn on the backlight after initializing the lfb */
    /* to avoid flickering. */
    display_enable_backlight(true);

    u8 sbkBuf[0x10];
    memset(sbkBuf, 0, sizeof(sbkBuf));
    fuse_get_hardware_info(sbkBuf);
    printk("HWI: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
        (u32)sbkBuf[0], (u32)sbkBuf[1], (u32)sbkBuf[2], (u32)sbkBuf[3], (u32)sbkBuf[4], (u32)sbkBuf[5], (u32)sbkBuf[6], (u32)sbkBuf[7],
        (u32)sbkBuf[8], (u32)sbkBuf[9], (u32)sbkBuf[10], (u32)sbkBuf[11], (u32)sbkBuf[12], (u32)sbkBuf[13], (u32)sbkBuf[14], (u32)sbkBuf[15]);

    memcpy(sbkBuf, (void *)get_fuse_chip_regs()->FUSE_PRIVATE_KEY, sizeof(sbkBuf));
	printk("SBK: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
        (u32)sbkBuf[0], (u32)sbkBuf[1], (u32)sbkBuf[2], (u32)sbkBuf[3], (u32)sbkBuf[4], (u32)sbkBuf[5], (u32)sbkBuf[6], (u32)sbkBuf[7],
        (u32)sbkBuf[8], (u32)sbkBuf[9], (u32)sbkBuf[10], (u32)sbkBuf[11], (u32)sbkBuf[12], (u32)sbkBuf[13], (u32)sbkBuf[14], (u32)sbkBuf[15]);

    load_sbk(sbkBuf);

    u8 tempBuf[32];
    memset(tempBuf, 0, sizeof(tempBuf));
    se_aes_128_ecb_encrypt_block(0xE, tempBuf, 0x10, tempBuf, 0x10);
    printk("SBK AESE 0 %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
        (u32)tempBuf[0], (u32)tempBuf[1], (u32)tempBuf[2], (u32)tempBuf[3], (u32)tempBuf[4], (u32)tempBuf[5], (u32)tempBuf[6], (u32)tempBuf[7], 
        (u32)tempBuf[8], (u32)tempBuf[9], (u32)tempBuf[10], (u32)tempBuf[11], (u32)tempBuf[12], (u32)tempBuf[13], (u32)tempBuf[14], (u32)tempBuf[15]);
        

    int retVal = tsec_key_readout(tempBuf);
    if (retVal == 0)
    {
        const int target_firmware = EXOSPHERE_TARGET_FIRMWARE_100; //doesnt matter for BIS keys

        memcpy(g_tsec_key, tempBuf, 0x10);
        derive_nx_keydata(target_firmware);

        for (int i=0; i<4; i++)
        {
            const int partition_id = (i > 2) ? 2 : i;
            derive_bis_key(tempBuf, (BisPartition_t)partition_id, target_firmware);

            printk("BIS KEY %d (crypt): %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n", i,
                (u32)tempBuf[0], (u32)tempBuf[1], (u32)tempBuf[2], (u32)tempBuf[3], (u32)tempBuf[4], (u32)tempBuf[5], (u32)tempBuf[6], (u32)tempBuf[7], 
                (u32)tempBuf[8], (u32)tempBuf[9], (u32)tempBuf[10], (u32)tempBuf[11], (u32)tempBuf[12], (u32)tempBuf[13], (u32)tempBuf[14], (u32)tempBuf[15]);
            printk("BIS KEY %d (tweak): %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n", i,
                (u32)tempBuf[16], (u32)tempBuf[17], (u32)tempBuf[18], (u32)tempBuf[19], (u32)tempBuf[20], (u32)tempBuf[21], (u32)tempBuf[22], (u32)tempBuf[23], 
                (u32)tempBuf[24], (u32)tempBuf[25], (u32)tempBuf[26], (u32)tempBuf[27], (u32)tempBuf[28], (u32)tempBuf[29], (u32)tempBuf[30], (u32)tempBuf[31]);
        }
    }
    else
        printk("ERROR getting TSEC key, cannot continue (is the TSEC firmware correct?)\n");

    // credits
    printk("\n                       fusee gelee by ktemkin, biskeydump by rajkosto\n");

    // Wait for the power button
    while (btn_read() != BTN_POWER);

    // Tell the PMIC to turn everything off
    shutdown_using_pmic();

    /* Do nothing for now */
    return 0;
}
