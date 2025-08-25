#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

#define PINO_SERVO 2

void inicializar_pwm_servo(void);
void servo_posicao(uint8_t posicao);

#endif // SERVO_H