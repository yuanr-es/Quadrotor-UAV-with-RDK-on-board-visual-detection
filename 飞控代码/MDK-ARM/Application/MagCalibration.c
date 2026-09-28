#include "MagCalibration.h"
#include "App_freeRTOS.h"
#include "Buzzer.h"
#include "Com_debug.h"
#include <math.h>
#include <string.h>

#define MAG_CAL_MIN_TIME_TICKS pdMS_TO_TICKS(20000U)
#define MAG_CAL_MAX_TIME_TICKS pdMS_TO_TICKS(90000U)
#define MAG_CAL_STABLE_TIME_TICKS pdMS_TO_TICKS(3000U)
#define MAG_CAL_VALIDATE_TIME_TICKS pdMS_TO_TICKS(10000U)
#define MAG_CAL_READY_DELAY_TICKS pdMS_TO_TICKS(3000U)
#define MAG_CAL_PROGRESS_TICKS pdMS_TO_TICKS(2000U)
#define MAG_CAL_START_DELAY_TICKS pdMS_TO_TICKS(4000U)

#define MAG_CAL_MIN_SAMPLES 500U
#define MAG_CAL_MIN_VALIDATE_SAMPLES 300U

#define MAG_CAL_MIN_AXIS_RANGE 500.0f
#define MAG_CAL_MIN_RADIUS 150.0f
#define MAG_CAL_MAX_RADIUS_RATIO 2.5f

#define MAG_CAL_MAX_VARIATION_RATIO 0.18f

#define MAG_DIRECTION_POS_X (1U << 0)
#define MAG_DIRECTION_NEG_X (1U << 1)
#define MAG_DIRECTION_POS_Y (1U << 2)
#define MAG_DIRECTION_NEG_Y (1U << 3)
#define MAG_DIRECTION_POS_Z (1U << 4)
#define MAG_DIRECTION_NEG_Z (1U << 5)

#define MAG_DIRECTION_ALL 0x3FU
typedef struct
{
    MagCalibrationState state;
    MagCalibrationEntry entry;

    TickType_t state_start_tick;
    TickType_t last_progress_beep_tick;
    TickType_t last_range_change_tick;

    uint32_t sample_count;
    uint32_t rejected_count;

    float min_x;
    float min_y;
    float min_z;

    float max_x;
    float max_y;
    float max_z;

    uint8_t direction_mask;

    MagCalibrationData active_data;
    bool active_data_valid;

    MagCalibrationData candidate_data;

    float norm_sum;
    float norm_squared_sum;
    uint32_t validation_count;

} MagCalibrationContext;

static MagCalibrationContext mag_cal;
void MagCalibration_Init(void)
{
    memset(&mag_cal, 0, sizeof(mag_cal));
    mag_cal.state = MAG_CAL_STATE_IDLE;
    mag_cal.entry = MAG_CAL_ENTRY_NONE;
    mag_cal.active_data.offset_x = 0.0f;
    mag_cal.active_data.offset_y = 0.0f;
    mag_cal.active_data.offset_z = 0.0f;
    mag_cal.active_data.scale_x = 1.0f;
    mag_cal.active_data.scale_y = 1.0f;
    mag_cal.active_data.scale_z = 1.0f;
    mag_cal.active_data_valid = false;
}
bool MagCalibration_RequestStart(MagCalibrationEntry entry)
{
    if (MagCalibration_IsBusy() || mag_cal.state == MAG_CAL_STATE_READY_DELAY ||
        mag_cal.state == MAG_CAL_STATE_WAIT_LOCK)
    {
        return false;
    }

    TickType_t now = xTaskGetTickCount();
    mag_cal.state = MAG_CAL_STATE_WAIT_START;
    mag_cal.entry = entry;
    mag_cal.state_start_tick = now;
    mag_cal.last_progress_beep_tick = now;
    mag_cal.last_range_change_tick = now;
    mag_cal.sample_count = 0U;
    mag_cal.rejected_count = 0U;
    mag_cal.validation_count = 0U;
    mag_cal.direction_mask = 0U;
    mag_cal.norm_sum = 0.0f;
    mag_cal.norm_squared_sum = 0.0f;
    mag_cal.max_x = -32767.0f;
    mag_cal.max_y = -32767.0f;
    mag_cal.max_z = -32767.0f;
    mag_cal.min_x = 32767.0f;
    mag_cal.min_y = 32767.0f;
    mag_cal.min_z = 32767.0f;
    mag_cal.active_data_valid = false;

    Buzzer_Post(BUZZER_EVENT_MAG_NEED_CAL);
    return true;
}
static void MagCalibration_UpdateDirection(const CompassRawData *raw_data)
{
    float radius_x = (mag_cal.max_x - mag_cal.min_x) * 0.5f;
    float radius_y = (mag_cal.max_y - mag_cal.min_y) * 0.5f;
    float radius_z = (mag_cal.max_z - mag_cal.min_z) * 0.5f;
    if (radius_x < 50.0f || radius_y < 50.0f || radius_z < 50.0f)
    {
        return;
    }
    float center_x = (mag_cal.max_x + mag_cal.min_x) * 0.5f;
    float center_y = (mag_cal.max_y + mag_cal.min_y) * 0.5f;
    float center_z = (mag_cal.max_z + mag_cal.min_z) * 0.5f;
    float x = (raw_data->x - center_x) / radius_x;
    float y = (raw_data->y - center_y) / radius_y;
    float z = (raw_data->z - center_z) / radius_z;
    float abs_x = fabsf(x);
    float abs_y = fabsf(y);
    float abs_z = fabsf(z);
    if (abs_x >= abs_y && abs_x >= abs_z)
    {
        mag_cal.direction_mask |= x >= 0.0f ? MAG_DIRECTION_POS_X : MAG_DIRECTION_NEG_X;
    }
    else if (abs_y >= abs_x && abs_y >= abs_z)
    {
        mag_cal.direction_mask |= y >= 0.0f ? MAG_DIRECTION_POS_Y : MAG_DIRECTION_NEG_Y;
    }
    else if (abs_z >= abs_y && abs_z >= abs_x)
    {
        mag_cal.direction_mask |= z >= 0.0f ? MAG_DIRECTION_POS_Z : MAG_DIRECTION_NEG_Z;
    }
}
static bool MagCalibration_CalculateCandidate(void)
{
    float radius_x = (mag_cal.max_x - mag_cal.min_x) * 0.5f;
    float radius_y = (mag_cal.max_y - mag_cal.min_y) * 0.5f;
    float radius_z = (mag_cal.max_z - mag_cal.min_z) * 0.5f;
    if (radius_x < MAG_CAL_MIN_RADIUS || radius_y < MAG_CAL_MIN_RADIUS || radius_z < MAG_CAL_MIN_RADIUS)
    {
        return false;
    }
    float max_radius = fmaxf(radius_x, fmaxf(radius_y, radius_z));
    float min_radius = fminf(radius_x, fminf(radius_y, radius_z));
    if (max_radius / min_radius > MAG_CAL_MAX_RADIUS_RATIO)
    {
        return false;
    }
    float average_r = (radius_x + radius_y + radius_z) / 3.0f;
    mag_cal.candidate_data.offset_x = (mag_cal.max_x + mag_cal.min_x) * 0.5f;
    mag_cal.candidate_data.offset_y = (mag_cal.max_y + mag_cal.min_y) * 0.5f;
    mag_cal.candidate_data.offset_z = (mag_cal.max_z + mag_cal.min_z) * 0.5f;
    mag_cal.candidate_data.scale_x = average_r / radius_x;
    mag_cal.candidate_data.scale_y = average_r / radius_y;
    mag_cal.candidate_data.scale_z = average_r / radius_z;
    return true;
}
void MagCalibration_Update(const CompassRawData *raw_data, TickType_t now_tick)
{
    if (raw_data == NULL)
    {
        return;
    }
    if (mag_cal.state == MAG_CAL_STATE_WAIT_START)
    {
        if ((now_tick - mag_cal.state_start_tick) >= MAG_CAL_START_DELAY_TICKS)
        {
            mag_cal.state = MAG_CAL_STATE_COLLECTING;
            mag_cal.state_start_tick = now_tick;
            mag_cal.last_range_change_tick = now_tick;
            Buzzer_Post(BUZZER_EVENT_MAG_CAL_START);
        }
        return;
    }
    if (mag_cal.state == MAG_CAL_STATE_COLLECTING)
    {
        mag_cal.sample_count++;
        bool range_changed = false;
        if (raw_data->x < mag_cal.min_x)
        {
            mag_cal.min_x = raw_data->x;
            range_changed = true;
        }
        if (raw_data->x > mag_cal.max_x)
        {
            mag_cal.max_x = raw_data->x;
            range_changed = true;
        }
        if (raw_data->y < mag_cal.min_y)
        {
            mag_cal.min_y = raw_data->y;
            range_changed = true;
        }
        if (raw_data->y > mag_cal.max_y)
        {
            mag_cal.max_y = raw_data->y;
            range_changed = true;
        }
        if (raw_data->z < mag_cal.min_z)
        {
            mag_cal.min_z = raw_data->z;
            range_changed = true;
        }
        if (raw_data->z > mag_cal.max_z)
        {
            mag_cal.max_z = raw_data->z;
            range_changed = true;
        }
        if (range_changed)
        {
            mag_cal.last_range_change_tick = now_tick;
        }
        MagCalibration_UpdateDirection(raw_data);
        if ((now_tick - mag_cal.last_progress_beep_tick) >= MAG_CAL_PROGRESS_TICKS)
        {
            mag_cal.last_progress_beep_tick = now_tick;
            Buzzer_Post(BUZZER_EVENT_MAG_CAL_PROGRESS);
        }
        uint32_t elapsed = now_tick - mag_cal.state_start_tick;
        float range_x = mag_cal.max_x - mag_cal.min_x;
        float range_y = mag_cal.max_y - mag_cal.min_y;
        float range_z = mag_cal.max_z - mag_cal.min_z;
        bool enough_ranges =
            range_x >= MAG_CAL_MIN_AXIS_RANGE && range_y >= MAG_CAL_MIN_AXIS_RANGE && range_z >= MAG_CAL_MIN_AXIS_RANGE;
        bool enough_directions = mag_cal.direction_mask == MAG_DIRECTION_ALL;
        bool stable_ranges = (now_tick - mag_cal.last_range_change_tick) >= MAG_CAL_STABLE_TIME_TICKS;
        bool enough_samples = mag_cal.sample_count >= MAG_CAL_MIN_SAMPLES;
        if (elapsed >= MAG_CAL_MIN_TIME_TICKS && enough_ranges && enough_directions && stable_ranges && enough_samples)
        {
            if (!MagCalibration_CalculateCandidate())
            {
                mag_cal.state = MAG_CAL_STATE_FAILED;
                Buzzer_Post(BUZZER_EVENT_MAG_CAL_FAILED);
                return;
            }
            mag_cal.state = MAG_CAL_STATE_VALIDATING;
            mag_cal.state_start_tick = now_tick;
            mag_cal.norm_sum = 0.0f;
            mag_cal.norm_squared_sum = 0.0f;
            mag_cal.validation_count = 0;
            Buzzer_Post(BUZZER_EVENT_MAG_CAL_VALIDATE);
            return;
        }
        if (elapsed >= MAG_CAL_MAX_TIME_TICKS)
        {
            mag_cal.state = MAG_CAL_STATE_FAILED;
            Buzzer_Post(BUZZER_EVENT_MAG_CAL_FAILED);
        }
        return;
    }
    if (mag_cal.state == MAG_CAL_STATE_VALIDATING)
    {
        float x = ((float)raw_data->x - mag_cal.candidate_data.offset_x) * mag_cal.candidate_data.scale_x;
        float y = ((float)raw_data->y - mag_cal.candidate_data.offset_y) * mag_cal.candidate_data.scale_y;
        float z = ((float)raw_data->z - mag_cal.candidate_data.offset_z) * mag_cal.candidate_data.scale_z;
        float norm = sqrtf(x * x + y * y + z * z);
        if (!isfinite(norm) || norm < 100.0f || norm > 2000.0f)
        {
            mag_cal.rejected_count++;
        }
        else
        {
            mag_cal.norm_sum += norm;
            mag_cal.norm_squared_sum += norm * norm;
            mag_cal.validation_count++;
        }
        if ((now_tick - mag_cal.last_progress_beep_tick) >= 2000U)
        {
            mag_cal.last_progress_beep_tick = now_tick;
            Buzzer_Post(BUZZER_EVENT_MAG_CAL_PROGRESS);
        }
        if ((now_tick - mag_cal.state_start_tick) >= MAG_CAL_VALIDATE_TIME_TICKS)
        {
            if (mag_cal.validation_count < MAG_CAL_MIN_VALIDATE_SAMPLES)
            {
                mag_cal.state = MAG_CAL_STATE_FAILED;
                Buzzer_Post(BUZZER_EVENT_MAG_CAL_FAILED);
                return;
            }
            float count = (float)mag_cal.validation_count;
            float mean = mag_cal.norm_sum / count;
            float variance = mag_cal.norm_squared_sum / count - mean * mean;
            if (variance < 0.0f)
            {
                variance = 0.0f;
            }
            float standard_deviation = sqrtf(variance);
            float variation_ratio = standard_deviation / mean;
            if (!isfinite(variation_ratio) || variation_ratio > MAG_CAL_MAX_VARIATION_RATIO)
            {
                mag_cal.state = MAG_CAL_STATE_FAILED;
                Buzzer_Post(BUZZER_EVENT_MAG_CAL_FAILED);
                return;
            }
            mag_cal.active_data = mag_cal.candidate_data;
            mag_cal.active_data_valid = true;
            mag_cal.state = MAG_CAL_STATE_READY_DELAY;
            mag_cal.state_start_tick = now_tick;
            Buzzer_Post(BUZZER_EVENT_MAG_CAL_SUCCESS);
        }
    }
}
void MagCalibration_Abort(void)
{
    if (!MagCalibration_IsBusy())
    {
        return;
    }

    mag_cal.state = MAG_CAL_STATE_ABORTED;
    mag_cal.active_data_valid = false;
    Buzzer_Post(BUZZER_EVENT_MAG_CAL_ABORTED);
}

bool MagCalibration_IsBusy(void)
{
    return mag_cal.state == MAG_CAL_STATE_WAIT_START || mag_cal.state == MAG_CAL_STATE_COLLECTING ||
           mag_cal.state == MAG_CAL_STATE_VALIDATING;
}
bool MagCalibration_HasValidData(void) { return mag_cal.active_data_valid; }
bool MagCalibration_IsFlightInhibited(void) { return mag_cal.state != MAG_CAL_STATE_READY; }
const MagCalibrationData *MagCalibration_GetActiveData(void) { return &mag_cal.active_data; }
MagCalibrationState MagCalibration_GetState(void) { return mag_cal.state; }
MagCalibrationEntry MagCalibration_GetEntry(void) { return mag_cal.entry; }
void MagCalibration_Service(TickType_t now_tick, bool remote_online, bool remote_locked, bool throttle_low)
{
    if (mag_cal.state == MAG_CAL_STATE_READY_DELAY &&
        (now_tick - mag_cal.state_start_tick) >= MAG_CAL_READY_DELAY_TICKS)
    {
        mag_cal.state = MAG_CAL_STATE_WAIT_LOCK;
    }

    if (mag_cal.state == MAG_CAL_STATE_WAIT_LOCK && remote_online && remote_locked && throttle_low)
    {
        mag_cal.state = MAG_CAL_STATE_READY;
        Buzzer_Post(BUZZER_EVENT_BOOT_OK);
    }
}

void MagCalibration_PrintInfo(void)
{
    debug_printf("MAG state:%d entry:%d "
                 "samples:%lu reject:%lu "
                 "mask:0x%02X\r\n",
                 mag_cal.state, mag_cal.entry, mag_cal.sample_count, mag_cal.rejected_count, mag_cal.direction_mask);
    debug_printf("MAG min:%.1f,%.1f,%.1f "
                 "max:%.1f,%.1f,%.1f\r\n",
                 mag_cal.min_x, mag_cal.min_y, mag_cal.min_z, mag_cal.max_x, mag_cal.max_y, mag_cal.max_z);
    debug_printf("MAG active offset:"
                 "%.2f,%.2f,%.2f "
                 "scale:%.4f,%.4f,%.4f "
                 "valid:%d\r\n",
                 mag_cal.active_data.offset_x, mag_cal.active_data.offset_y, mag_cal.active_data.offset_z,
                 mag_cal.active_data.scale_x, mag_cal.active_data.scale_y, mag_cal.active_data.scale_z,
                 mag_cal.active_data_valid);
}
