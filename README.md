# biskeydump ![License](https://img.shields.io/badge/License-GPLv2-blue.svg)
Dumps all your Switch BIS keys for eMMC contents decryption, to be used with fusée gelée.

With all your BIS keys and your RawNand.bin (or the physical eMMC attached via microSD reader or using a mass storage gadget mode in u-boot/linux) you can explore/modify your eMMC partitions using my HacDiskMount tool (if running Windows) from https://switchtools.sshnuke.net

## Usage

 1. Place your own tsec fw as a C hex array or escaped string into the file src/hwinit/tsecfw.inl
 2. Build `biskeydump.bin` using make in repository root directory
 3. Run fusée gelée with `biskeydump.bin` instead of `fusee.bin` (sudo ./fusee-launcher.py biskeydump.bin)

## Changes

This section is required by the original license of Atmosphere, GPLv2.

 * This originates from https://github.com/Atmosphere-NX/Atmosphere.
 * everything except fusee-primary and key_derivation/masterkey/exocfg from fusee-secondary has been removed.
 * tsec.c has been slightly modified (to include fw from tsecfw.inl, and print out crc32 of used fw), hwinit.c has been modified to allow parameters to mc_enable_ahb_redirect
 * miniz (from https://github.com/richgel999/miniz) has been included to facilitate crc32 used in tsec.c
 * main.c has been modified to dump tsec key and store it into a global, then call key_derivation.c functions and dump bis keys.
 * key_derivation.c has been modified to use dumped tsec key and not use any keyblob data (via #ifdef guards)

## Responsibility

**I am not responsible for anything, including dead switches, loss of life, or total nuclear annihalation.**
