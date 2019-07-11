/***************************************************************************//**
 * @file app.c
 * @brief Silicon Labs Empty Example Project
 *
 * This example demonstrates the bare minimum needed for a Blue Gecko C application
 * that allows Over-the-Air Device Firmware Upgrading (OTA DFU). The application
 * starts advertising after boot and restarts advertising after a connection is closed.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 * GN: suggested reading SiliconLabs application Notes KB-BT-0801, KB-BT-0807, AN1086
 *
 ******************************************************************************/

/* Bluetooth stack headers */
#include "bg_types.h"
#include "native_gecko.h"
#include "gatt_db.h"

#include "btl_interface.h"
#include "btl_interface_storage.h"

#include "app.h"

/* Print boot message */
//static
void bootMessage(struct gecko_msg_system_boot_evt_t *bootevt);

/* Flag for indicating DFU Reset must be performed */
static uint8_t boot_to_dfu = 0;

#if 1 // GN:
static BootloaderInformation_t bldInfo;
static BootloaderStorageSlot_t slotInfo;

/* OTA variables */
#if 0
static uint32 ota_image_position = 0;
static uint8 ota_in_progress = 0;
static uint8 ota_image_finished = 0;
static uint16 ota_time_elapsed = 0;
#else
extern uint32 ota_image_position;
extern uint8 ota_in_progress;
extern uint8 ota_image_finished;
extern uint16 ota_time_elapsed;
#endif

static void print_progress();
#if 0
bool get_ota_image_finished(void){
	return (0 != ota_image_finished);
}
bool get_ota_in_progress(void){
	ota_time_elapsed++;
	print_progress();
	return (0 != ota_in_progress);
}
void ota_update_ota_control(void){}
void ota_update_ota_data(void){}
#endif

int32_t get_slot_info()
{
	int32_t err;

	bootloader_getInfo(&bldInfo);
	printf("Gecko bootloader version: %u.%u\r\n", (bldInfo.version & 0xFF000000) >> 24, (bldInfo.version & 0x00FF0000) >> 16);

	err = bootloader_getStorageSlotInfo(0, &slotInfo);

	if(err == BOOTLOADER_OK)
	{
		printf("Slot 0 starts @ 0x%8.8x, size %u bytes\r\n", slotInfo.address, slotInfo.length);
	}
	else
	{
		printf("Unable to get storage slot info, error %x\r\n", err);
	}

	return(err);
}

void erase_slot_if_needed()
{
	uint32_t offset = 0;
	uint8_t buffer[256];
	int i;
	int dirty = 0;
	int32_t err = BOOTLOADER_OK;
	int num_blocks = 0;

	/* check the download area content by reading it in 256-byte blocks */

	num_blocks = slotInfo.length / 256;

	while((dirty == 0) && (offset < 256*num_blocks) && (err == BOOTLOADER_OK))
	{
		err = bootloader_readStorage(0, offset, buffer, 256);
		if(err == BOOTLOADER_OK)
		{
			i=0;
			while(i<256)
			{
				if(buffer[i++] != 0xFF)
				{
					dirty = 1;
					break;
				}
			}
			offset += 256;
		}
		printf(".");
	}

	if(err != BOOTLOADER_OK)
	{
		printf("error reading flash! %x\r\n", err);
	}
	else if(dirty)
	{
		printf("download area is not empty, erasing...\r\n");
		bootloader_eraseStorageSlot(0);
		printf("done\r\n");
	}
	else
	{
		printf("download area is empty\r\n");
	}

	return;
}

static void print_progress()
{
	// estimate transfer speed in kbps
	int kbps = ota_image_position*8/(1024*ota_time_elapsed);

	printf("pos: %u, time: %u, kbps: %u\r\n", ota_image_position, ota_time_elapsed, kbps);
}

#endif // GN:


/* Print stack version and local Bluetooth address as boot message */
//static
void bootMessage(struct gecko_msg_system_boot_evt_t *bootevt)
{
#if DEBUG_LEVEL
  bd_addr local_addr;
  int i;

  printLog("stack version: %u.%u.%u\r\n", bootevt->major, bootevt->minor, bootevt->patch);
  local_addr = gecko_cmd_system_get_bt_address()->address;

  printLog("local BT device address: ");
  for (i = 0; i < 5; i++) {
    printLog("%2.2x:", local_addr.addr[5 - i]);
  }
  printLog("%2.2x\r\n", local_addr.addr[0]);
#endif
}
