#include "utils.h"
#include "hwinit.h"
#include "lib/printk.h"
#include "display/video_fb.h"

#include "fuse.h"
#include "se.h"
#include "hwinit/btn.h"
#include "hwinit/i2c.h"
#include "hwinit/t210.h"
#include "key_derivation.h"
#include "exocfg.h"
#include "smiley.h"
#define XVERSION 2

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

static __attribute__ ((noinline)) void print_aes_key(const u8* keyData, size_t keySize)
{
    for (size_t i=0; i<keySize; i+=8)
    {
        printk("%02X%02X%02X%02X%02X%02X%02X%02X",
            (u32)keyData[i+0], (u32)keyData[i+1], (u32)keyData[i+2], (u32)keyData[i+3],
            (u32)keyData[i+4], (u32)keyData[i+5], (u32)keyData[i+6], (u32)keyData[i+7]);
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
        printk("TSEC KEY: ");
        print_aes_key(outBuf, 0x10);
        printk(" retVal: %d\n", retVal);
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
    printk("HWI: "); print_aes_key(sbkBuf, 0x10); printk("\n");

    memcpy(sbkBuf, (void *)get_fuse_chip_regs()->FUSE_PRIVATE_KEY, sizeof(sbkBuf));
	printk("SBK: "); print_aes_key(sbkBuf, 0x10); printk("\n");

    load_sbk(sbkBuf);

    u8 tempBuf[32];
    memset(tempBuf, 0, sizeof(tempBuf));
    se_aes_128_ecb_encrypt_block(KEYSLOT_SWITCH_SBK, tempBuf, 0x10, tempBuf, 0x10);
    printk("SBK AESE 0 "); print_aes_key(tempBuf, 0x10); printk("\n");

    int retVal = tsec_key_readout(tempBuf);
    if (retVal == 0)
    {
        const int target_firmware = EXOSPHERE_TARGET_FIRMWARE_100; //doesnt matter for BIS keys

        memcpy(g_tsec_key, tempBuf, 0x10);
        derive_nx_keydata(target_firmware);

        const unsigned int device_keyslot = (target_firmware >= EXOSPHERE_TARGET_FIRMWARE_400) ? (KEYSLOT_SWITCH_4XOLDDEVICEKEY) : (KEYSLOT_SWITCH_DEVICEKEY);
        memset(tempBuf, 0, sizeof(tempBuf));
        read_aes_keyslot(device_keyslot, tempBuf, 0x10);
        printk("DEVICE KEY: "); print_aes_key(tempBuf, 0x10); printk("\n");

        for (int i=0; i<4; i++)
        {
            const int partition_id = (i > 2) ? 2 : i;
            derive_bis_key(tempBuf, (BisPartition_t)partition_id, target_firmware);

            printk("BIS KEY %d (crypt): ", i); print_aes_key(&tempBuf[0], sizeof(tempBuf)/2); printk("\n");
            printk("BIS KEY %d (tweak): ", i); print_aes_key(&tempBuf[sizeof(tempBuf)/2], sizeof(tempBuf)/2); printk("\n");
        }
    }
    else
        printk("ERROR getting TSEC key, cannot continue (is the TSEC firmware correct?)\n");

    // credits
    printk("\n                       fusee gelee by ktemkin, biskeydump by rajkosto\n");

    const int smileySize = 128;
    const int smileyVertStart = 128*3;
    const int smileyHorizStart = (720/2)-(smileySize/2);
    const float incPixel = 1.0f/smileySize;
    const float timerToSeconds = 1.0f/1000000;
    u32 lastTmr = TMR(0x10);
    float lastTime = 0.0f;
    for (;;)
    {
        u32 currTmr = TMR(0x10);
        u32 passedTime = currTmr - lastTmr;
        lastTmr = currTmr;
        float fTime = lastTime + (passedTime*timerToSeconds);

        float sinOfTime = sinf(fTime*0.5f);
        float smile = sinOfTime*0.5f+0.5f; //animate smile with time
        vec2 eyesVect; // make it that the eyes look around as time passes
        {
            vec2 eyeRot = {sinOfTime, cosf(fTime*0.38f)};        
            eyesVect = vec2_mul_one(eyeRot,0.4f);
        }
        
        #if 0
        if(vec2_lengthsq(eyesVect) > 0.5f) 
            eyesVect = (vec2){0.0f,0.0f};	// fix bug with sudden eye move
        #endif

        int rowIdx = smileyVertStart*768;
        vec2 uv = {0.0f, 0.5f};
        for (int y=0; y<smileySize; y++)
        {
            uv.x = -0.5f;
            for (int x=0; x<128; x++)
            {
                vec4 currPixel = mainImage(uv, eyesVect, smile, fTime*3.0f);
                lfb_base[rowIdx+smileyHorizStart+x] = floats_to_pixel(currPixel.x, currPixel.y, currPixel.z);
                uv.x += incPixel;
            }
            rowIdx += 768;
            uv.y -= incPixel;

            // Check for power button press
            if (btn_read() == BTN_POWER)
                goto progend;
        }

        lastTime = fTime;
    }    

progend:
    // Tell the PMIC to turn everything off
    shutdown_using_pmic();

    /* Do nothing for now */
    return 0;
}
