#include "PID.h"
#include <math.h>

#define PID_D_LPF_CUTOFF_HZ 25.0f
#define PID_TWO_PI 6.283185307f

static float PID_UpdateFilteredDerivative(PID_parameterStruct *PID, float dt)
{
    if (!PID->derivative_initialized)
    {
        PID->last_err = PID->err;
        PID->derivative_lpf = 0.0f;
        PID->derivative_initialized = 1U;
        return 0.0f;
    }

    const float raw_derivative = (PID->err - PID->last_err) / dt;
    const float rc = 1.0f / (PID_TWO_PI * PID_D_LPF_CUTOFF_HZ);
    const float alpha = dt / (rc + dt);

    PID->derivative_lpf += alpha * (raw_derivative - PID->derivative_lpf);
    return PID->derivative_lpf;
}

void PID_Reset(PID_parameterStruct *PID)
{
    PID->err = 0.0f;
    PID->last_err = 0.0f;
    PID->derivative_lpf = 0.0f;
    PID->derivative_initialized = 0U;
    PID->integral_err = 0.0f;
    PID->output = 0.0f;
}
void PID_ResetIntegral(PID_parameterStruct *PID) { PID->integral_err = 0.0f; }
void PID_mono(PID_parameterStruct *PID, float dt)
{
    if (dt < 0.0005f)
    {
        dt = 0.0005f;
    }
    if (dt > 0.03f)
    {
        dt = 0.03f;
    }
    PID->err = PID->desire - PID->measure;
    PID->integral_err += PID->err * dt;
    if (PID->integral_err > 500.0f) PID->integral_err = 500.0f;
    if (PID->integral_err < -500.0f) PID->integral_err = -500.0f;
    const float derivative = PID->kd * PID_UpdateFilteredDerivative(PID, dt);
    PID->output = PID->kp * PID->err + PID->ki * PID->integral_err + derivative;
    if (PID->output > 500.0f) PID->output = 500.0f;
    if (PID->output < -500.0f) PID->output = -500.0f;
    PID->last_err = PID->err;
}
void PID_mono_anti_windup(PID_parameterStruct *PID, float dt, uint8_t allow_integral, float output_limit,
                          float integral_output_limit)
{
    if (dt < 0.0005f) dt = 0.0005f;
    if (dt > 0.03f) dt = 0.03f;

    PID->err = PID->desire - PID->measure;
    float proportional = PID->kp * PID->err;
    float derivative = PID->kd * PID_UpdateFilteredDerivative(PID, dt);
    float integral_candidate = PID->integral_err;

    if (allow_integral && fabsf(PID->ki) > 0.000001f)
    {
        integral_candidate += PID->err * dt;
        float integral_err_limit = integral_output_limit / fabsf(PID->ki);
        if (integral_candidate > integral_err_limit) integral_candidate = integral_err_limit;
        if (integral_candidate < -integral_err_limit) integral_candidate = -integral_err_limit;

        float candidate_output = proportional + PID->ki * integral_candidate + derivative;
        uint8_t pushing_further_into_saturation = (candidate_output > output_limit && PID->err > 0.0f) ||
                                                  (candidate_output < -output_limit && PID->err < 0.0f);

        if (!pushing_further_into_saturation)
        {
            PID->integral_err = integral_candidate;
        }
    }

    PID->output = proportional + PID->ki * PID->integral_err + derivative;
    if (PID->output > output_limit) PID->output = output_limit;
    if (PID->output < -output_limit) PID->output = -output_limit;
    PID->last_err = PID->err;
}
void PID_chain(PID_parameterStruct *OUT_PID, PID_parameterStruct *IN_PID, float dt)
{
    PID_mono(OUT_PID, dt);
    IN_PID->desire = OUT_PID->output;
    PID_mono(IN_PID, dt);
}
