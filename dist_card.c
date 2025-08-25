#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "vl53l0x.h"
#include "servo.h"
#include "inc/ssd1306.h"
#include "inc/ssd1306_fonts.h"
#include "ff.h"  // FatFs para SD

#define PORTA_I2C i2c0
const uint PINO_SDA_I2C = 0;
const uint PINO_SCL_I2C = 1;

#define LED_VERDE 11
#define LED_VERMELHO 13

#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

FATFS fs;

// Função para registrar distância no SD com tempo desde boot e unidade correta
void registrar_distancia(uint16_t distancia_cm, const char* estado, uint64_t tempo_ms) {
    FIL arquivo;
    char linha[80];
    char valor_str[16];
    char unidade[4];
    if (distancia_cm >= 100 && distancia_cm < 65535) {
        float distancia_m = distancia_cm / 100.0f;
        snprintf(valor_str, sizeof(valor_str), "%.2f", distancia_m);
        strcpy(unidade, "m");
    } else if (distancia_cm == 65535) {
        strcpy(valor_str, "ERRO");
        strcpy(unidade, "");
    } else {
        snprintf(valor_str, sizeof(valor_str), "%d", distancia_cm);
        strcpy(unidade, "cm");
    }
    unsigned long minutos = tempo_ms / 60000;
    unsigned long segundos = (tempo_ms / 1000) % 60;

    FRESULT fr = f_open(&arquivo, "registro.txt", FA_OPEN_APPEND | FA_WRITE);
    if (fr == FR_OK) {
        snprintf(linha, sizeof(linha), "[%02lu:%02lu] Distancia: %s %s - Estado: %s\n",
                 minutos, segundos, valor_str, unidade, estado);
        UINT bytes_escritos;
        f_write(&arquivo, linha, strlen(linha), &bytes_escritos);
        f_close(&arquivo);
    } else {
        printf("Erro ao abrir arquivo: %d\n", fr);
    }
}

void inicializar_sd() {
    spi_init(SPI_PORT, 1000 * 1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        printf("Erro ao montar SD: %d\n", fr);
    } else {
        printf("Cartão SD montado com sucesso.\n");
    }
}

void exibir_oled(uint16_t distancia_cm, const char* estado_porta) {
    char buffer[32];
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("MONITOR DISTANCIA", Font_6x8, White);

    if (distancia_cm >= 100 && distancia_cm < 65535) {
        float distancia_m = distancia_cm / 100.0f;
        snprintf(buffer, sizeof(buffer), "DISTANCIA: %.2f m", distancia_m);
    } else if (distancia_cm == 65535) {
        snprintf(buffer, sizeof(buffer), "ERRO SENSOR");
    } else {
        snprintf(buffer, sizeof(buffer), "DISTANCIA: %d cm", distancia_cm);
    }
    ssd1306_SetCursor(0, 16);
    ssd1306_WriteString(buffer, Font_6x8, White);

    snprintf(buffer, sizeof(buffer), "ACESSO-AUT: %s", estado_porta);
    ssd1306_SetCursor(0, 32);
    ssd1306_WriteString(buffer, Font_6x8, White);

    ssd1306_UpdateScreen();
}

int main() {
    stdio_init_all();
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    // Inicialize I2C ANTES do display!
    i2c_init(PORTA_I2C, 100 * 1000);
    gpio_set_function(PINO_SDA_I2C, GPIO_FUNC_I2C);
    gpio_set_function(PINO_SCL_I2C, GPIO_FUNC_I2C);
    gpio_pull_up(PINO_SDA_I2C);
    gpio_pull_up(PINO_SCL_I2C);

    // Inicialize o display OLED
    printf("Iniciando SSD1306...\n");
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();
    printf("Display SSD1306 OK\n");

    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);

    inicializar_pwm_servo();
    inicializar_sd();

    vl53l0x_dispositivo sensor;
    printf("Iniciando VL53L0X...\n");
    if (!vl53l0x_inicializar(&sensor, PORTA_I2C)) {
        printf("ERRO: Falha ao inicializar o sensor VL53L0X.\n");
        while (1);
    }
    printf("Sensor VL53L0X inicializado com sucesso.\n");

    vl53l0x_iniciar_continuo(&sensor, 0);
    printf("Sensor em modo contínuo. Coletando dados...\n");

    uint8_t ultima_posicao = 255;

    while (1) {
        uint16_t distancia_cm = vl53l0x_ler_distancia_continua_cm(&sensor);

        // Data/hora desde boot
        uint64_t tempo_ms = to_ms_since_boot(get_absolute_time());
        unsigned long minutos = tempo_ms / 60000;
        unsigned long segundos = (tempo_ms / 1000) % 60;

        // Decide unidade e valor para terminal
        char valor_str[16];
        char unidade[4];
        if (distancia_cm >= 100 && distancia_cm < 65535) {
            float distancia_m = distancia_cm / 100.0f;
            snprintf(valor_str, sizeof(valor_str), "%.2f", distancia_m);
            strcpy(unidade, "m");
        } else if (distancia_cm == 65535) {
            strcpy(valor_str, "ERRO");
            strcpy(unidade, "");
        } else {
            snprintf(valor_str, sizeof(valor_str), "%d", distancia_cm);
            strcpy(unidade, "cm");
        }

        printf("[%02lu:%02lu] Valor lido: %s %s | ", minutos, segundos, valor_str, unidade);

        const char* estado_porta = "FECHADO";
        uint8_t nova_posicao;
        if (distancia_cm < 10) {
            nova_posicao = 1;
            estado_porta = "ABERTO";
        } else {
            nova_posicao = 2;
            estado_porta = "FECHADO";
        }

        exibir_oled(distancia_cm, estado_porta);

        if (distancia_cm == 65535) {
            printf("Timeout ou erro de leitura.\n");
        } else if (distancia_cm > 800) {
            printf("Fora de alcance.\n");
        } else {
            printf("Distância: %s %s\n", valor_str, unidade);

            registrar_distancia(distancia_cm, estado_porta, tempo_ms);

            if (nova_posicao != ultima_posicao) {
                servo_posicao(nova_posicao);
                sleep_ms(500);
                servo_posicao(0);
                ultima_posicao = nova_posicao;
            }

            gpio_put(LED_VERDE, nova_posicao == 1);
            gpio_put(LED_VERMELHO, nova_posicao != 1);
        }
        sleep_ms(200);
    }
    return 0;
}
   