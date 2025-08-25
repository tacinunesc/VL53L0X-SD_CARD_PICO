#ifndef VL53L0X_H
#define VL53L0X_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/i2c.h"

#define ENDERECO_VL53L0X 0x29 // Endereço I2C padrão do VL53L9X

typedef struct {
i2c_inst_t* i2c;
    uint8_t endereco;
    uint16_t tempo_timeout;
    uint8_t variavel_parada;
    uint32_t tempo_medicao_us;
} vl53l0x_dispositivo;

bool vl53l0x_inicializar(vl53l0x_dispositivo* dispositivo, i2c_inst_t* porta_i2c);
void vl53l0x_iniciar_continuo(vl53l0x_dispositivo* dispositivo, uint32_t periodo_ms);
uint16_t vl53l0x_ler_distancia_continua_cm(vl53l0x_dispositivo* dispositivo);

#endif // VL53L0X_H 
