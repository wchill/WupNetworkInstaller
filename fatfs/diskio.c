/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include <iosuhax.h>
#include <coreinit/filesystem.h>
#include <coreinit/time.h>
#include "../ios_fs.h"

/* Definitions of physical drive number for each drive */
#define DEV_SD		0
#define DEV_USB_EXT 1

#define SD_PATH		"/dev/sdcard01"
#define USB_EXT_PATH	"/dev/usb02"


static int deviceFds[] = {-1, -1};
static const char *devicePaths[] = {SD_PATH, USB_EXT_PATH};


static int usbFd = -1;
static FSClient fsClient;


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	if (pdrv < 0 || pdrv >= FF_VOLUMES) return STA_NOINIT;
	if (deviceFds[pdrv] < 0) {
		return STA_NOINIT;
	}
	return 0;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
	if (pdrv < 0 || pdrv >= FF_VOLUMES) return STA_NOINIT;
	if (initFs(&fsClient) < 0) {
		return STA_NOINIT;
	}

	int res = FSA_RawOpen(&fsClient, USB_PATH, &deviceFds[pdrv]);
	if (res < 0) return STA_NOINIT;
	if (usbFd < 0) return STA_NOINIT;

	return 0;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	if (pdrv < 0 || pdrv >= FF_VOLUMES) return STA_NOINIT;
	// sector size 512 bytes
	if (usbFd < 0) return RES_NOTRDY;
	int res = FSA_RawRead(&fsClient, buff, 512, count, sector, deviceFds[pdrv]);
    if (res < 0) return RES_ERROR;

	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if (pdrv < 0 || pdrv >= FF_VOLUMES) return STA_NOINIT;
	if (deviceFds[pdrv] < 0) return RES_NOTRDY;
	int res = FSA_RawWrite(&fsClient, (const void*) buff, 512, count, sector, deviceFds[pdrv]);
    if (res < 0) return RES_ERROR;

	return RES_OK;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	if (pdrv < 0 || pdrv >= FF_VOLUMES) return STA_NOINIT;
	int res;
	uint8_t ioctl_buf[0x28];

	if (deviceFds[pdrv] < 0) return RES_NOTRDY;
	const char *devicePath = devicePaths[pdrv];

	switch (cmd) {
		case CTRL_SYNC:
			res = FSA_FlushVolume(&fsClient, devicePath);
			if (res) return RES_ERROR;
			return RES_OK;
		case GET_SECTOR_COUNT:
			res = FSA_GetDeviceInfo(&fsClient, devicePath, 0x08, ioctl_buf);
			if (res) return RES_ERROR;
			*(LBA_t*)buff = *(uint64_t*)&ioctl_buf[0x08];
			return RES_OK;
		case GET_SECTOR_SIZE:
			res = FSA_GetDeviceInfo(&fsClient, devicePath, 0x08, ioctl_buf);
			if (res) return RES_ERROR;
			*(WORD*)buff = *(uint32_t*)&ioctl_buf[0x10];
			return RES_OK;
		case GET_BLOCK_SIZE:
			*(WORD*)buff = 1;
			return RES_OK;
		case CTRL_TRIM:
			return RES_OK;
	}

	return RES_PARERR;
}

DWORD get_fattime() {
	OSCalendarTime output;
	OSTicksToCalendarTime(OSGetTime(), &output);
	return (DWORD)(output.tm_year - 1980) << 25 |
		(DWORD)(output.tm_mon + 1) << 21 |
		(DWORD)output.tm_mday << 16 |
		(DWORD)output.tm_hour << 11 |
		(DWORD)output.tm_min << 5 |
		(DWORD)output.tm_sec >> 1;
}