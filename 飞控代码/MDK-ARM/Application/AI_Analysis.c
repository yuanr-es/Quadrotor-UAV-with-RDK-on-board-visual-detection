#include "AI_Analysis.h"

#include <math.h>

ControlPacket_u AI_Received;
volatile uint8_t ai_valid = 0U;

static ParserState_t ai_parser_state = STATE_WAIT_HEADER_1;
static ControlPacket_u parser_packet;
static uint8_t ai_parser_index = 0U;
static uint8_t crc_read = 0U;
static TickType_t ai_last_valid_tick = 0U;

uint8_t AI_CalculateCRC8(const uint8_t *data, uint16_t length)
{
    uint8_t crc = 0x00U;

    for (uint16_t i = 0U; i < length; i++)
    {
        crc ^= data[i];
        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1U) ^ 0x07U) : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

static bool AI_ValuesAreValid(const ControlPacket_u *packet)
{
    const AI_Data_t *data = &packet->data;

    return isfinite(data->roll) && isfinite(data->pitch) && isfinite(data->yaw) && isfinite(data->throttle) &&
           fabsf(data->roll) <= AI_ROLL_LIMIT_DEG && fabsf(data->pitch) <= AI_PITCH_LIMIT_DEG &&
           fabsf(data->yaw) <= AI_YAW_RATE_LIMIT_DPS && data->throttle >= 0.0f && data->throttle <= AI_THROTTLE_LIMIT;
}

static void AI_ReturnToHeaderSearch(uint8_t byte)
{
    ai_parser_index = 0U;
    ai_parser_state = (byte == 0xAAU) ? STATE_WAIT_HEADER_2 : STATE_WAIT_HEADER_1;
}

void AI_ResetParser(void)
{
    ai_parser_state = STATE_WAIT_HEADER_1;
    ai_parser_index = 0U;
    crc_read = 0U;
}

void AI_InvalidateCommand(void)
{
    taskENTER_CRITICAL();
    ai_valid = 0U;
    taskEXIT_CRITICAL();
}

bool AI_GetLatestCommand(AiControlCommand *command, TickType_t max_age_ticks)
{
    ControlPacket_u packet;
    TickType_t received_tick;
    uint8_t valid;

    if (command == NULL)
    {
        return false;
    }

    taskENTER_CRITICAL();
    packet = AI_Received;
    received_tick = ai_last_valid_tick;
    valid = ai_valid;
    taskEXIT_CRITICAL();

    if (!valid || (xTaskGetTickCount() - received_tick) > max_age_ticks)
    {
        return false;
    }

    command->roll_deg = packet.data.roll;
    command->pitch_deg = packet.data.pitch;
    command->yaw_rate_dps = packet.data.yaw;
    command->throttle = packet.data.throttle;
    command->received_tick = received_tick;
    command->valid = 1U;
    return true;
}

bool Analyse_data_from_ai(uint8_t byte)
{
    switch (ai_parser_state)
    {
    case STATE_WAIT_HEADER_1:
        if (byte == 0xAAU)
        {
            ai_parser_state = STATE_WAIT_HEADER_2;
        }
        break;

    case STATE_WAIT_HEADER_2:
        if (byte == 0x55U)
        {
            ai_parser_index = 0U;
            ai_parser_state = STATE_READ_PAYLOAD;
        }
        else if (byte != 0xAAU)
        {
            ai_parser_state = STATE_WAIT_HEADER_1;
        }
        break;

    case STATE_READ_PAYLOAD:
        parser_packet.bytes[ai_parser_index++] = byte;
        if (ai_parser_index >= AI_PACKET_PAYLOAD_SIZE)
        {
            ai_parser_index = 0U;
            ai_parser_state = STATE_READ_CRC;
        }
        break;

    case STATE_READ_CRC:
        crc_read = byte;
        ai_parser_state = STATE_WAIT_TAIL_1;
        break;

    case STATE_WAIT_TAIL_1:
        if (byte == 0xFFU)
        {
            ai_parser_state = STATE_WAIT_TAIL_2;
        }
        else
        {
            AI_ReturnToHeaderSearch(byte);
        }
        break;

    case STATE_WAIT_TAIL_2:
        if (byte == 0xFFU)
        {
            const bool crc_ok = AI_CalculateCRC8(parser_packet.bytes, AI_PACKET_PAYLOAD_SIZE) == crc_read;
            const bool values_ok = AI_ValuesAreValid(&parser_packet);

            AI_ResetParser();
            if (crc_ok && values_ok)
            {
                taskENTER_CRITICAL();
                AI_Received = parser_packet;
                ai_last_valid_tick = xTaskGetTickCount();
                ai_valid = 1U;
                taskEXIT_CRITICAL();
                return true;
            }
        }
        else
        {
            AI_ReturnToHeaderSearch(byte);
        }
        break;

    default:
        AI_ResetParser();
        break;
    }

    return false;
}
