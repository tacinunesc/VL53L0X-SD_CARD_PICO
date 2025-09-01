
// Bibliotecas padrão
#include <assert.h>     // Para verificações de assertividade em tempo de execução
#include <string.h>     // Para manipulação de strings

// Bibliotecas do projeto
#include "my_debug.h"   // Biblioteca personalizada para depuração
#include "hw_config.h"  // Configuração de hardware específica do projeto

// Bibliotecas do sistema de arquivos FAT
#include "ff.h"         // Tipos inteiros e funções do sistema de arquivos FAT
#include "diskio.h"     // Declarações de funções de acesso ao disco

/* 
Configuração de hardware assumida para comunicação SPI com cartão MicroSD:

|       | SPI0  | GPIO  | Pin   | SPI       | MicroSD   | Descrição               | 
| ----- | ----  | ----- | ---   | --------  | --------- | ----------------------- |
| MISO  | RX    | 16    | 21    | DO        | DO        | Dados do cartão para MCU|
| MOSI  | TX    | 19    | 25    | DI        | DI        | Dados da MCU para cartão|
| SCK   | SCK   | 18    | 24    | SCLK      | CLK       | Clock do barramento SPI |
| CS0   | CSn   | 17    | 22    | SS ou CS  | CS        | Seleção do cartão SD    |
| DET   |       | 22    | 29    |           | CD        | Detecção de cartão      |
| GND   |       |       | 18,23 |           | GND       | Terra                   |
| 3v3   |       |       | 36    |           | 3v3       | Alimentação 3.3V        |
*/

// ========================== Configuração de SPI ==========================

// Array de configurações para interfaces SPI
static spi_t spis[] = {
    {
        .hw_inst = spi0,      // Instância de hardware SPI utilizada
        .miso_gpio = 16,      // GPIO para MISO (entrada de dados)
        .mosi_gpio = 19,      // GPIO para MOSI (saída de dados)
        .sck_gpio = 18,       // GPIO para clock SPI
        .baud_rate = 1000000  // Taxa de transmissão: 1 Mbps
        // Alternativa comentada: 25 Mbps (frequência real: ~20.8 MHz)
    }
};

// ========================== Configuração do cartão SD ==========================

// Array de configurações para cartões SD
static sd_card_t sd_cards[] = {
    {
        .pcName = "0:",             // Nome lógico do dispositivo (usado para montagem)
        .spi = &spis[0],            // Ponteiro para a interface SPI correspondente
        .ss_gpio = 17,              // GPIO para seleção do cartão (CS)
        .use_card_detect = false,   // Desativa verificação de presença do cartão
        .card_detect_gpio = 22,     // GPIO que poderia ser usado para detectar o cartão
        .card_detected_true = -1    // Valor esperado para indicar presença do cartão
    }
};

// ========================== Funções de acesso ==========================

// Retorna o número de cartões SD configurados
size_t sd_get_num() {
    return count_of(sd_cards);
}

// Retorna o ponteiro para o cartão SD de índice 'num'
sd_card_t *sd_get_by_num(size_t num) {
    assert(num <= sd_get_num()); // Garante que o índice é válido
    if (num <= sd_get_num()) {
        return &sd_cards[num];   // Retorna ponteiro para o cartão
    } else {
        return NULL;             // Retorna NULL se índice inválido
    }
}

// Retorna o número de interfaces SPI configuradas
size_t spi_get_num() {
    return count_of(spis);
}

// Retorna o ponteiro para a interface SPI de índice 'num'
spi_t *spi_get_by_num(size_t num) {
    assert(num <= spi_get_num()); // Garante que o índice é válido
    if (num <= spi_get_num()) {
        return &spis[num];        // Retorna ponteiro para a interface SPI
    } else {
        return NULL;              // Retorna NULL se índice inválido
    }
}


