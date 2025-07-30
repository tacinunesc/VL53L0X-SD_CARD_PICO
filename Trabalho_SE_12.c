// Declaração de cabeçalhos e bibliotecas
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/bootrom.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "ff.h"
#include "diskio.h"
#include "f_util.h"
#include "hw_config.h"
#include "sd_card.h"

// Definições de pinos
#define BOTAO_A 5
#define BOTAO_B 6
#define LED_PIN 7
#define BUZZER_PIN 21
#define RED_PIN 13
#define BLUE_PIN 12
#define GREEN_PIN 11

// I2C para MPU6050
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1
#define MPU6050_ADDR 0x68

// I2C para Display
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define ENDERECO_DISP 0x3C
#define DISP_W 128
#define DISP_H 64

// Estados do sistema
typedef enum {
    STATE_INIT,
    STATE_READY,
    STATE_RECORDING,
    STATE_ERROR,
    STATE_SD_ACCESS,
    STATE_WAIT_SD_INSERT,
    STATE_WAIT_SD_REMOVE
} system_state_t;

// Variáveis globais
static system_state_t current_state = STATE_INIT;
static ssd1306_t ssd;
static bool recording = false;
static bool sd_mounted = false;
static uint32_t sample_count = 0;
static absolute_time_t recording_start_time;
static FIL data_file;
static char filename[32];

// Flags para os botões
static volatile bool button_a_pressed = false;
static volatile bool button_b_pressed = false;

// Estrutura para armazenar dados do MPU6050
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} mpu6050_data_t;

// Protótipos de funções
static void init_hardware(void);
static void init_display(void);
static void init_mpu6050(void);
static void reset_sd_hardware(void);
static void init_sd_card(void);
static void unmount_sd_card(void);
static void set_led_color(uint8_t r, uint8_t g, uint8_t b);
static void buzzer_beep(uint count, uint duration_ms);
static void update_display(void);
static void read_mpu6050(mpu6050_data_t *data);
static uint32_t find_next_file_number(void);
static bool start_recording(void);
static void stop_recording(void);
static void write_data_to_csv(mpu6050_data_t *data);
static void button_irq_handler(uint gpio, uint32_t events);
static void process_button_a(void);
static void process_button_b(void);

int main() {
    // Inicializar hardware
    init_hardware();
    sleep_ms(2000);
    
    // Teste rápido dos periféricos
    printf("\n=== Teste de Perifericos ===\n");
    
    // LED amarelo durante inicialização
    set_led_color(1, 1, 0);
    printf("LED: Amarelo\n");
    
    // Teste do buzzer
    buzzer_beep(1, 50);
    printf("Buzzer: OK\n");
    
    // Inicializar display
    init_display();
    current_state = STATE_INIT;
    update_display();
    printf("Display: Inicializado\n");
    
    // Inicializar MPU6050
    init_mpu6050();
    printf("MPU6050: Inicializado\n");
    
    // Inicializar SD
    init_sd_card();
    
    // Estado pronto
    if (sd_mounted) {
        current_state = STATE_READY;
        set_led_color(0, 1, 0); // Verde
        printf("Estado: READY (SD montado)\n");
    } else {
        current_state = STATE_ERROR;
        set_led_color(1, 0, 1); // Roxo piscando
        printf("Estado: ERROR (SD nao montado)\n");
    }
    
    update_display();
    
    // Loop principal
    mpu6050_data_t sensor_data;
    absolute_time_t last_display_update = get_absolute_time();
    absolute_time_t last_led_blink = get_absolute_time();
    absolute_time_t last_button_check = get_absolute_time();
    absolute_time_t last_sd_check = get_absolute_time();
    bool led_state = false;
    uint32_t loop_counter = 0;
    
    while (1) {
        // Debug ocasional
        if (loop_counter++ % 1000 == 0) {
            // A cada 1000 loops, mostra que está vivo
            gpio_put(LED_PIN, !gpio_get(LED_PIN)); 
        }
        
        // Verificação adicional dos botões
        static uint32_t button_debug_counter = 0;
        if (button_debug_counter++ % 100 == 0) { // A cada 100 loops
            // Verificar estado direto dos botões
            if (gpio_get(BOTAO_A) == 0) { // Botão pressionado
                printf("DEBUG: Botao A detectado pressionado diretamente!\n");
                button_a_pressed = true;
            }
            if (gpio_get(BOTAO_B) == 0) { // Botão pressionado
                printf("DEBUG: Botao B detectado pressionado diretamente!\n");
                button_b_pressed = true;
            }
        }
        
        // Processar botões com debounce
        if (absolute_time_diff_us(last_button_check, get_absolute_time()) > 200000) { // 200ms debounce
            if (button_a_pressed) {
                printf("\n>>> Botao A pressionado! Estado atual: %d\n", current_state);
                process_button_a();
                button_a_pressed = false;
                last_button_check = get_absolute_time();
            }
            
            if (button_b_pressed) {
                printf("\n>>> Botao B pressionado! Estado atual: %d\n", current_state);
                process_button_b();
                button_b_pressed = false;
                last_button_check = get_absolute_time();
            }
        }
        
        // Verificar periodicamente o estado do SD (a cada 2 segundos)
        if (absolute_time_diff_us(last_sd_check, get_absolute_time()) > 2000000) {
            if (current_state == STATE_WAIT_SD_INSERT || current_state == STATE_ERROR) {
                // Verificar se um cartão foi inserido
                DSTATUS status = disk_status(0);
                if (!(status & STA_NODISK)) {
                    printf("Cartao SD detectado! Pressione B para montar.\n");
                    buzzer_beep(1, 50);
                }
            }
            last_sd_check = get_absolute_time();
        }
        
        // Se estiver gravando, capturar dados
        if (recording) {
            read_mpu6050(&sensor_data);
            write_data_to_csv(&sensor_data);
            
            // Taxa de amostragem: ~75Hz
            sleep_ms(10);
        }
        
        // Atualizar display a cada 250ms
        if (absolute_time_diff_us(last_display_update, get_absolute_time()) > 250000) {
            update_display();
            last_display_update = get_absolute_time();
        }
        
        // Controle de LEDs baseado no estado
        if (absolute_time_diff_us(last_led_blink, get_absolute_time()) > 500000) {
            led_state = !led_state;
            
            switch (current_state) {
                case STATE_ERROR:
                    // Piscar roxo
                    if (led_state) {
                        set_led_color(1, 0, 1);
                    } else {
                        set_led_color(0, 0, 0);
                    }
                    break;
                    
                case STATE_WAIT_SD_INSERT:
                    // Piscar amarelo
                    if (led_state) {
                        set_led_color(1, 1, 0);
                    } else {
                        set_led_color(0, 0, 0);
                    }
                    break;
                    
                case STATE_WAIT_SD_REMOVE:
                    // Vermelho fixo
                    set_led_color(1, 0, 0);
                    break;
                    
                case STATE_READY:
                    // Verde fixo
                    set_led_color(0, 1, 0);
                    break;
                    
                case STATE_RECORDING:
                    // Vermelho fixo (com piscadas azuis durante escrita, indicando funcionamento ininterrupto)
                    set_led_color(1, 0, 0);
                    break;
                    
                default:
                    break;
            }
            
            last_led_blink = get_absolute_time();
        }
        
        // Pequeno delay para não sobrecarregar
        if (!recording) {
            sleep_ms(10);
        }
    }
    
    return 0;
}

// Implementação das funções

static void init_hardware(void) {
    stdio_init_all();
    
    // Inicializar botões com pull-up
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    
    // Configurar interrupções dos botões
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &button_irq_handler);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);
    
    // Inicializar LED RGB
    gpio_init(RED_PIN);
    gpio_set_dir(RED_PIN, GPIO_OUT);
    gpio_init(GREEN_PIN);
    gpio_set_dir(GREEN_PIN, GPIO_OUT);
    gpio_init(BLUE_PIN);
    gpio_set_dir(BLUE_PIN, GPIO_OUT);
    
    // Inicializar buzzer
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    
    // LED onboard
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
}

static void set_led_color(uint8_t r, uint8_t g, uint8_t b) {
    gpio_put(RED_PIN, r);   
    gpio_put(GREEN_PIN, g);
    gpio_put(BLUE_PIN, b);
}

static void buzzer_beep(uint count, uint duration_ms) {
    for (uint i = 0; i < count; i++) {
        gpio_put(BUZZER_PIN, 1);
        sleep_ms(duration_ms);
        gpio_put(BUZZER_PIN, 0);
        if (i < count - 1) {
            sleep_ms(duration_ms);
        }
    }
}

static void init_display(void) {
    // Inicializar I2C para display
    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);
    
    // Inicializar display OLED
    ssd1306_init(&ssd, DISP_W, DISP_H, false, ENDERECO_DISP, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}

static void init_mpu6050(void) {
    // Inicializar I2C para MPU6050
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Reset MPU6050
    uint8_t buf[] = {0x6B, 0x80};
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, buf, 2, false);
    sleep_ms(100); // Estabilizar
    buf[1] = 0x00;
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, buf, 2, false);
    sleep_ms(10); // Estabilizar
}

static void read_mpu6050(mpu6050_data_t *data) {
    uint8_t buffer[6];
    uint8_t reg;
    
    // Ler acelerômetro
    reg = 0x3B;
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, MPU6050_ADDR, buffer, 6, false);
    
    data->accel_x = (buffer[0] << 8) | buffer[1]; // Combinar bytes
    data->accel_y = (buffer[2] << 8) | buffer[3];
    data->accel_z = (buffer[4] << 8) | buffer[5];
    
    // Ler giroscópio
    reg = 0x43;
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, MPU6050_ADDR, buffer, 6, false);
    
    data->gyro_x = (buffer[0] << 8) | buffer[1]; // Combinar bytes
    data->gyro_y = (buffer[2] << 8) | buffer[3];
    data->gyro_z = (buffer[4] << 8) | buffer[5];
}

static void reset_sd_hardware(void) {
    // Reinicializar o hardware do SD completamente
    sd_card_t *pSD = sd_get_by_num(0);
    if (pSD) {
        // Forçar estado não inicializado
        pSD->m_Status = STA_NOINIT;
        pSD->mounted = false;
        
        // Dar tempo para o cartão estabilizar
        sleep_ms(100);
        
        // Reinicializar o driver do SD
        disk_initialize(0);
        sleep_ms(100);
    }
}

static void init_sd_card(void) {
    printf("Inicializando cartao SD...\n");
    
    // Reset do hardware primeiro - necessário para evitar problemas de estado
    // Isso garante que o SD esteja em um estado limpo antes de tentar montar
    // Deu trabalho descobrir que o SD precisa ser resetado antes de montar
    reset_sd_hardware();
    
    sd_card_t *pSD = sd_get_by_num(0);
    if (!pSD) {
        printf("Erro: SD card structure not found\n");
        sd_mounted = false;
        current_state = STATE_ERROR;
        return;
    }
    
    // Verificar se o cartão está presente
    DSTATUS status = disk_status(0);
    if (status & STA_NODISK) {
        printf("Erro: Nenhum cartao SD detectado\n");
        sd_mounted = false;
        current_state = STATE_ERROR;
        return;
    }
    
    if (status & STA_NOINIT) {
        printf("SD nao inicializado, tentando inicializar...\n");
        status = disk_initialize(0);
        if (status) {
            printf("Erro ao inicializar SD: status=%d\n", status);
            sd_mounted = false;
            current_state = STATE_ERROR;
            return;
        }
    }
    
    // Tentar montar o sistema de arquivos
    FATFS *p_fs = &pSD->fatfs;
    const char *drive = pSD->pcName;
    
    FRESULT fr = f_mount(p_fs, drive, 1);
    if (FR_OK != fr) {
        sd_mounted = false;
        current_state = STATE_ERROR;
        printf("Erro ao montar SD: %d\n", fr);
        
        // Se falhou, tentar formatar
        if (fr == FR_NO_FILESYSTEM) {
            printf("Sistema de arquivos nao encontrado. Formate o cartao.\n");
        }
    } else {
        sd_mounted = true;
        pSD->mounted = true;
        printf("SD montado com sucesso!\n");
        
        // Verificar espaço livre
        DWORD fre_clust;
        FATFS *fs;
        fr = f_getfree(drive, &fre_clust, &fs);
        if (fr == FR_OK) {
            DWORD total = (fs->n_fatent - 2) * fs->csize / 2;
            DWORD free = fre_clust * fs->csize / 2;
            printf("SD: %lu KB total, %lu KB livre\n", total, free);
        }
        
        // Listar arquivos existentes
        printf("\nArquivos de dados existentes:\n");
        DIR dir;
        FILINFO fno;
        uint32_t file_count = 0;
        fr = f_opendir(&dir, "/");
        if (fr == FR_OK) {
            while (1) {
                fr = f_readdir(&dir, &fno);
                if (fr != FR_OK || fno.fname[0] == 0) break;
                
                // Mostrar apenas arquivos data_XXXX.csv
                if (strncmp(fno.fname, "data_", 5) == 0 && strstr(fno.fname, ".csv")) {
                    printf("  - %s (%lu bytes)\n", fno.fname, fno.fsize);
                    file_count++;
                }
            }
            f_closedir(&dir);
            printf("Total de arquivos de dados: %lu\n", file_count);
        }
    }
}

static void unmount_sd_card(void) {
    printf("Desmontando cartao SD...\n");
    
    // Fechar qualquer arquivo aberto
    if (recording) {
        stop_recording();
    }
    
    if (sd_mounted) {
        sd_card_t *pSD = sd_get_by_num(0);
        if (pSD) {
            const char *drive = pSD->pcName;
            
            // Desmontar o sistema de arquivos
            FRESULT fr = f_unmount(drive);
            if (FR_OK == fr) {
                sd_mounted = false;
                pSD->mounted = false;
                
                // Forçar estado não inicializado
                pSD->m_Status = STA_NOINIT;
                
                printf("SD desmontado com sucesso\n");
            } else {
                printf("Erro ao desmontar SD: %d\n", fr);
            }
        }
    } else {
        printf("SD ja estava desmontado\n");
    }
}

static void update_display(void) {
    ssd1306_fill(&ssd, false);
    
    // Desenhar informações no display em enquadramento definido
    char line1[32] = "";
    char line2[32] = "";
    char line3[32] = "";
    char line4[32] = "";
    
    switch (current_state) {
        case STATE_INIT:
            strcpy(line1, "Inicializando...");
            break;
            
        case STATE_READY:
            strcpy(line1, "Sistema Pronto");
            strcpy(line2, sd_mounted ? "SD: OK" : "SD: --");
            strcpy(line3, "A: Iniciar");
            strcpy(line4, "B: Montar/Desm SD");
            break;
            
        case STATE_RECORDING:
            strcpy(line1, "Gravando...");
            sprintf(line2, "%s", filename); // Nome do arquivo atual
            sprintf(line3, "Amostras: %lu", sample_count);
            
            // Calcular tempo decorrido
            uint32_t elapsed_s = absolute_time_diff_us(recording_start_time, get_absolute_time()) / 1000000;
            sprintf(line4, "%02lu:%02lu  A:Parar", elapsed_s / 60, elapsed_s % 60);
            break;
            
        case STATE_ERROR:
            strcpy(line1, "ERRO!");
            strcpy(line2, sd_mounted ? "" : "SD Nao Detectado");
            strcpy(line3, "B: Tentar SD");
            break;
            
        case STATE_SD_ACCESS:
            strcpy(line1, "Acessando SD...");
            strcpy(line2, "Aguarde...");
            break;
            
        case STATE_WAIT_SD_INSERT:
            strcpy(line1, "Insira o SD");
            strcpy(line2, "e pressione B");
            strcpy(line3, "para montar");
            break;
            
        case STATE_WAIT_SD_REMOVE:
            strcpy(line1, "SD Desmontado!");
            strcpy(line2, "Remova o cartao");
            strcpy(line3, "com seguranca");
            strcpy(line4, "B: Novo cartao");
            break;
            
        default:
            strcpy(line1, "Estado Desconhecido");
            break;
    }
    
    ssd1306_draw_string(&ssd, line1, 0, 0);
    if (strlen(line2) > 0) ssd1306_draw_string(&ssd, line2, 0, 16);
    if (strlen(line3) > 0) ssd1306_draw_string(&ssd, line3, 0, 32);
    if (strlen(line4) > 0) ssd1306_draw_string(&ssd, line4, 0, 48);
    
    ssd1306_send_data(&ssd);
}

// Função para encontrar o próximo número de arquivo disponível
static uint32_t find_next_file_number(void) {
    DIR dir;
    FILINFO fno;
    uint32_t max_num = 0;
    
    // Abrir diretório raiz
    FRESULT fr = f_opendir(&dir, "/");
    if (fr != FR_OK) {
        printf("Erro ao abrir diretorio: %d\n", fr);
        return 0;
    }
    
    // Procurar por arquivos data_XXXX.csv
    while (1) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;
        
        // Verificar se é um arquivo data_XXXX.csv
        if (strncmp(fno.fname, "data_", 5) == 0) {
            // Extrair o número do arquivo
            char *end_ptr;
            uint32_t num = strtoul(fno.fname + 5, &end_ptr, 10);
            
            // Verificar se a conversão foi bem sucedida e se termina com .csv
            if (end_ptr && strcmp(end_ptr, ".csv") == 0) {
                if (num > max_num) {
                    max_num = num;
                }
            }
        }
    }
    
    f_closedir(&dir);
    
    // Limite de segurança para evitar overflow
    if (max_num >= 9999) {
        printf("AVISO: Numero maximo de arquivos atingido!\n");
        return 0; // Vai sobrescrever data_0000.csv
    }
    
    printf("Proximo numero de arquivo: %lu\n", max_num + 1);
    return max_num + 1;
}

static bool start_recording(void) {
    if (!sd_mounted) {
        printf("Erro: SD nao montado!\n");
        return false;
    }
    
    // Encontrar o próximo número de arquivo disponível
    uint32_t file_number = find_next_file_number();
    
    // Tentar até 10 vezes caso haja conflito
    int attempts = 0;
    FILINFO fno;
    do {
        sprintf(filename, "data_%04lu.csv", file_number);
        
        // Verificar se o arquivo já existe
        FRESULT fr = f_stat(filename, &fno);
        if (fr == FR_NO_FILE) {
            // Arquivo não existe, podemos criar
            break;
        } else if (fr == FR_OK) {
            // Arquivo existe, tentar próximo número
            printf("AVISO: Arquivo %s ja existe! Tentando proximo...\n", filename);
            file_number++;
            attempts++;
        } else {
            // Outro erro
            printf("Erro ao verificar arquivo: %d\n", fr);
            return false;
        }
    } while (attempts < 10);
    
    if (attempts >= 10) {
        printf("ERRO: Nao foi possivel encontrar nome de arquivo disponivel!\n");
        return false;
    }
    
    printf("Criando arquivo: %s\n", filename);
    
    // Abrir arquivo
    FRESULT fr = f_open(&data_file, filename, FA_WRITE | FA_CREATE_NEW); // CREATE_NEW falha se arquivo existir
    if (FR_OK != fr) {
        printf("Erro ao abrir arquivo: %d\n", fr);
        return false;
    }
    
    // Escrever cabeçalho
    const char *header = "numero_amostra,accel_x,accel_y,accel_z,giro_x,giro_y,giro_z\n";
    UINT bw;
    fr = f_write(&data_file, header, strlen(header), &bw);
    if (FR_OK != fr) {
        printf("Erro ao escrever cabecalho: %d\n", fr);
        f_close(&data_file);
        return false;
    }
    
    // Fazer sync inicial
    f_sync(&data_file);
    
    sample_count = 0;
    recording_start_time = get_absolute_time();
    recording = true;
    current_state = STATE_RECORDING;
    
    printf("Gravacao iniciada no arquivo: %s\n", filename);
    return true;
}

static void stop_recording(void) {
    if (recording) {
        // Garantir que todos os dados sejam escritos
        f_sync(&data_file);
        
        // Obter tamanho do arquivo antes de fechar
        FILINFO fno;
        FRESULT fr = f_stat(filename, &fno);
        
        f_close(&data_file);
        recording = false;
        current_state = STATE_READY;
        
        // Calcular tempo total de gravação
        uint32_t total_time_s = absolute_time_diff_us(recording_start_time, get_absolute_time()) / 1000000;
        
        printf("\n=== Gravacao Finalizada ===\n");
        printf("Arquivo: %s\n", filename);
        printf("Amostras: %lu\n", sample_count);
        printf("Tempo: %lu:%02lu\n", total_time_s / 60, total_time_s % 60);
        if (fr == FR_OK) {
            printf("Tamanho: %lu bytes\n", fno.fsize);
        }
        printf("===========================\n\n");
        
        // Mostrar mensagem de sucesso
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "Dados Salvos!", 20, 10);
        
        char msg[32];
        sprintf(msg, "%s", filename);
        ssd1306_draw_string(&ssd, msg, 20, 26);
        
        sprintf(msg, "%lu amostras", sample_count);
        ssd1306_draw_string(&ssd, msg, 20, 42);
        
        ssd1306_send_data(&ssd);
        
        sleep_ms(3000); // Mostrar por 3 segundos
    }
}

static void write_data_to_csv(mpu6050_data_t *data) {
    if (!recording) return;
    
    char buffer[128];
    sprintf(buffer, "%lu,%d,%d,%d,%d,%d,%d\n",
            sample_count + 1,
            data->accel_x, data->accel_y, data->accel_z,
            data->gyro_x, data->gyro_y, data->gyro_z);
    
    UINT bw;
    FRESULT fr = f_write(&data_file, buffer, strlen(buffer), &bw);
    
    if (FR_OK == fr) {
        sample_count++;
        
        // Fazer flush a cada 50 amostras para garantir que dados sejam salvos
        if (sample_count % 50 == 0) {
            f_sync(&data_file);
            
            // Piscar LED azul durante sync
            set_led_color(0, 0, 1);
            sleep_ms(10);
            set_led_color(1, 0, 0); // Voltar para vermelho (gravando)
        }
    } else {
        printf("Erro ao escrever no arquivo: %d\n", fr);
        // Em caso de erro, parar gravação
        stop_recording();
        current_state = STATE_ERROR;
        buzzer_beep(5, 50); // 5 beeps de erro
    }
}

// Handlers de interrupção simplificados - setam flags para o loop principal
static void button_irq_handler(uint gpio, uint32_t events) {
    if (gpio == BOTAO_A && (events & GPIO_IRQ_EDGE_FALL)) {
        button_a_pressed = true;
    } else if (gpio == BOTAO_B && (events & GPIO_IRQ_EDGE_FALL)) {
        button_b_pressed = true;
    }
}

// Funções para processar os botões no loop principal
static void process_button_a(void) {
    if (current_state == STATE_READY && sd_mounted) {
        // Iniciar gravação
        if (start_recording()) {
            buzzer_beep(1, 100);
            set_led_color(1, 0, 0); // Vermelho
        } else {
            // Falha ao iniciar gravação
            buzzer_beep(3, 50);
            printf("Erro ao iniciar gravacao!\n");
        }
    } else if (current_state == STATE_RECORDING) {
        // Parar gravação
        stop_recording();
        buzzer_beep(2, 100);
        set_led_color(0, 1, 0); // Verde
    } else if (current_state == STATE_WAIT_SD_REMOVE) {
        // Se estiver aguardando remover SD, voltar para READY se SD ainda montado
        if (sd_mounted) {
            current_state = STATE_READY;
            set_led_color(0, 1, 0); // Verde
            update_display();
        }
    } else {
        // Estado inválido para iniciar gravação
        buzzer_beep(3, 50);
        printf("Estado invalido para botao A: %d\n", current_state);
    }
}

static void process_button_b(void) {
    // Não permitir durante gravação
    if (current_state == STATE_RECORDING) {
        buzzer_beep(3, 50); // Beeps de erro
        return;
    }
    
    if (sd_mounted) {
        // Se SD está montado, desmontar
        printf("Desmontando SD para remocao segura...\n");
        
        set_led_color(1, 1, 0); // Amarelo
        current_state = STATE_SD_ACCESS;
        update_display();
        
        unmount_sd_card();
        
        if (!sd_mounted) {
            current_state = STATE_WAIT_SD_REMOVE;
            set_led_color(1, 0, 0); // Vermelho - aguardando remover
            buzzer_beep(2, 100);
            printf("SD desmontado. Remova o cartao com seguranca.\n");
        } else {
            current_state = STATE_ERROR;
            set_led_color(1, 0, 1); // Roxo - erro
            buzzer_beep(3, 50);
        }
    } else {
        // Se SD não está montado, tentar montar
        printf("Tentando montar SD...\n");
        
        set_led_color(0, 0, 1); // Azul
        current_state = STATE_SD_ACCESS;
        update_display();
        
        // Dar um tempo extra para o cartão estabilizar
        sleep_ms(500);
        
        init_sd_card();
        
        if (sd_mounted) {
            buzzer_beep(1, 100);
            current_state = STATE_READY;
            set_led_color(0, 1, 0); // Verde
            printf("SD montado com sucesso!\n");
        } else {
            buzzer_beep(3, 50); // Erro
            current_state = STATE_WAIT_SD_INSERT;
            set_led_color(1, 1, 0); // Amarelo piscando - aguardando inserir
            printf("Insira um cartao SD e pressione B novamente.\n");
        }
    }
    
    update_display();
}