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

// === DEFINIÇÕES DE PINOS E HARDWARE ===
#define PORTA_I2C i2c0 // VL53L0X no barramento I2C0
#define PINO_SDA_I2C 0
#define PINO_SCL_I2C 1

#define LED_VERDE 11
#define LED_VERMELHO 13

#define SPI_PORT spi0 // SD CARD no barramento SPI0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

#define DISTANCIA_INVALIDA 2001 // Valor para indicar leitura inválida (>2m)
#define DISTANCIA_MAXIMA_CM 999 // Limite para exibir em cm, acima disso exibe em metros

FATFS fs;

// === Função para registrar distância no cartão SD ===
void registrar_distancia(uint16_t distancia_cm, const char* estado, uint64_t tempo_ms) {
    FIL arquivo;
    char linha[80], valor_str[16], unidade[4];

    // Decide unidade e valor a registrar
    if (distancia_cm >= 100 && distancia_cm < DISTANCIA_INVALIDA) {
        snprintf(valor_str, sizeof(valor_str), "%.2f", distancia_cm / 100.0f);
        strcpy(unidade, "m");
    } else if (distancia_cm == DISTANCIA_INVALIDA) {
        strcpy(valor_str, "ERRO");
        strcpy(unidade, "");
    } else {
        snprintf(valor_str, sizeof(valor_str), "%d", distancia_cm);
        strcpy(unidade, "cm");
    }

    // Calcula tempo em minutos e segundos desde o boot
    unsigned long minutos = tempo_ms / 60000;
    unsigned long segundos = (tempo_ms / 1000) % 60;

    // Abre arquivo e registra linha
    FRESULT fr = f_open(&arquivo, "distancia.txt", FA_OPEN_APPEND | FA_WRITE);
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

// === Inicialização do cartão SD ===
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

// === Exibe informações na tela OLED ===
void exibir_oled(uint16_t distancia_cm, const char* estado_porta) {
    char buffer[32];
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("MONITOR DISTANCIA", Font_6x8, White);

    // Mostra distância em metros, cm ou erro
    if (distancia_cm >= 100 && distancia_cm < DISTANCIA_INVALIDA) {
        snprintf(buffer, sizeof(buffer), "DISTANCIA: %.2f m", distancia_cm / 100.0f);
    } else if (distancia_cm == DISTANCIA_INVALIDA) {
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

// === Função principal ===
int main() {
    stdio_init_all();
    while (!stdio_usb_connected()) sleep_ms(100);

    // Inicializa I2C antes do display e sensor
    i2c_init(PORTA_I2C, 100 * 1000);
    gpio_set_function(PINO_SDA_I2C, GPIO_FUNC_I2C);
    gpio_set_function(PINO_SCL_I2C, GPIO_FUNC_I2C);
    gpio_pull_up(PINO_SDA_I2C);
    gpio_pull_up(PINO_SCL_I2C);

    // Inicializa display OLED
    printf("Iniciando SSD1306...\n");
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();
    printf("Display SSD1306 OK\n");

    // Inicializa LEDs
    gpio_init(LED_VERDE); gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_init(LED_VERMELHO); gpio_set_dir(LED_VERMELHO, GPIO_OUT);

    // Inicializa servo e SD
    inicializar_pwm_servo();
    inicializar_sd();

    // Inicializa sensor VL53L0X
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

    // === Loop principal ===
    while (1) {
        // Lê distância do sensor
        uint16_t distancia_cm = vl53l0x_ler_distancia_continua_cm(&sensor);
        uint64_t tempo_ms = to_ms_since_boot(get_absolute_time());

        char valor_str[16], unidade[4];
        const char* estado_porta = "FECHADO";
        uint8_t nova_posicao = 2;

        // Decide unidade para terminal e lógica
        if (distancia_cm >= 100 && distancia_cm < DISTANCIA_INVALIDA) {
            snprintf(valor_str, sizeof(valor_str), "%.2f", distancia_cm / 100.0f);
            strcpy(unidade, "m");
        } else if (distancia_cm == DISTANCIA_INVALIDA) {
            strcpy(valor_str, "ERRO");
            strcpy(unidade, "");
        } else {
            snprintf(valor_str, sizeof(valor_str), "%d", distancia_cm);
            strcpy(unidade, "cm");
        }

        // Define estado da porta e posição do servo
        if (distancia_cm < 10) {
            nova_posicao = 1;
            estado_porta = "ABERTO";
        }

        // Mostra no terminal o estado e a distância
        printf("Estado: %s | Distancia: %s %s\n", estado_porta, valor_str, unidade);

        // Atualiza display OLED
        exibir_oled(distancia_cm, estado_porta);

        // Lógica de tratamento de erro e fora de alcance
        if (distancia_cm == DISTANCIA_INVALIDA) {
            printf("Erro de leitura.\n");
        } else if (distancia_cm > DISTANCIA_MAXIMA_CM) {
            printf("Fora de alcance.\n");
        } else {
            // Registra no SD e aciona servo se necessário
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