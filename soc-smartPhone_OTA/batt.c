/***********************************************************************************************//**
 * \file   batt.c
 * \brief  Battery Service
 ***************************************************************************************************
 * <b> (C) Copyright 2015 Silicon Labs, http://www.silabs.com</b>
 ***************************************************************************************************
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 **************************************************************************************************/


/* standard library headers */
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/* BG stack headers */
#include "bg_types.h"
#include "gatt_db.h"
#ifdef HOST
#include "gecko_bglib.h"
#else /* !HOST */
#include "native_gecko.h"
#endif /* !HOST */
#include "infrastructure.h"

/* plugin headers */
#include "app_hw.h"
#include "app.h"

/* Own header*/
#include "batt.h"

/***********************************************************************************************//**
 * @addtogroup Features
 * @{
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup batt
 * @{
 **************************************************************************************************/

/***************************************************************************************************
  Local Macros and Definitions
***************************************************************************************************/

#define DUMMY_BATT_LEVEL 		(99U)
/** Battery measurement period in ms. */
#define BATT_IND_TIMEOUT                SILABS_AF_PLUGIN_BATT_NOTIFICATION_INT

/** Indicates currently there is no active connection using this service. */
#define BATT_NO_CONNECTION                   0xFF

/***************************************************************************************************
 Local Type Definitions
 **************************************************************************************************/

/***************************************************************************************************
 Local Variables
 **************************************************************************************************/

static uint8 battBatteryLevel = DUMMY_BATT_LEVEL; /* Battery Level */

/***************************************************************************************************
 Static Function Declarations
 **************************************************************************************************/

/***************************************************************************************************
 Public Variable Definitions
 **************************************************************************************************/


/***************************************************************************************************
 Public Function Definitions
 **************************************************************************************************/

void battInit(void)
{
}

void battCharStatusChange(uint8_t connection, uint16_t clientConfig)
{
/* if the new value of CCC is not 0 (either indication or notification enabled)
   *  start battery level measurement */
  if (clientConfig)
  {
// printLog("battCharStatusChange: battMeasure( clientConfig == %d )\r\n", clientConfig); flushLog();

    battMeasure(); /* make an initial measurement */
  }
}

void battMeasure(void)
{
//  if (battBatteryLevel  > 0){
//	battBatteryLevel  -= 1;
//  } else {
//   /* Update battery level based on battery level sensor */
//   battBatteryLevel = DUMMY_BATT_LEVEL;
//  }

  /* Send notification */
  gecko_cmd_gatt_server_send_characteristic_notification(
	conGetConnectionId(), gattdb_battery_level, sizeof(battBatteryLevel), &battBatteryLevel);
}

void battRead(void)
{
  /* Update battery level based on battery level sensor */
  if (battBatteryLevel < DUMMY_BATT_LEVEL){
    battBatteryLevel += 1;//= DUMMY_BATT_LEVEL;
  }
  else {
    battBatteryLevel =0;
  }

  /* Send response to read request */
  gecko_cmd_gatt_server_send_user_read_response(conGetConnectionId(), gattdb_battery_level, 0,
		  sizeof(battBatteryLevel), &battBatteryLevel);

printLog("battReadP: batteryVoltagePct == %d \r\n", battBatteryLevel); flushLog(); // tmp
}

void battSet(int amount){
// printLog(" battReset \r\n" ); flushLog();
	battBatteryLevel = amount;

	battMeasure(); // calls the cmd_gatt_server_send_characteristic_notification()
}


/** @} (end addtogroup batt) */
/** @} (end addtogroup Features) */

