
#include "vl53l0x.h"
#include "pico/stdlib.h"
#include <string.h>

//Funções auxiliares)
static void write_reg(vl53l0x_dispositivo* dev, uint8_t reg, uint8_t val){
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(dev->i2c, dev->endereco, buf, 2, false);
}

static void write_reg16(vl53l0x_dispositivo* dev, uint8_t reg, uint16_t val){
    uint8_t buf[3] = {reg, (val >> 8), (val & 0xFF)};
    i2c_write_blocking(dev->i2c, dev->endereco, buf, 3, false);
}

static uint8_t read_reg(vl53l0x_dispositivo* dev, uint8_t reg) {
    uint8_t val;
    i2c_write_blocking(dev->i2c, dev->endereco, &reg, 1, true);
    i2c_read_blocking(dev->i2c, dev->endereco, &val, 1, false);
    return val;
}

static uint16_t read_reg16(vl53l0x_dispositivo* dev, uint8_t reg) {
    uint8_t buf[2];
    i2c_write_blocking(dev->i2c, dev->endereco, &reg, 1, true);
    i2c_read_blocking(dev->i2c, dev->endereco, buf, 2, false);
    return ((uint16_t)buf[0] << 8) | buf[1];
}

bool vl53l0x_inicializar(vl53l0x_dispositivo* dev, i2c_inst_t* porta_i2c) {
    dev->i2c = porta_i2c;
    dev->endereco = ENDERECO_VL53L0X;
    dev->tempo_timeout = 1000;

    write_reg(dev, 0x80, 0x01);
    write_reg(dev, 0xFF, 0x01);
    write_reg(dev, 0x00, 0x00);
    dev->variavel_parada = read_reg(dev, 0x91);
    write_reg(dev, 0x00, 0x01);
    write_reg(dev, 0xFF, 0x00);
    write_reg(dev, 0x80, 0x00);

    write_reg(dev, 0x60, read_reg(dev, 0x60) | 0x12);
    write_reg16(dev, 0x44, (uint16_t)(0.25 * (1 << 7)));
    write_reg(dev, 0x01, 0xFF);

    write_reg(dev, 0x80, 0x01); write_reg(dev, 0xFF, 0x01);
    write_reg(dev, 0x00, 0x00); write_reg(dev, 0xFF, 0x06);
    write_reg(dev, 0x83, read_reg(dev, 0x83) | 0x04);
    write_reg(dev, 0xFF, 0x07); write_reg(dev, 0x81, 0x01);
    write_reg(dev, 0x80, 0x01); write_reg(dev, 0x94, 0x6b);
    write_reg(dev, 0x83, 0x00);
    uint32_t inicio = to_ms_since_boot(get_absolute_time());
    while (read_reg(dev, 0x83) == 0x00) {
        if (to_ms_since_boot(get_absolute_time()) - inicio > dev->tempo_timeout) return false;
    }
    write_reg(dev, 0x83, 0x01);
    read_reg(dev, 0x92);
    write_reg(dev, 0x81, 0x00); write_reg(dev, 0xFF, 0x06);
    write_reg(dev, 0x83, read_reg(dev, 0x83) & ~0x04);
    write_reg(dev, 0xFF, 0x01); write_reg(dev, 0x00, 0x01);
    write_reg(dev, 0xFF, 0x00); write_reg(dev, 0x80, 0x00);

    write_reg(dev, 0x0A, 0x04);
    write_reg(dev, 0x84, read_reg(dev, 0x84) & ~0x10);
    write_reg(dev, 0x0B, 0x01);

     dev->tempo_medicao_us = 33000;
    write_reg(dev, 0x01, 0xE8);
    write_reg16(dev, 0x04, 33000 / 1085);

    write_reg(dev, 0x0B, 0x01);
    return true;
}

void vl53l0x_iniciar_continuo(vl53l0x_dispositivo* dev, uint32_t periodo_ms) {
    write_reg(dev, 0x80, 0x01);
    write_reg(dev, 0xFF, 0x01); write_reg(dev, 0x00, 0x00);
    write_reg(dev, 0x91, dev->variavel_parada); write_reg(dev, 0x00, 0x01);
    write_reg(dev, 0xFF, 0x00); write_reg(dev, 0x80, 0x00);

    if (periodo_ms != 0) {
        write_reg16(dev, 0x04, periodo_ms * 12 / 13);
        write_reg(dev, 0x00, 0x04);
    } else {
        write_reg(dev, 0x00, 0x02);
    }
}

uint16_t vl53l0x_ler_distancia_continua_cm(vl53l0x_dispositivo* dev) {
    uint32_t inicio = to_ms_since_boot(get_absolute_time());
    while ((read_reg(dev, 0x13) & 0x07) == 0) {
        if ((to_ms_since_boot(get_absolute_time()) - inicio) > dev->tempo_timeout) return 65535;
    }
    uint16_t distancia_mm = read_reg16(dev, 0x1E);
    write_reg(dev, 0x0B, 0x01);
    if (distancia_mm == 65535) return 65535;
    return distancia_mm / 10; // retorna em centímetros
}