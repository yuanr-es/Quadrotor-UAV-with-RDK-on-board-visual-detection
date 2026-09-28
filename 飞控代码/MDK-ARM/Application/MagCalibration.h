#ifndef __MAG_CALIBRATION_H
#define __MAG_CALIBRATION_H

#include "FreeRTOS.h"
#include "task.h"
#include "Compass.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MAG_CAL_STATE_IDLE = 0,
    MAG_CAL_STATE_WAIT_START,
    MAG_CAL_STATE_COLLECTING,
    MAG_CAL_STATE_VALIDATING,
    MAG_CAL_STATE_READY_DELAY,
    MAG_CAL_STATE_WAIT_LOCK,
    MAG_CAL_STATE_READY,
    MAG_CAL_STATE_FAILED,
    MAG_CAL_STATE_ABORTED
} MagCalibrationState;

typedef enum
{
    MAG_CAL_ENTRY_NONE = 0,
    MAG_CAL_ENTRY_FIRST_BOOT,
    MAG_CAL_ENTRY_REMOTE,
    MAG_CAL_ENTRY_SERIAL
} MagCalibrationEntry;

void MagCalibration_Init(void);
bool MagCalibration_RequestStart(MagCalibrationEntry entry);
void MagCalibration_Update(const CompassRawData *raw_data, TickType_t now_tick);
void MagCalibration_Service(TickType_t now_tick, bool remote_online, bool remote_locked, bool throttle_low);
void MagCalibration_Abort(void);
bool MagCalibration_IsBusy(void);
bool MagCalibration_IsFlightInhibited(void);
bool MagCalibration_HasValidData(void);
MagCalibrationState MagCalibration_GetState(void);
MagCalibrationEntry MagCalibration_GetEntry(void);
const MagCalibrationData *MagCalibration_GetActiveData(void);
void MagCalibration_PrintInfo(void);

#endif
