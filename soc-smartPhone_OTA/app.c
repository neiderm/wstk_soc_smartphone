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

/* Own header */
#include "app.h"

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
static uint8_t heart_rate_measurement = 0xff;

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
                printLog("\r\nBoot! ........ \r\n");
      }
//fall-thru ...
    case gecko_evt_le_connection_closed_id:

      if (gecko_evt_le_connection_closed_id == BGLIB_MSG_ID(evt->header)) { // GN:
        printLog("connection closed, reason: 0x%2.2x\r\n", evt->data.evt_le_connection_closed.reason);

        /* Store the connection ID */
        activeConnectionId = 0xFF; /* delete the connection ID */
      }
      /* Restart advertising after client has disconnected ?????  */
///*
// *  GN: what about ...
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

      printLog("gecko_evt_gatt_server_user_read_request_id, ...: (%d) \r\n", evt->data.evt_gatt_server_user_read_request.characteristic); flushLog();

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
      /* Check which software timer handle is in question */
      switch (evt->data.evt_hardware_soft_timer.handle) {
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
          break;
      }
      break;

    /* User write request event. Checks if the user-type OTA Control Characteristic was written.
     * If written, boots the device into Device Firmware Upgrade (DFU) mode. */
    case gecko_evt_gatt_server_user_write_request_id:
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

      switch(evt->data.evt_gatt_server_user_write_request.characteristic){
      case  gattdb_battery_level:

//        battSet(evt->data.evt_gatt_server_attribute_value.value.data[0]); // gecko_cmd_gatt_server_send_characteristic_notification()

        // GN: gecko_cmd_gatt_server_send_user_write_response()  ?????

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
        printLog("(gecko_evt_le_connection_parameters_id) Event =='%08x' \r\n", BGLIB_MSG_ID(evt->header)); flushLog();
      break;
    case gecko_evt_le_connection_phy_status_id:
        printLog("(gecko_evt_le_connection_phy_status_id) Event =='%08x' \r\n", BGLIB_MSG_ID(evt->header)); flushLog();
      break;
    case gecko_evt_gatt_mtu_exchanged_id:
        printLog("(gecko_evt_gatt_mtu_exchanged_id) Event =='%08x' \r\n", BGLIB_MSG_ID(evt->header)); flushLog();
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
