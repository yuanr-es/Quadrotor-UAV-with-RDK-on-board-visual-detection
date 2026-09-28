#ifndef __PID_H
#define __PID_H
#include "stm32f4xx_hal.h"
#include "main.h"
typedef struct
{
    float kp;
    float ki;
    float kd;
    float err;
    float last_err;
    float derivative_lpf;
    uint8_t derivative_initialized;
    float integral_err;
    float desire;
    float measure;
    float output;
} PID_parameterStruct;
void PID_Reset(PID_parameterStruct *PID);
void PID_ResetIntegral(PID_parameterStruct *PID);
void PID_mono(PID_parameterStruct *PID, float dt);
void PID_mono_anti_windup(PID_parameterStruct *PID, float dt, uint8_t allow_integral, float output_limit,
                          float integral_output_limit);
void PID_chain(PID_parameterStruct *OUT_PID, PID_parameterStruct *IN_PID, float dt);

#endif
