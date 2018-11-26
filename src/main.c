#include "utils.h"
#include "lib/printk.h"
#include "display/video_fb.h"

#include "hwinit/btn.h"
#include "hwinit/i2c.h"
#include "hwinit/tsec.h"
#include "hwinit/hwinit.h"
#include "hwinit/di.h"
#include "hwinit/mc.h"
#include "hwinit/t210.h"
#include "hwinit/sdmmc.h"
#include "hwinit/sdmmc_driver.h"
#include "hwinit/timer.h"
#include "fuse.h"
#include "se.h"
#include "key_derivation.h"
#include "exocfg.h"
#include "smiley.h"
#include "lib/heap.h"
#include "lib/crc32.h"
#include "lib/qrcodegen.h"
#include "rcm_usb.h"
#include "storage.h"
#include "lib/ff.h"
#include <alloca.h>
#include <string.h>
#define XVERSION 7

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

static NOINLINE int tsec_key_readout(sdmmc_storage_t* mmc, void* outBuf)  //noinline so we get the stack space back
{
    u8 carveoutData[0x2A00];
    u32 carveoutCurrIdx = 0;
    
    static const u32 PKG1LDR_OFFSET = 0x100000;
    static const u32 PKG1LDR_SIZE = 0x40000;    
    static const u32 SML_TSECFW_SIZE = 0xF00;
    static const u32 BIG_TSECFW_SIZE = 0x2900;

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
        if (!sdmmc_storage_read(mmc, currentSectorIdx, numSectorsToRead, readPtr))
        {
            printk("SDMMC ERROR Reading bytes 0x%08x-0x%08x\n", currentSectorIdx*SECTOR_SIZE, (currentSectorIdx+numSectorsToRead)*SECTOR_SIZE);
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
            if (carveoutCurrIdx >= BIG_TSECFW_SIZE)
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
    u32 tsecFwSize = 0;
    {
        u32 theCrc = crc32b(carveoutBytes, SML_TSECFW_SIZE);
        if (theCrc == 0xB035021F)
            tsecFwSize = SML_TSECFW_SIZE;
        else
        {
            theCrc = crc32b(carveoutBytes, BIG_TSECFW_SIZE);
            if (theCrc == 0xBEC3BC15)
                tsecFwSize = BIG_TSECFW_SIZE;
        }

        printk("TSEC FW CRC32: %08x - %s\n", theCrc, (tsecFwSize != 0) ? "CORRECT" : "INCORRECT");
    }    
    if (tsecFwSize == 0)
		return -11;
    
    int retVal = 0;
    const u32 tsecRev = 1;
    printk("TSEC using carveout 0x%08x rev %u\n", (u32)carveoutBytes, tsecRev);
    memset(outBuf,0,0x10);
    retVal = tsec_query((u32)carveoutBytes, tsecFwSize, outBuf, tsecRev);
    return retVal;
}

static const u32 FRAMEBUFFER_LINE_WIDTH = 720;
static const u32 FRAMEBUFFER_LINE_STRIDE = 768;

static NOINLINE bool drawQrCode(u32* lfb_base, const char* textBuf, int qrCodeVertStart, u32 qrCodeBackgroundColor)
{
    u8 qrDataBuf[qrcodegen_BUFFER_LEN_FOR_VERSION(20)];
    u8 qrTempBuf[qrcodegen_BUFFER_LEN_FOR_VERSION(20)];

    if (qrcodegen_encodeText(textBuf, qrTempBuf, qrDataBuf, qrcodegen_Ecc_MEDIUM, 10, 20, qrcodegen_Mask_AUTO, true))
    {
        int qrCodeOrigSize = qrcodegen_getSize(qrDataBuf);
        int qrCodeActualSize = qrCodeOrigSize << 2; //quadruple the size
        int qrCodeBackgroundSize = (qrCodeActualSize*3)/2;

        int qrCodeHorizStart = (FRAMEBUFFER_LINE_WIDTH>>1)-(qrCodeBackgroundSize>>1);
        u32* frameBufferRowPtr = &lfb_base[qrCodeVertStart*FRAMEBUFFER_LINE_STRIDE+qrCodeHorizStart];
        for (int y=0; y<qrCodeBackgroundSize; y++)
        {
            for (int x=0; x<qrCodeBackgroundSize; x++)
                frameBufferRowPtr[x] = qrCodeBackgroundColor;

            frameBufferRowPtr += FRAMEBUFFER_LINE_STRIDE;
        }

        qrCodeVertStart += (qrCodeBackgroundSize-qrCodeActualSize)/2;
        qrCodeHorizStart = (FRAMEBUFFER_LINE_WIDTH>>1)-(qrCodeActualSize>>1);
        u32* qrLineVideoData = alloca(qrCodeActualSize*sizeof(u32));
        frameBufferRowPtr = &lfb_base[qrCodeVertStart*FRAMEBUFFER_LINE_STRIDE+qrCodeHorizStart];
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
                frameBufferRowPtr += FRAMEBUFFER_LINE_STRIDE;
            }
        }

        return true;
    }
    return false;
}

static int initialize_mount(FATFS* outFS, u8 devNum)
{
    sdmmc_t* currCont = get_controller_for_index(devNum);
    sdmmc_storage_t* currStor = get_storage_for_index(devNum);

    if (currCont == NULL || currStor == NULL)
        return 0;

    if (currStor->sdmmc != NULL)
        return 1; //already initialized

    if (devNum == 0) //maybe support more ?
    {
        if (sdmmc_storage_init_sd(currStor, currCont, SDMMC_1, SDMMC_BUS_WIDTH_4, 11) && f_mount(outFS, "", 1) == FR_OK)
            return 1;
        else
        {
            if (currStor->sdmmc != NULL)
                sdmmc_storage_end(currStor, 0);

            memset(currCont, 0, sizeof(sdmmc_t));
            memset(currStor, 0, sizeof(sdmmc_storage_t));
        }
    }

	return 0;
}

static void deinitialize_storage()
{
    f_unmount("");
    for (u32 i=0; i<FF_VOLUMES; i++)
    {
        sdmmc_storage_t* stor = get_storage_for_index((u8)i);
        if (stor != NULL && stor->sdmmc != NULL)
        {
            if (!sdmmc_storage_end(stor, 1))
                dbg_print("sdmmc_storage_end for storage idx %u FAILED!\n", i);
            else
                memset(stor, 0, sizeof(sdmmc_storage_t));
        }
    }
}

int main(void) {
    char textBuf[1024];
    u32 tempBuf[0x20/sizeof(u32)];
    int currTextBufPos = 0;
    int lastLineLength = 0;
    u32* lfb_base;    

    config_hw();
    display_enable_backlight(false);
    display_init();

    // Set up the display, and register it as a printk provider.
    lfb_base = display_init_framebuffer();
    video_init(lfb_base);

    //Tegra/Horizon configuration goes to 0x80000000+, package2 goes to 0xA9800000, we place our heap in between.
	heap_init(0x90020000);

    printk("                                         biskeydump\n");
    printk("                                            v%d\n", XVERSION);

    /* Turn on the backlight after initializing the lfb */
    /* to avoid flickering. */
    display_enable_backlight(true);

    #define REMAINING_TEXT_BYTES(bufVar, bufPosVar) ((((int)sizeof(bufVar)-(int)bufPosVar-1) > 0) ? (sizeof(bufVar)-bufPosVar-1) : 0)

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

    sdmmc_storage_t mmcPart;
    sdmmc_t mmcDev;

    memset(&mmcPart,0,sizeof(mmcPart));
    memset(&mmcDev,0,sizeof(mmcDev));

    int retVal = -13;
    mc_enable_ahb_redirect(); //needed for sdmmc as well
    if (!sdmmc_storage_init_mmc(&mmcPart, &mmcDev, SDMMC_4, SDMMC_BUS_WIDTH_8, 4))
        printk("ERROR initializing SDMMC!\n");
    else
    {
        if (!sdmmc_storage_set_mmc_partition(&mmcPart, 1))
            printk("ERROR switching to boot0 partition!\n");
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
    printk("\n fusee gelee exploit by ktemkin (and others), hwinit by naehrwert, biskeydump by rajkosto\n");
    sdmmc_storage_end(&mmcPart, 0);

    const int smileySize = 128;
    const int smileyVertStart = 128*3;
    const int smileyHorizStart = (FRAMEBUFFER_LINE_WIDTH/2)-(smileySize/2);    

    int qrCodeVertStart = smileyVertStart + ((smileySize*3)/2);
    drawQrCode(lfb_base, textBuf, qrCodeVertStart, (retVal == 0) ? 0xAB7213u : 0xFFu);

	//output to usb RCM host if it's listening
    if (rcm_usb_device_ready())
    {
        rcm_usb_device_write_ep1_in_sync((u8*)textBuf, currTextBufPos, NULL);
        rcm_usb_device_reset_ep1();
    }

    if (retVal == 0)
        printk("\n\n\n\n\n\n\n\n\n\n\n\n        PRESS VOL- TO SAVE KEYS TO MICROSD OR PRESS POWER TO TURN OFF THE CONSOLE");

    int currRow = video_get_row();
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

        int rowIdx = smileyVertStart*FRAMEBUFFER_LINE_STRIDE;
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
            rowIdx += FRAMEBUFFER_LINE_STRIDE;
            uv.y -= incPixel;

            // Check for button press
            if (retVal == 0 && btn_read() == BTN_VOL_DOWN)
                goto writetosd;
            else if (btn_read() == BTN_POWER)
                goto progend;
        }

        lastTime = fTime;
    }    
writetosd:
    video_reposition(0,0);
    while ((currRow--) >= 0)
    {
        printk(" ");
        video_clear_line();
        printk("\n");
    }

    video_reposition(0,0);
    FATFS fs;
    memset(&fs, 0, sizeof(FATFS));
    while (!initialize_mount(&fs, 0))
    {
        memset(&fs, 0, sizeof(FATFS));
		printk("Failed to mount SD card, press VOL+ to try again or POWER to power off\n");
        u32 btnRes;
        for (;;)
        {
            btnRes = btn_read();
            if ((btnRes & (BTN_POWER | BTN_VOL_UP)) == 0)
                msleep(1);
            else
                break;
        }
        if (btnRes & BTN_VOL_UP)
        {
            msleep(200);
            continue;
        }
        else if (btnRes & BTN_POWER)
            goto progend;
    }

    printk("microSD mounted\n");
    static const char* KEYS_FILENAME = "device.keys";

    FILINFO finfo;
    memset(&finfo, 0, sizeof(finfo));
    FRESULT res = f_stat(KEYS_FILENAME, &finfo);
    if (res == FR_OK)
    {
        if (finfo.fattrib & AM_DIR)
        {
            printk("%s on the microSD is a directory, cannot continue!\n", KEYS_FILENAME);
            goto sdend;
        }
        else
        {
            printk("%s on the microSD already exists, press VOL+ to overwrite or POWER to cancel\n", KEYS_FILENAME);
            u32 btnRes;
            for (;;)
            {
                btnRes = btn_read();
                if ((btnRes & (BTN_VOL_UP | BTN_POWER)) == 0)
                    msleep(1);
                else
                    break;
            }
            if (btnRes & BTN_VOL_UP)
            {
                res = f_unlink(KEYS_FILENAME);
                if (res == FR_OK)
                    printk("Successfully deleted existing %s on the microSD\n", KEYS_FILENAME);
                else
                {
                    printk("Error %d deleting existing %s on the microSD!\n", res, KEYS_FILENAME);
                    goto sdend;
                }
            }
            else if (btnRes & BTN_POWER)
                goto sdend;
        }
    }

    FIL filHndl;
    memset(&filHndl, 0, sizeof(filHndl));
    res = f_open(&filHndl, KEYS_FILENAME, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK)
    {
        printk("Error %d opening %s on the microSD, cannot continue!\n", res, KEYS_FILENAME);
        goto sdend;
    }
    else
        printk("Opened %s on the microSD for writing\n", KEYS_FILENAME);

    {
        char* sbk_hex = NULL;
        char* tsec_hex = NULL;
        char* devkey_hex = NULL;
        char* biskey_crypt_hex[4] = {NULL};
        char* biskey_tweak_hex[4] = {NULL};

        const char* foundPos = NULL;
        
        static const char SBK_SEARCH_STR[] = "SBK:";
        foundPos = strstr((foundPos == NULL) ? textBuf : foundPos, SBK_SEARCH_STR);
        if (foundPos != NULL)
        {
            foundPos += sizeof(SBK_SEARCH_STR);
            const char* nlPos = strchr(foundPos, '\n');
            if (nlPos == NULL) nlPos = &textBuf[currTextBufPos];
            sbk_hex = strndup(foundPos, nlPos-foundPos);
            foundPos = nlPos;
        }

        static const char TSEC_SEARCH_STR[] = "TSEC KEY:";
        foundPos = strstr((foundPos == NULL) ? textBuf : foundPos, TSEC_SEARCH_STR);
        if (foundPos != NULL)
        {
            foundPos += sizeof(TSEC_SEARCH_STR);
            const char* nlPos = strchr(foundPos, '\n');
            if (nlPos == NULL) nlPos = &textBuf[currTextBufPos];
            tsec_hex = strndup(foundPos, nlPos-foundPos);
            foundPos = nlPos;
        }

        static const char DEVKEY_SEARCH_STR[] = "DEVICE KEY:";
        foundPos = strstr((foundPos == NULL) ? textBuf : foundPos, DEVKEY_SEARCH_STR);
        if (foundPos != NULL)
        {
            foundPos += sizeof(DEVKEY_SEARCH_STR);
            const char* nlPos = strchr(foundPos, '\n');
            if (nlPos == NULL) nlPos = &textBuf[currTextBufPos];
            devkey_hex = strndup(foundPos, nlPos-foundPos);
            foundPos = nlPos;
        }

        char bisKeySearchStrCrypt[] = "BIS KEY 0 (crypt):";
        char bisKeySearchStrTweak[] = "BIS KEY 0 (tweak):";

        const int bisKeySearchIdxChar = strchr(bisKeySearchStrCrypt, '0') - bisKeySearchStrCrypt;
        for (int i=0; i<4; i++)
        {
            bisKeySearchStrCrypt[bisKeySearchIdxChar] = '0' + i;
            bisKeySearchStrTweak[bisKeySearchIdxChar] = '0' + i;

            foundPos = strstr((foundPos == NULL) ? textBuf : foundPos, bisKeySearchStrCrypt);
            if (foundPos != NULL)
            {
                foundPos += sizeof(bisKeySearchStrCrypt);
                const char* nlPos = strchr(foundPos, '\n');
                if (nlPos == NULL) nlPos = &textBuf[currTextBufPos];
                biskey_crypt_hex[i] = strndup(foundPos, nlPos-foundPos);
                foundPos = nlPos;
            }
            foundPos = strstr((foundPos == NULL) ? textBuf : foundPos, bisKeySearchStrTweak);
            if (foundPos != NULL)
            {
                foundPos += sizeof(bisKeySearchStrTweak);
                const char* nlPos = strchr(foundPos, '\n');
                if (nlPos == NULL) nlPos = &textBuf[currTextBufPos];
                biskey_tweak_hex[i] = strndup(foundPos, nlPos-foundPos);
                foundPos = nlPos;
            }
        }

        currTextBufPos = 0;
        textBuf[currTextBufPos] = 0;

        if (sbk_hex != NULL)
        {
            currTextBufPos += snprintfk(&textBuf[currTextBufPos], REMAINING_TEXT_BYTES(textBuf, currTextBufPos), "secure_boot_key = %s\r\n", sbk_hex);
            free(sbk_hex); sbk_hex = NULL;
        }

        if (tsec_hex != NULL)
        {
            currTextBufPos += snprintfk(&textBuf[currTextBufPos], REMAINING_TEXT_BYTES(textBuf, currTextBufPos), "tsec_key = %s\r\n", tsec_hex);
            free(tsec_hex); tsec_hex = NULL;
        }

        if (devkey_hex != NULL)
        {
            currTextBufPos += snprintfk(&textBuf[currTextBufPos], REMAINING_TEXT_BYTES(textBuf, currTextBufPos), "device_key = %s\r\n", devkey_hex);
            free(devkey_hex); devkey_hex = NULL;
        }

        for (int i=0; i<4; i++)
        {
            if (biskey_crypt_hex[i] == NULL && biskey_tweak_hex[i] == NULL)
                continue;

            currTextBufPos += snprintfk(&textBuf[currTextBufPos], REMAINING_TEXT_BYTES(textBuf, currTextBufPos), 
                                        "bis_key_0%d = %s%s\r\n", i, biskey_crypt_hex[i], biskey_tweak_hex[i]);

            if (biskey_crypt_hex[i] != NULL) { free(biskey_crypt_hex[i]); biskey_crypt_hex[i] = NULL; }
            if (biskey_tweak_hex[i] != NULL) { free(biskey_tweak_hex[i]); biskey_tweak_hex[i] = NULL; }
        }
    }

    res = f_write(&filHndl, textBuf, currTextBufPos, NULL);
    if (res != FR_OK)
        printk("Error %d writing %d bytes to %s on the microSD\n", res, currTextBufPos, KEYS_FILENAME);
    else
        printk("Written %d bytes to %s on the microSD\n", currTextBufPos, KEYS_FILENAME);

    res = f_close(&filHndl);
    if (res != FR_OK)
        printk("Error %d closing %s on the microSD, cannot continue!\n", res, KEYS_FILENAME);

sdend:
    deinitialize_storage();
    printk("\nPress POWER to turn off the console\n");
    while (btn_read() != BTN_POWER) { msleep(1); }
progend:
    mc_disable_ahb_redirect();
    // Tell the PMIC to turn everything off
    shutdown_using_pmic();

    /* Do nothing for now */
    return 0;
}
