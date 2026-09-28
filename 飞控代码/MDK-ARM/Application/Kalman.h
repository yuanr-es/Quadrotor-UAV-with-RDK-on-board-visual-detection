#ifndef __KALMAN_H
#define __KALMAN_H

typedef struct
{
    float altitude;
    float speed;
    float p[2][2];

    float Q_acc;
    float R_baro;
} kalman_t;
void Kalman_Init(kalman_t *Kalman);
void Kalman_GetAltitude(kalman_t *Kalman, float altitude, float acc_z, float dt);
#endif
