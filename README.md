# biskeydump ![License](https://img.shields.io/badge/License-GPLv2-blue.svg)
Dumps all your Switch BIS keys for eMMC contents decryption, to be used with fusée gelée.

With all your BIS keys and your RawNand.bin (or the physical eMMC attached via microSD reader or using a mass storage gadget mode in u-boot/linux) you can explore/modify your eMMC partitions using my HacDiskMount tool (if running Windows) from https://switchtools.sshnuke.net

## Usage
 1. Build `biskeydump.bin` using make from the repository root directory, or download a binary release from https://switchtools.sshnuke.net
 2. Send the biskeydump.bin to your Switch running in RCM mode via a fusee-launcher (sudo ./fusee-launcher.py biskeydump.bin or just drag and drop it onto TegraRcmSmash.exe on Windows)
 3. Either read out and note down the text printed on your Switch's screen, or scan the generated QR code with your phone to have a copy of all your device-specific keys

## Changes

This section is required by the original license of Atmosphere, GPLv2.

 * This originates from https://github.com/Atmosphere-NX/Atmosphere
 * everything except fusee-primary and key_derivation/masterkey/exocfg from fusee-secondary has been removed
 * tsec.c has been slightly modified (to not try and copy fw from static array)
 * sdmmc.c has been modified to have simple switch to boot0 function and only output debug info under an ifdef (default disabled)
 * miniz (from https://github.com/richgel999/miniz) has been included to facilitate crc32 used in tsec.c
 * qrcodegen (from https://github.com/nayuki/QR-Code-generator) has been included so that a QR code image of the dumped data can be displayed
 * main.c has been modified to get tsec fw, query for tsec key then call key_derivation.c functions using that key, then dump device and bis keys
 * key_derivation.c has been modified to use passed-in tsec key and not use any keyblob data (via #ifdef guards)

## Responsibility

**I am not responsible for anything, including dead switches, loss of life, or total nuclear annihilation.**
