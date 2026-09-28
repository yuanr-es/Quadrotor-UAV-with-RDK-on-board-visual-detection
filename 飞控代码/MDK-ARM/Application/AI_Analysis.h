#ifndef __AI_ANALYSIS_H
#define __AI_ANALYSIS_H

#include <stdbool.h>
#include <stdint.h>
#include "App_freeRTOS.h"

#define AI_PACKET_PAYLOAD_SIZE 16U
#define AI_COMMAND_TIMEOUT_MS 200U
#define AI_ROLL_LIMIT_DEG 10.0f
#define AI_PITCH_LIMIT_DEG 10.0f
#define AI_YAW_RATE_LIMIT_DPS 120.0f
#define AI_THROTTLE_LIMIT 700.0f

typedef enum
{
    STATE_WAIT_HEADER_1,
    STATE_WAIT_HEADER_2,
    STATE_READ_PAYLOAD,
    STATE_READ_CRC,
    STATE_WAIT_TAIL_1,
    STATE_WAIT_TAIL_2
} ParserState_t;

typedef struct
{
    float roll;
    float pitch;
    float yaw;
    float throttle;
} AI_Data_t;

typedef union
{
    AI_Data_t data;
    uint8_t bytes[AI_PACKET_PAYLOAD_SIZE];
} ControlPacket_u;

extern ControlPacket_u AI_Received;
extern volatile uint8_t ai_valid;

uint8_t AI_CalculateCRC8(const uint8_t *data, uint16_t length);
bool Analyse_data_from_ai(uint8_t byte);
bool AI_GetLatestCommand(AiControlCommand *command, TickType_t max_age_ticks);
void AI_InvalidateCommand(void);
void AI_ResetParser(void);

#endif
