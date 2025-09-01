
#ifndef VL53L0X_H
#define VL53L0X_H

// Inclusão de bibliotecas padrão para tipos booleanos e inteiros com tamanho fixo
#include <stdbool.h>     // Permite o uso do tipo bool (true/false)
#include <stdint.h>      // Permite o uso de tipos como uint8_t, uint16_t, etc.

// Inclusão da biblioteca de comunicação I2C específica do Raspberry Pi Pico
#include "hardware/i2c.h"

// Define o endereço I2C padrão do sensor VL53L0X
#define ENDERECO_VL53L0X 0x29 // Endereço hexadecimal padrão do VL53L0X

// Estrutura que representa um dispositivo VL53L0X
typedef struct {
    i2c_inst_t* i2c;             // Ponteiro para a instância da interface I2C utilizada
    uint8_t endereco;            // Endereço I2C do sensor
    uint16_t tempo_timeout;      // Tempo limite para operações (em milissegundos)
    uint8_t variavel_parada;     // Flag usada para controle de parada de medições contínuas
    uint32_t tempo_medicao_us;   // Tempo de medição em microssegundos
} vl53l0x_dispositivo;

// Função para inicializar o sensor VL53L0X com a interface I2C especificada
bool vl53l0x_inicializar(vl53l0x_dispositivo* dispositivo, i2c_inst_t* porta_i2c);

// Função para iniciar medições contínuas com intervalo definido em milissegundos
void vl53l0x_iniciar_continuo(vl53l0x_dispositivo* dispositivo, uint32_t periodo_ms);

// Função para ler a distância medida em modo contínuo, retornando o valor em centímetros
uint16_t vl53l0x_ler_distancia_continua_cm(vl53l0x_dispositivo* dispositivo);

#endif // VL53L0X_H

