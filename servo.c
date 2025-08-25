#include "servo.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"

void inicializar_pwm_servo(void) {
    gpio_set_function(PINO_SERVO, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PINO_SERVO);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 125.0f);
    pwm_config_set_wrap(&config, 20000);
    pwm_init(slice_num, &config, true);
}

// Para servo contínuo: 0 = parar, 1 = direita (abrir), 2 = esquerda (fechar)
void servo_posicao(uint8_t posicao) {
    float pulso_ms = 1.5f; // Neutro (parado)
    switch (posicao) {
        case 1: // Abrir (direita)
            pulso_ms = 2.0f;
            break;
        case 2: // Fechar (esquerda)
            pulso_ms = 1.0f;
            break;
        default: // Parado
            pulso_ms = 1.5f;
            break;
    }
    uint16_t nivel_pwm = (uint16_t)((pulso_ms / 20.0f) * 20000.0f);
    pwm_set_gpio_level(PINO_SERVO, nivel_pwm);
}