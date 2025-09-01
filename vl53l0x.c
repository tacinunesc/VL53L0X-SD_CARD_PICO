
#include "vl53l0x.h"
#include "pico/stdlib.h"
#include <string.h>

// Tempo padrão de medição em microssegundos
#define TEMPO_PADRAO_MEDICAO_US 33000
// Valor que representa uma distância inválida (pois o sensor pode medir até 2 metros)
#define DISTANCIA_INVALIDA 2001

// ========================== Funções auxiliares ==========================

// Escreve um valor de 8 bits em um registrador do sensor
static void write_reg(vl53l0x_dispositivo* dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(dev->i2c, dev->endereco, buf, 2, false);
}

// Escreve um valor de 16 bits em um registrador do sensor
static void write_reg16(vl53l0x_dispositivo* dev, uint8_t reg, uint16_t val) {
    uint8_t buf[3] = {reg, (val >> 8), (val & 0xFF)};
    i2c_write_blocking(dev->i2c, dev->endereco, buf, 3, false);
}

// Lê um valor de 8 bits de um registrador do sensor
static uint8_t read_reg(vl53l0x_dispositivo* dev, uint8_t reg) {
    uint8_t val;
    i2c_write_blocking(dev->i2c, dev->endereco, &reg, 1, true);
    i2c_read_blocking(dev->i2c, dev->endereco, &val, 1, false);
    return val;
}

// Lê um valor de 16 bits de um registrador do sensor
static uint16_t read_reg16(vl53l0x_dispositivo* dev, uint8_t reg) {
    uint8_t buf[2];
    i2c_write_blocking(dev->i2c, dev->endereco, &reg, 1, true);
    i2c_read_blocking(dev->i2c, dev->endereco, buf, 2, false);
    return ((uint16_t)buf[0] << 8) | buf[1];
}

// Retorna o tempo atual em milissegundos desde o boot
static inline uint32_t tempo_agora_ms() {
    return to_ms_since_boot(get_absolute_time());
}

// ========================== Inicialização do sensor ==========================

bool vl53l0x_inicializar(vl53l0x_dispositivo* dev, i2c_inst_t* porta_i2c) {
    // Configura a instância I2C e o endereço do sensor
    dev->i2c = porta_i2c;
    dev->endereco = ENDERECO_VL53L0X;
    dev->tempo_timeout = 1000; // Timeout de 1 segundo

    // Sequência de inicialização do VL53L0X (configuração interna)
    write_reg(dev, 0x80, 0x01);
    write_reg(dev, 0xFF, 0x01);
    write_reg(dev, 0x00, 0x00);
    dev->variavel_parada = read_reg(dev, 0x91); // Armazena valor de parada
    write_reg(dev, 0x00, 0x01);
    write_reg(dev, 0xFF, 0x00);
    write_reg(dev, 0x80, 0x00);

    // Configura parâmetros de medição
    write_reg(dev, 0x60, read_reg(dev, 0x60) | 0x12);
    write_reg16(dev, 0x44, (uint16_t)(0.25 * (1 << 7))); // Ajuste de ganho
    write_reg(dev, 0x01, 0xFF);

    // Configuração adicional para inicialização
    write_reg(dev, 0x80, 0x01);
    write_reg(dev, 0xFF, 0x01);
    write_reg(dev, 0x00, 0x00);
    write_reg(dev, 0xFF, 0x06);
    write_reg(dev, 0x83, read_reg(dev, 0x83) | 0x04);
    write_reg(dev, 0xFF, 0x07);
    write_reg(dev, 0x81, 0x01);
    write_reg(dev, 0x80, 0x01);
    write_reg(dev, 0x94, 0x6B);
    write_reg(dev, 0x83, 0x00);

    // Aguarda resposta do sensor com timeout
    uint32_t inicio = tempo_agora_ms();
    while (read_reg(dev, 0x83) == 0x00) {
        if (tempo_agora_ms() - inicio > dev->tempo_timeout) return false;
    }

    // Finaliza sequência de inicialização
    write_reg(dev, 0x83, 0x01);
    read_reg(dev, 0x92);
    write_reg(dev, 0x81, 0x00);
    write_reg(dev, 0xFF, 0x06);
    write_reg(dev, 0x83, read_reg(dev, 0x83) & ~0x04);
    write_reg(dev, 0xFF, 0x01);
    write_reg(dev, 0x00, 0x01);
    write_reg(dev, 0xFF, 0x00);
    write_reg(dev, 0x80, 0x00);

    // Configura modo de medição padrão
    write_reg(dev, 0x0A, 0x04);
    write_reg(dev, 0x84, read_reg(dev, 0x84) & ~0x10);
    write_reg(dev, 0x0B, 0x01);

    // Define tempo de medição e inicia o sensor
    dev->tempo_medicao_us = TEMPO_PADRAO_MEDICAO_US;
    write_reg(dev, 0x01, 0xE8);
    write_reg16(dev, 0x04, TEMPO_PADRAO_MEDICAO_US / 1085);

    write_reg(dev, 0x0B, 0x01);
    return true;
}

// ========================== Modo contínuo ==========================

void vl53l0x_iniciar_continuo(vl53l0x_dispositivo* dev, uint32_t periodo_ms) {
    // Reconfigura registradores para modo contínuo
    write_reg(dev, 0x80, 0x01);
    write_reg(dev, 0xFF, 0x01);
    write_reg(dev, 0x00, 0x00);
    write_reg(dev, 0x91, dev->variavel_parada);
    write_reg(dev, 0x00, 0x01);
    write_reg(dev, 0xFF, 0x00);
    write_reg(dev, 0x80, 0x00);

    // Define o período de medição contínua
    if (periodo_ms != 0) {
        write_reg16(dev, 0x04, periodo_ms * 12 / 13);
        write_reg(dev, 0x00, 0x04); // Modo contínuo com intervalo
    } else {
        write_reg(dev, 0x00, 0x02); // Modo contínuo sem intervalo
    }
}

// ========================== Leitura contínua ==========================

uint16_t vl53l0x_ler_distancia_continua_cm(vl53l0x_dispositivo* dev) {
    // Aguarda nova medição com timeout
    uint32_t inicio = tempo_agora_ms();
    while ((read_reg(dev, 0x13) & 0x07) == 0) {
        if (tempo_agora_ms() - inicio > dev->tempo_timeout) return DISTANCIA_INVALIDA;
    }

    // Lê distância em milímetros
    uint16_t distancia_mm = read_reg16(dev, 0x1E);
    write_reg(dev, 0x0B, 0x01); // Limpa flag de interrupção

    // Verifica se a distância é válida
    if (distancia_mm >= 2001 || distancia_mm == DISTANCIA_INVALIDA) return DISTANCIA_INVALIDA;

    // Converte para centímetros
    return distancia_mm / 10;
}
