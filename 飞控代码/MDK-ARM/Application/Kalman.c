#include "stm32f4xx_hal.h"
#include "Kalman.h"
#include "BMP388.h"
#include <math.h>

void Kalman_Init(kalman_t *Kalman)
{
    Kalman->altitude = 0.0f;
    Kalman->speed = 0.0f;
    Kalman->p[0][0] = 1.0f;
    Kalman->p[0][1] = 0.0f;
    Kalman->p[1][0] = 0.0f;
    Kalman->p[1][1] = 1.0f;
    Kalman->Q_acc = 0.005f;
    Kalman->R_baro = 1.0f;
}
void Kalman_GetAltitude(kalman_t *Kalman, float altitude, float acc_z, float dt)
{
    Kalman->altitude += Kalman->speed * dt + 0.5f * acc_z * dt * dt;

    Kalman->p[0][0] +=
        (Kalman->p[1][1] * dt + Kalman->p[1][0] + Kalman->p[0][1] + Kalman->Q_acc * 0.25f * dt * dt) * dt;
    Kalman->p[0][1] += (Kalman->p[1][1] + 0.5f * Kalman->Q_acc * dt) * dt;
    Kalman->p[1][0] += (Kalman->p[1][1] + 0.5f * Kalman->Q_acc * dt) * dt;
    Kalman->p[1][1] += Kalman->Q_acc * dt;

    float S = Kalman->p[0][0] + Kalman->R_baro;
    float k0 = Kalman->p[0][0] / S;
    float k1 = Kalman->p[1][0] / S;

    float delta = altitude - Kalman->altitude;
    Kalman->altitude += k0 * delta;
    Kalman->speed += k1 * delta;

    float temp_p00 = Kalman->p[0][0];
    float temp_p01 = Kalman->p[0][1];
    Kalman->p[0][0] -= temp_p00 * k0;
    Kalman->p[0][1] -= temp_p01 * k0;
    Kalman->p[1][0] -= temp_p00 * k1;
    Kalman->p[1][1] -= temp_p01 * k1;
}
