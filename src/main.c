#include "utils.h"
#include "hwinit.h"
#include "lib/printk.h"
#include "display/video_fb.h"

#include "fuse.h"
#include "se.h"
#include "hwinit/btn.h"
#include "hwinit/i2c.h"
#include "hwinit/t210.h"
#include "sdmmc.h"
#include "lib/miniz.h"
#include "key_derivation.h"
#include "exocfg.h"
#include "smiley.h"
#include "lib/qrcodegen.h"
#include <alloca.h>
#define XVERSION 5

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

static NOINLINE const char* hexify_crypto_key(const void* dataBuf, size_t keySize)
{
    static char tempBuf[72];
    const u8* keyData = (const u8*)dataBuf;
    int currOffs = 0;    
    for (size_t i=0; i<keySize; i+=8)
    {
        currOffs += snprintfk(&tempBuf[currOffs], sizeof(tempBuf)-currOffs, 
            "%02X%02X%02X%02X%02X%02X%02X%02X",
            (u32)keyData[i+0], (u32)keyData[i+1], (u32)keyData[i+2], (u32)keyData[i+3],
            (u32)keyData[i+4], (u32)keyData[i+5], (u32)keyData[i+6], (u32)keyData[i+7]);
    }
    return tempBuf;
}

static NOINLINE int tsec_key_readout(struct mmc* mmc, void* outBuf)  //noinline so we get the stack space back
{
    u8 carveoutData[0x1000];
    u32 carveoutCurrIdx = 0;
    
    static const u32 PKG1LDR_OFFSET = 0x100000;
    static const u32 PKG1LDR_SIZE = 0x40000;    
    static const u32 TSECFW_SIZE = 0xF00;

    static const u32 SECTOR_SIZE = 512;
    static const u32 BUFFER_SIZE_IN_SECTORS = sizeof(carveoutData)/SECTOR_SIZE;    
    static const u32 PKG1LDR_SIZE_IN_SECTORS = PKG1LDR_SIZE/SECTOR_SIZE;

    u32 tsecFoundAt = 0;
    u32 totalSectorsRead = 0;
    u32 currentSectorIdx = PKG1LDR_OFFSET/SECTOR_SIZE;
    printk("eMMC initialized, looking for TSEC FW (scanning 0x%08x-0x%08x of boot0)\n", PKG1LDR_OFFSET, PKG1LDR_OFFSET+PKG1LDR_SIZE);
    while (totalSectorsRead < PKG1LDR_SIZE_IN_SECTORS)
    {
        u32 numSectorsToRead = PKG1LDR_SIZE_IN_SECTORS-totalSectorsRead;
        if (numSectorsToRead > BUFFER_SIZE_IN_SECTORS)
            numSectorsToRead = BUFFER_SIZE_IN_SECTORS;

        static const u32 MAX_SECTORS_PER_READ = 4;
        if (numSectorsToRead > MAX_SECTORS_PER_READ)
            numSectorsToRead = MAX_SECTORS_PER_READ;

        u8* readPtr = carveoutData;
        if (tsecFoundAt != 0)
        {
            readPtr = &carveoutData[carveoutCurrIdx];

            u32 maxReadLimit = sizeof(carveoutData)-carveoutCurrIdx;
            maxReadLimit /= SECTOR_SIZE;
            if (numSectorsToRead > maxReadLimit)
                numSectorsToRead = maxReadLimit;
        }

#ifdef SDMMC_DEBUGGING
        printk("SDMMC: Reading bytes 0x%08x-0x%08x\n", currentSectorIdx*SECTOR_SIZE, (currentSectorIdx+numSectorsToRead)*SECTOR_SIZE);
#endif
        int sdMmcRetVal = sdmmc_read(mmc, readPtr, currentSectorIdx, numSectorsToRead);
        if (sdMmcRetVal != 0)
        {
            printk("SDMMC ERROR Reading bytes 0x%08x-0x%08x: %d\n", sdMmcRetVal, currentSectorIdx*SECTOR_SIZE, (currentSectorIdx+numSectorsToRead)*SECTOR_SIZE);
            break;
        }
        totalSectorsRead += numSectorsToRead;

        u32 numBytesRead = numSectorsToRead*SECTOR_SIZE;
        if (tsecFoundAt == 0) //havent found tsecfw start yet
        {
            for (u32 i=0; i<numBytesRead; i+=0x100) //tsecfw is always 0x100 aligned
            {
                u32 currentDataWord = 0;
                memcpy(&currentDataWord, &readPtr[i], sizeof(currentDataWord));

                static const u32 TSECFW_START_IDENT = 0xCF42004D; //in little endian
                if (currentDataWord == TSECFW_START_IDENT)
                {
                    u32 numBytesWeHave = numBytesRead-i;
                    memmove(&carveoutData[0], &readPtr[i], numBytesWeHave);
                    carveoutCurrIdx = numBytesWeHave;
                    tsecFoundAt = (currentSectorIdx*SECTOR_SIZE)+i;
                    break;
                }
            }
        }
        else //read to tsecfw buffer
        {
            carveoutCurrIdx += numBytesRead;
            if (carveoutCurrIdx >= TSECFW_SIZE)
                break; //we are done
        }

        currentSectorIdx += numSectorsToRead;
    }
    	
    if (tsecFoundAt == 0)
    {
        printk("ERROR: No TSEC FW found in pkg1ldr! (started at 0x%08x of boot0, ended at 0x%08x)\n", PKG1LDR_OFFSET, PKG1LDR_OFFSET+(totalSectorsRead*SECTOR_SIZE));
        return -10;
    }
    else
        printk("Found TSEC FW at offset 0x%08x of boot0 (0x%08x of pkg1ldr)\n", tsecFoundAt, tsecFoundAt-PKG1LDR_OFFSET);

    u8* carveoutBytes = (u8*)(((u32)carveoutData + 0xFF) & ~(0xFFu)); //align to 0x100
    u32 carveoutSize = (u32)carveoutData+sizeof(carveoutData)-(u32)carveoutBytes;
    memmove(carveoutBytes, carveoutData, carveoutSize);
    {
        u32 theCrc = mz_crc32(MZ_CRC32_INIT, carveoutBytes, TSECFW_SIZE);
        bool crcCorrect = (theCrc == 0xB035021F);
        printk("TSEC FW CRC32: %08x - %s\n", theCrc, crcCorrect ? "CORRECT" : "INCORRECT");
        if (!crcCorrect)
			return -11;
    }    
    
    int retVal = 0;
    const u32 tsecRev = 1;
    printk("TSEC using carveout 0x%08x rev %u\n", (u32)carveoutBytes, tsecRev);
    memset(outBuf,0,0x10);
    retVal = tsec_query((u32)carveoutBytes, outBuf, tsecRev);
    return retVal;
}

#define REMAINING_TEXT_BYTES(bufVar, bufPosVar) ((((int)sizeof(bufVar)-(int)bufPosVar-1) > 0) ? (sizeof(bufVar)-bufPosVar-1) : 0)
int main(void) {
    char textBuf[1024];
    u8 qrDataBuf[qrcodegen_BUFFER_LEN_FOR_VERSION(20)];
    u8 qrTempBuf[4096-sizeof(qrDataBuf)-sizeof(textBuf)]; //larger than necessary to preserve nice alignments
    u32 tempBuf[0x20/sizeof(u32)];
    int currTextBufPos = 0;
    int lastLineLength = 0;
    u32* lfb_base;    

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

    memset(tempBuf, 0, sizeof(tempBuf));
    fuse_get_hardware_info(tempBuf);
    lastLineLength = snprintfk(&textBuf[currTextBufPos], REMAINING_TEXT_BYTES(textBuf, currTextBufPos),
                     "HWI: %s\n", hexify_crypto_key(tempBuf, 0x10));
    video_puts(&textBuf[currTextBufPos]); currTextBufPos += lastLineLength;

    memset(tempBuf, 0, sizeof(tempBuf));
    memcpy(tempBuf, (void*)get_fuse_chip_regs()->FUSE_PRIVATE_KEY, 0x10);
    lastLineLength = snprintfk(&textBuf[currTextBufPos], REMAINING_TEXT_BYTES(textBuf, currTextBufPos),
                     "SBK: %s\n", hexify_crypto_key(tempBuf, 0x10));
    video_puts(&textBuf[currTextBufPos]); currTextBufPos += lastLineLength;

    set_aes_keyslot(KEYSLOT_SWITCH_SBK, tempBuf, 0x10);
    se_aes_128_ecb_encrypt_block(KEYSLOT_SWITCH_SBK, (u8*)(tempBuf)+0, sizeof(tempBuf)/2, (u8*)(tempBuf)+sizeof(tempBuf)/2, sizeof(tempBuf)/2);
    printk("SBK AESE 0 (test): %s\n", hexify_crypto_key(tempBuf, 0x10));

    struct mmc mmcPart;
    memset(&mmcPart,0,sizeof(mmcPart));
    mc_enable_ahb_redirect(); //needed for sdmmc as well
    int retVal = sdmmc_init(&mmcPart, SWITCH_EMMC);
    if (retVal != 0)
        printk("ERROR %d initializing SDMMC!\n", retVal);
    else
    {
        retVal = sdmmc_switch_part(&mmcPart, 1);
        if (retVal != 0)
            printk("ERROR %d switching to boot0 partition!\n", retVal);
        else
        {
            memset(tempBuf, 0, sizeof(tempBuf));
            retVal = tsec_key_readout(&mmcPart, tempBuf);
        }
    }
    
    if (retVal == 0)
    {
        lastLineLength = snprintfk(&textBuf[currTextBufPos], REMAINING_TEXT_BYTES(textBuf, currTextBufPos),
                     "TSEC KEY: %s\n", hexify_crypto_key(tempBuf, 0x10));
        video_puts(&textBuf[currTextBufPos]); currTextBufPos += lastLineLength;

        const u32 target_tsec_keyslot = KEYSLOT_SWITCH_DEVICEKEY;
        set_aes_keyslot(target_tsec_keyslot, tempBuf, 0x10);
        se_aes_128_ecb_encrypt_block(target_tsec_keyslot, (u8*)(tempBuf)+0, sizeof(tempBuf)/2, (u8*)(tempBuf)+sizeof(tempBuf)/2, sizeof(tempBuf)/2);
        printk("TSEC AESE 0 (test): %s\n", hexify_crypto_key(tempBuf, 0x10));
        
        const u32 target_firmware = EXOSPHERE_TARGET_FIRMWARE_100; //doesnt matter for BIS keys
        retVal = derive_nx_keydata(&mmcPart, target_firmware);
        if (retVal == 0)
        {
            const u32 device_keyslot = (target_firmware >= EXOSPHERE_TARGET_FIRMWARE_400) ? (KEYSLOT_SWITCH_4XOLDDEVICEKEY) : (KEYSLOT_SWITCH_DEVICEKEY);
            memset(tempBuf, 0, sizeof(tempBuf));
            read_aes_keyslot(device_keyslot, tempBuf, 0x10);
            lastLineLength = snprintfk(&textBuf[currTextBufPos], REMAINING_TEXT_BYTES(textBuf, currTextBufPos),
                        "DEVICE KEY: %s\n", hexify_crypto_key(tempBuf, 0x10));
            video_puts(&textBuf[currTextBufPos]); currTextBufPos += lastLineLength;

            for (int i=0; i<4; i++)
            {
                memset(tempBuf, 0, sizeof(tempBuf));
                const int partition_id = (i > 2) ? 2 : i;
                derive_bis_key(tempBuf, (BisPartition_t)partition_id, target_firmware);

                lastLineLength = snprintfk(&textBuf[currTextBufPos], REMAINING_TEXT_BYTES(textBuf, currTextBufPos),
                        "BIS KEY %d (crypt): %s\n", i, hexify_crypto_key((u8*)(tempBuf)+0, sizeof(tempBuf)/2));
                video_puts(&textBuf[currTextBufPos]); currTextBufPos += lastLineLength;
                
                lastLineLength = snprintfk(&textBuf[currTextBufPos], REMAINING_TEXT_BYTES(textBuf, currTextBufPos),
                        "BIS KEY %d (tweak): %s\n", i, hexify_crypto_key((u8*)(tempBuf)+sizeof(tempBuf)/2, sizeof(tempBuf)/2));
                video_puts(&textBuf[currTextBufPos]); currTextBufPos += lastLineLength;
            }
        }
        else
            printk("ERROR deriving device keydata (retVal %d), cannot continue\n", retVal);
    }
    else
        printk("ERROR getting TSEC key (retVal %d), cannot continue\n", retVal);

    // credits
    printk("\n                       fusee gelee by ktemkin, biskeydump by rajkosto\n");
    mc_disable_ahb_redirect(); //no longer needed 

    const u32 framebufferLineWidth = 720;
    const u32 framebufferLineStride = 768;
    
    const int smileySize = 128;
    const int smileyVertStart = 128*3;
    const int smileyHorizStart = (framebufferLineWidth/2)-(smileySize/2);    

    int qrCodeVertStart = smileyVertStart + ((smileySize*3)/2);
    if (qrcodegen_encodeText(textBuf, qrTempBuf, qrDataBuf, qrcodegen_Ecc_MEDIUM, 10, 20, qrcodegen_Mask_AUTO, true))
    {
        int qrCodeOrigSize = qrcodegen_getSize(qrDataBuf);
        int qrCodeActualSize = qrCodeOrigSize << 2; //quadruple the size
        int qrCodeBackgroundSize = (qrCodeActualSize*3)/2;
        u32 qrCodeBackgroundColor = (retVal == 0) ? 0xAB7213u : 0xFFu; //bsod blue or very red

        int qrCodeHorizStart = (framebufferLineWidth>>1)-(qrCodeBackgroundSize>>1);
        u32* frameBufferRowPtr = &lfb_base[qrCodeVertStart*framebufferLineStride+qrCodeHorizStart];
        for (int y=0; y<qrCodeBackgroundSize; y++)
        {
            for (int x=0; x<qrCodeBackgroundSize; x++)
                frameBufferRowPtr[x] = qrCodeBackgroundColor;

            frameBufferRowPtr += framebufferLineStride;
        }

        qrCodeVertStart += (qrCodeBackgroundSize-qrCodeActualSize)/2;
        qrCodeHorizStart = (framebufferLineWidth>>1)-(qrCodeActualSize>>1);
        u32* qrLineVideoData = alloca(qrCodeActualSize*sizeof(u32));
        frameBufferRowPtr = &lfb_base[qrCodeVertStart*framebufferLineStride+qrCodeHorizStart];
        for (int y=0; y<qrCodeOrigSize; y++)
        {
            u32* lineDataPtr = qrLineVideoData;
            for (int x=0; x<qrCodeOrigSize; x++)
            {
                bool currBit = qrcodegen_getModule(qrDataBuf, x, y);
                u32 pixelColor = currBit ? 0x00000000 : 0xFFFFFFFF;
                for (u32 i=0; i<(qrCodeActualSize/qrCodeOrigSize); i++)
                    *lineDataPtr++ = pixelColor;
            }

            for (u32 i=0; i<(qrCodeActualSize/qrCodeOrigSize); i++)
            {
                memcpy(frameBufferRowPtr, qrLineVideoData, qrCodeActualSize*sizeof(u32));
                frameBufferRowPtr += framebufferLineStride;
            }
        }
    }

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

        int rowIdx = smileyVertStart*framebufferLineStride;
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
            rowIdx += framebufferLineStride;
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
