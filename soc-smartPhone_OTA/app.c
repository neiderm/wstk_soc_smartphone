/***************************************************************************//**
 * @file
 * @brief Application code
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
 ******************************************************************************/

/* standard library headers */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/* BG stack headers */
#include "bg_types.h"
#include "native_gecko.h"

/* profiles */
#include "htm.h"
#include "ia.h"

/* BG stack headers*/
#include "gatt_db.h"

/* em library */
#include "em_system.h"

/* application specific headers */
#include "app_ui.h"
#include "app_hw.h"
#include "advertisement.h"
#include "beacon.h"
#include "app_timer.h"
#include "board_features.h"
#include "batt.h"
#include "btl_interface.h"
#include "btl_interface_storage.h"

/* Own header */
#include "app.h"

void bootMessage(struct gecko_msg_system_boot_evt_t *bootevt);

extern erase_slot_if_needed();
extern int32_t get_slot_info();
extern bool get_ota_image_finished(void);
extern uint8 get_ota_in_progress(void);
// tmp?
uint32 ota_image_position = 0;
uint8 ota_in_progress = 0;
uint8 ota_image_finished = 0;
uint16 ota_time_elapsed = 0;

/***********************************************************************************************//**
 * @addtogroup Application
 * @{
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup app
 * @{
 **************************************************************************************************/

/***************************************************************************************************
 * Local Macros and Definitions
 **************************************************************************************************/
#define STRLENGHT (40U)
/***************************************************************************************************
 * Local Variables
 **************************************************************************************************/
static uint8_t activeConnectionId = 0xFF; 	/* Connection Handle ID */
static char passString[STRLENGHT];

//static uint8_t bonded = 0;
static uint8_t ledState = 0;
static uint8_t heart_rate_measurement = 0x7f;

/***************************************************************************************************
 * Static Function Declarations
 **************************************************************************************************/
   #ifndef FEATURE_IOEXPANDER
/* Periodically called Display Polarity Inverter Function for the LCD.
   Toggles the the EXTCOMIN signal of the Sharp memory LCD panel, which prevents building up a DC
   bias according to the LCD's datasheet */
static void (*dispPolarityInvert)(void *);
  #endif /* FEATURE_IOEXPANDER */

/***************************************************************************************************
 * Function Definitions
 **************************************************************************************************/

/***********************************************************************************************//**
 * \brief Function 
 **************************************************************************************************/
uint8_t conGetConnectionId(void)
{
  /* Return connection handle ID */
  return activeConnectionId;
}

/***********************************************************************************************//**
 * \brief Function that initializes the device name, LEDs, buttons and services.
 **************************************************************************************************/
void appInit(void)
{
  /* Unique device ID */
  uint16_t devId;
  struct gecko_msg_system_get_bt_address_rsp_t* btAddr;
  char devName[APP_DEVNAME_LEN + 1];

  /* Init device name */
  /* Get the unique device ID */

  /* Create the device name based on the 16-bit device ID */
  btAddr = gecko_cmd_system_get_bt_address();
  devId = *((uint16*)(btAddr->address.addr));
  snprintf(devName, APP_DEVNAME_LEN + 1, APP_DEVNAME, devId);
  gecko_cmd_gatt_server_write_attribute_value(gattdb_device_name,
                                              0,
                                              strlen(devName),
                                              (uint8_t *)devName);

  /* Initialize LEDs, buttons, graphics. */
  appUiInit(devId);

  /* Hardware initialization. Initializes temperature sensor. */
  appHwInit();

  /* Initialize services */
  htmInit();
}

/***********************************************************************************************//**
 * \brief Event handler function
 * @param[in] evt Event pointer
 **************************************************************************************************/
void appHandleEvents(struct gecko_cmd_packet *evt)
{
  /* Flag for indicating DFU Reset must be performed */
  static uint8_t boot_to_dfu = 0;

  switch (BGLIB_MSG_ID(evt->header)) {
    /* Boot event and connection closed event */
    case gecko_evt_system_boot_id:

      if (gecko_evt_system_boot_id == BGLIB_MSG_ID(evt->header)) { // GN:

	    	  /* 1 second soft timer, used for performance statistics during OTA file upload */
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(1000), OTA_TIMER, 0);

//                printLog("\r\nBoot! ........ \r\n");
                bootMessage(&(evt->data.evt_system_boot));

    	        /* bootloader init must be called before calling other bootloader_xxx API calls */
    	        bootloader_init();

    	        /* read slot information from bootloader */
    	        if(get_slot_info() == BOOTLOADER_OK)
    	        {
    	        	/* the download area is erased here (if needed), prior to any connections are opened */
    	        	erase_slot_if_needed();
    	        }
    	        else
    	        {
    	        	printf("Check that you have installed correct type of Gecko bootloader!\r\n");
    	        }
      }
//fall-thru ...
    case gecko_evt_le_connection_closed_id:

      if (gecko_evt_le_connection_closed_id == BGLIB_MSG_ID(evt->header)) { // GN:
        printLog("connection closed, reason: 0x%2.2x\r\n", evt->data.evt_le_connection_closed.reason);

        /* Store the connection ID */
        activeConnectionId = 0xFF; /* delete the connection ID */

        if (ota_image_finished) {
   		  printf("Installing new image\r\n"); flushLog(); // uart_flush();
  	      bootloader_setImageToBootload(0);
#if 1 // GN: stop here if you don't want to install! (e.g. just perf. testing the transfer time)
  	      bootloader_rebootAndInstall();
#endif
  	    }
      }
      /* Restart advertising after client has disconnected ?????  */
///*
// *  GN: set sm mode ...
            gecko_cmd_sm_configure(0x00, sm_io_capability_displayyesno);
            gecko_cmd_sm_set_bondable_mode(1);
 //*/
      /* Initialize app */
      appInit(); /* App initialization */
      htmInit(); /* Health thermometer initialization */
      advSetup(); /* Advertisement initialization */

      /* Enter to DFU OTA mode if needed */
      if (boot_to_dfu) {
        gecko_cmd_system_reset(2);
      }

      break;

    /* Connection opened event */
    case gecko_evt_le_connection_opened_id:

      printLog("connection opened\r\n"); flushLog();

      /* Store the connection ID */
      activeConnectionId = evt->data.evt_le_connection_opened.connection;

      /* Call advertisement.c connection started callback */
      advConnectionStarted();
      break;

    /* Value of attribute changed from the local database by remote GATT client */
    case gecko_evt_gatt_server_attribute_value_id:

      printLog("_evt_gatt_server_attribute_value_id (attribute_value.attribute == %d) \r\n", evt->data.evt_gatt_server_attribute_value.attribute); flushLog();

      switch (evt->data.evt_gatt_server_attribute_value.attribute){

        /* Check if changed characteristic is the Immediate Alert level */
        case gattdb_alert_level:
          /* Write the Immediate Alert level value */
          iaImmediateAlertWrite(&evt->data.evt_gatt_server_attribute_value.value);
          break;

        default:
          printLog("unhandled: _evt_gatt_server_attribute_value_id (%d == %d) \r\n",
            evt->data.evt_gatt_server_attribute_value.attribute, evt->data.evt_gatt_server_attribute_value.value.data[0]); flushLog();
          break;
      }
      break;

    /* Indicates the changed value of CCC or received characteristic confirmation */
    case gecko_evt_gatt_server_characteristic_status_id:
      switch (evt->data.evt_gatt_server_attribute_value.attribute) 
      {
        case gattdb_temperature_measurement:
          /* Check if changed client char config is for the temperature measurement */
          if (evt->data.evt_gatt_server_characteristic_status.status_flags == 0x01) {
            /* Call HTM temperature characteristic status changed callback */
            htmTemperatureCharStatusChange(
              evt->data.evt_gatt_server_characteristic_status.connection,
              evt->data.evt_gatt_server_characteristic_status.client_config_flags);
          }
          break;

        case gattdb_battery_level:
// char-write-cmd 0x3b 0100 ... enable notification   t_server_characteristic_status.client_config_flags == 1
          printLog("gattdb_battery_level: evt->data.evt_gatt_server_characteristic_status.client_config_flags (%d) \r\n", evt->data.evt_gatt_server_characteristic_status.client_config_flags); flushLog();

          battCharStatusChange(evt->data.evt_gatt_server_characteristic_status.connection,
              evt->data.evt_gatt_server_characteristic_status.client_config_flags);
          break;

//        case gattdb_characteristic_presentation_format: // GN: nfi

        case gattdb_service_changed_char:
          printLog("gattdb_service_changed_char ?????????????? (pairing??? )  \r\n");flushLog();
          break;

        case gattdb_device_name:
          printLog("_evt_gatt_server_characteristic_status_id -> gattdb_device_name \r\n");flushLog();
          break;

        case gattdb_heart_rate_measurement:
            heart_rate_measurement += 1;
            printLog("_evt_gatt_server_characteristic_status_id -> _heart_rate_measurement (%d) ... characteristic_status.status_flags== [%d]\r\n", heart_rate_measurement, evt->data.evt_gatt_server_characteristic_status.status_flags); flushLog();
            gecko_cmd_gatt_server_send_characteristic_notification( conGetConnectionId(), gattdb_heart_rate_measurement, sizeof(heart_rate_measurement), &heart_rate_measurement);
            break;

        default:
          printLog("unhandled  _gatt_server_attribute_value.attribute ) ... evt->data.evt_gatt_server_attribute_value.attribute %d \r\n", evt->data.evt_gatt_server_attribute_value.attribute);
//            printLog("evt->data.evt_gatt_server_characteristic_status.characteristic (%d) \r\n", evt->data.evt_gatt_server_characteristic_status.characteristic);
//            printLog("evt->data.evt_gatt_server_characteristic_status.client_config_flags (%d) \r\n", evt->data.evt_gatt_server_characteristic_status.client_config_flags);
//            printLog("evt->data.evt_gatt_server_characteristic_status.status_flags (%d) \r\n", evt->data.evt_gatt_server_characteristic_status.status_flags);
          flushLog();
          break;
      }

      break;

    case gecko_evt_gatt_server_user_read_request_id:
    	/*
    	 * When reading a user characteristic longer than MTU, multiple gatt_server_user_read_request events will be generated on the server side, each
    	 * containing the offset from the beginning of the characteristic. The application code must use the offset parameter to send the correct chunk of data.
    	 */
      switch (evt->data.evt_gatt_server_user_read_request.characteristic){
        case gattdb_battery_level:
          battRead();
          break;
        default:
          printLog("gecko_evt_gatt_server_user_read_request_id, ...: (%d) \r\n", evt->data.evt_gatt_server_user_read_request.characteristic); flushLog();
          break;
      }
      break;

    /* Software Timer event */
    case gecko_evt_hardware_soft_timer_id:
#if 0 // GN: if want to see the progress bar on VCOM
  	  if (ota_in_progress)
  	  {
  		  ota_time_elapsed++;
// 	    		  print_progress();
  	  }
#endif
      /* Check which software timer handle is in question */
      switch (evt->data.evt_hardware_soft_timer.handle) {
        case OTA_TIMER: /* performance statistics during OTA file upload  */
    		  ota_time_elapsed++;
  // 	    		  print_progress();
          break;
        case UI_TIMER: /* App UI Timer (LEDs, Buttons) */
          appUiTick();
          break;
        case ADV_TIMER: /* Advertisement Timer */
          advSetup();
          break;
        case TEMP_TIMER: /* Temperature measurement timer */
          /* Make a temperature measurement */
          htmTemperatureMeasure();
          break;
        #ifndef FEATURE_IOEXPANDER
        case DISP_POL_INV_TIMER:
          /*Toggle the the EXTCOMIN signal, which prevents building up a DC bias  within the
           * Sharp memory LCD panel */
          dispPolarityInvert(0);
          break;
        #endif /* FEATURE_IOEXPANDER */
        default:
        	printf("unhandled\r\n");
          break;
      }
      break;

    /* User write request event. Checks if the user-type OTA Control Characteristic was written.
     * If written, boots the device into Device Firmware Upgrade (DFU) mode. */
    case gecko_evt_gatt_server_user_write_request_id:
#if 0 // GN: this is for boot to DFU 
      /* Handle OTA */
      if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_ota_control) {
        /* Set flag to enter to OTA mode */
        boot_to_dfu = 1;
        /* Send response to Write Request */
        gecko_cmd_gatt_server_send_user_write_response(
          evt->data.evt_gatt_server_user_write_request.connection,
          gattdb_ota_control,
          bg_err_success);

        /* Close connection to enter to DFU OTA mode */
        gecko_cmd_le_connection_close(evt->data.evt_gatt_server_user_write_request.connection);
      }
#endif
      switch(evt->data.evt_gatt_server_user_write_request.characteristic){

      case gattdb_ota_control:
          printf("characteristic == gattdb_ota_control ...... value.data[0] %d \r\n", evt->data.evt_gatt_server_user_write_request.value.data[0]);
		  switch(evt->data.evt_gatt_server_user_write_request.value.data[0])
		  {
		  case 0://Erase and use slot 0
			  // NOTE: download are is NOT erased here, because the long blocking delay would result in supervision timeout
			  //bootloader_eraseStorageSlot(0);
			  ota_image_position=0;
			  ota_in_progress=1;
			  break;
		  case 3://END OTA process
			  //wait for connection close and then reboot
			  ota_in_progress=0;
			  ota_image_finished=1;
			  printf("upload finished. received file size %u bytes\r\n", ota_image_position); flushLog(); // uart_flush();
			  break;
		  default:
			  break;
		  }
  	      gecko_cmd_gatt_server_send_user_write_response(
	  	    		evt->data.evt_gatt_server_user_write_request.connection, gattdb_ota_control,0);
		  break;

      case gattdb_ota_data:
    	if(ota_in_progress)
		{
			  bootloader_writeStorage(0,//use slot 0
					  ota_image_position,
					  evt->data.evt_gatt_server_user_write_request.value.data,
					  evt->data.evt_gatt_server_user_write_request.value.len);
			  ota_image_position+=evt->data.evt_gatt_server_user_write_request.value.len;
		}
// GN: I can uncheck write /w/ response .... !!!
///*
  	    gecko_cmd_gatt_server_send_user_write_response(
  	    		evt->data.evt_gatt_server_user_write_request.connection, gattdb_ota_data,0);
//*/
//printLog("GN: received %d\r\n", evt->data.evt_gatt_server_user_write_request.value.len);
  	    break;

      case gattdb_battery_level:
        battSet(evt->data.evt_gatt_server_attribute_value.value.data[0]); // gecko_cmd_gatt_server_send_characteristic_notification() ... needs notifications enabled?

        /* Send response to Write Request */
        gecko_cmd_gatt_server_send_user_write_response(
            evt->data.evt_gatt_server_user_write_request.connection, gattdb_battery_level, bg_err_success);

        printLog("_evt_gatt_server_user_write_request_id: characteristic '%d' = (%d) \r\n",
        		evt->data.evt_gatt_server_user_write_request.characteristic,
				evt->data.evt_gatt_server_attribute_value.value.data[0]); flushLog();
        break;

      default:
        printLog("Unhandled evt->data.evt_gatt_server_user_write_request.characteristic [_evt_gatt_server_user_write_request_id]: characteristic '%d' = (%d) \r\n",
    	  evt->data.evt_gatt_server_user_write_request.characteristic, evt->data.evt_gatt_server_attribute_value.value.data[0]); flushLog();
        break;
      }

      break; // _user_write_request_id

    case gecko_evt_le_connection_parameters_id:
    {
        struct gecko_msg_le_connection_parameters_evt_t* data = &evt->data.evt_le_connection_parameters;
        printLog("(gecko_evt_le_connection_parameters_id) parameters connection: %lu \r\n", data->connection);
        printLog("(gecko_evt_le_connection_parameters_id) parameters interval: %lu \r\n", data->interval);
        printLog("(gecko_evt_le_connection_parameters_id) parameters latency: %lu \r\n", data->latency);
        printLog("(gecko_evt_le_connection_parameters_id) parameters timeout: %lu \r\n", data->timeout);
        printLog("(gecko_evt_le_connection_parameters_id) parameters security_mode: %lu \r\n", data->security_mode);
        printLog("(gecko_evt_le_connection_parameters_id) parameters txsize: %lu \r\n", data->txsize);
        flushLog();
        break;
    }
    case gecko_evt_le_connection_phy_status_id:
        printLog("(gecko_evt_le_connection_phy_status_id) Event =='%08x' \r\n", BGLIB_MSG_ID(evt->header)); flushLog();
      break;

    case gecko_evt_gatt_mtu_exchanged_id:
    {
    	struct gecko_msg_gatt_mtu_exchanged_evt_t* data = &evt->data.evt_gatt_mtu_exchanged;
    	printLog("(gecko_evt_gatt_mtu_exchanged_id) mtu: %u \r\n", data->mtu); flushLog();
    	break;
    }

    case gecko_evt_system_external_signal_id:
    	printLog("(_evt_system_external_signal_id) xxxx: %u \r\n", 0 ); flushLog();
    	break;

    case gecko_rsp_le_connection_set_parameters_id:
    	printLog("(_rsp_le_connection_set_parameters) xxxx: %u \r\n", 0 ); flushLog();
    	break;

    default:
        printLog("UNHANDLED Event =='%08x' \r\n", BGLIB_MSG_ID(evt->header)); flushLog();
      break;
  }   // end big switch
}

/**************************************************************************//**
 * @brief   Register a callback function at the given frequency.
 *
 * @param[in] pFunction  Pointer to function that should be called at the
 *                       given frequency.
 * @param[in] argument   Argument to be given to the function.
 * @param[in] frequency  Frequency at which to call function at.
 *
 * @return  0 for successful or
 *         -1 if the requested frequency does not match the RTC frequency.
 *****************************************************************************/
int rtcIntCallbackRegister(void (*pFunction)(void*),
                           void* argument,
                           unsigned int frequency)
{
  #ifndef FEATURE_IOEXPANDER

  dispPolarityInvert =  pFunction;
  /* Start timer with required frequency */
  gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(1000 / frequency), DISP_POL_INV_TIMER, false);

  #endif /* FEATURE_IOEXPANDER */

  return 0;
}

/** @} (end addtogroup app) */
/** @} (end addtogroup Application) */
