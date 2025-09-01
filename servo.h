
#ifndef SERVO_H
#define SERVO_H

#include <stdint.h> // Permite o uso de uint8_t, uint16_t, etc.

// Define o número do pino GPIO conectado ao servo motor
#define PINO_SERVO 2 // Pino GPIO 2 será usado para sinal PWM do servo

// Inicializa o PWM no pino definido para controlar o servo motor
void inicializar_pwm_servo(void);

// Define a posição do servo motor com base em um valor de 0 a 180 graus
void servo_posicao(uint8_t posicao); 

#endif // SERVO_H
