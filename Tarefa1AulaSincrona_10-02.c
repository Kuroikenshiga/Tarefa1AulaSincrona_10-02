#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "math.h"
#include "pico/bootrom.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"

const uint PIN_B = 12, PIN_R = 13, PIN_G = 11, PIN_VRX = 27, PIN_VRY = 26, PIN_BTN_JOY = 22, I2C_SCL = 15, I2C_SDA = 14, PIN_BTN_A = 5;
#define endereco 0x3C

int dot_position_x = 0, dot_position_y = 0;
uint SLICE_PIN_B, SLICE_PIN_R;
ssd1306_t ssd;
bool have_rect = false, pwm_enabled = true;
int64_t last_event_JY = 0, last_event_PIN_A = 0;
uint SLICE_PIN_B, SLICE_PIN_R;
void irq_handler_callback(uint gpio, uint32_t events);
// desenha um quadrado na tela
void put_pixel();
// configuração o firmware
void setup();
// gerencia os valores recebidos pelo conversor
void manage_joystick();

int main()
{
    stdio_init_all();
    SLICE_PIN_B = pwm_gpio_to_slice_num(PIN_B);
    SLICE_PIN_R = pwm_gpio_to_slice_num(PIN_R);
    setup();

    ssd1306_draw_rect(&ssd, 1, 1);
    ssd1306_send_data(&ssd);

    while (true)
    {
        manage_joystick();

        put_pixel();
        sleep_ms(10);
    }
}

void irq_handler_callback(uint gpio, uint32_t events)
{

    int64_t current_event = to_us_since_boot(get_absolute_time());
    if (gpio == PIN_BTN_JOY && current_event - last_event_JY > 200000)
    {
        last_event_JY = current_event;
        if (!have_rect)
        {
            // Desenha um novo retângulo
            ssd1306_draw_rect(&ssd, 3, 3);
            ssd1306_send_data(&ssd);
            have_rect = true;
        }
        else
        {
            have_rect = false;
            // apaga um retângulo
            clear_rect(&ssd, 3, 3);
            ssd1306_send_data(&ssd);
        }
        gpio_put(PIN_G, have_rect);
    }
    if (gpio == PIN_BTN_A && current_event - last_event_PIN_A > 200000)
    {
        last_event_PIN_A = current_event;
        pwm_enabled = !pwm_enabled;

        pwm_set_enabled(SLICE_PIN_B, pwm_enabled);
        pwm_set_enabled(SLICE_PIN_R, pwm_enabled);
    }
}
void put_pixel()
{

    ssd1306_clear_rect(&ssd, have_rect);
    ssd1306_send_data(&ssd);
    draw_square(&ssd, dot_position_x, dot_position_y);
    ssd1306_send_data(&ssd);
}
void setup()
{

    gpio_set_function(PIN_B, GPIO_FUNC_PWM);
    pwm_set_wrap(SLICE_PIN_B, 2000);
    pwm_set_clkdiv(SLICE_PIN_B, 8);
    pwm_set_enabled(SLICE_PIN_B, 1);

    gpio_set_function(PIN_R, GPIO_FUNC_PWM);
    pwm_set_wrap(SLICE_PIN_R, 2000);
    pwm_set_clkdiv(SLICE_PIN_R, 8);
    pwm_set_enabled(SLICE_PIN_R, 1);

    gpio_init(PIN_G);
    gpio_set_dir(PIN_G, GPIO_OUT);
    gpio_pull_up(PIN_G);

    adc_init();
    adc_gpio_init(PIN_VRX);
    adc_gpio_init(PIN_VRY);
    gpio_pull_up(PIN_BTN_JOY);
    gpio_pull_up(PIN_BTN_JOY);
    gpio_set_irq_enabled_with_callback(PIN_BTN_JOY, GPIO_IRQ_EDGE_FALL, 1, &irq_handler_callback);
    gpio_init(PIN_BTN_A);
    gpio_set_dir(PIN_BTN_A, GPIO_IN);
    gpio_pull_up(PIN_BTN_A);
    gpio_set_irq_enabled_with_callback(PIN_BTN_A, GPIO_IRQ_EDGE_FALL, 1, &irq_handler_callback);
    // I2C Initialisation. Using it at 400Khz.

    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, i2c1); // Inicializa o display
    ssd1306_config(&ssd);                                     // Configura o display
    ssd1306_send_data(&ssd);                                  // Envia os dados para o display
}
void manage_joystick()
{
    adc_select_input(1);
    sleep_us(10);
    uint16_t adc_values_x = adc_read();
    adc_values_x = abs(adc_values_x - 2048) > 200 ? adc_values_x : 2048;

    adc_select_input(0);
    sleep_us(10);
    uint16_t adc_values_y = adc_read();
    adc_values_y = abs(adc_values_y - 2048) > 200 ? adc_values_y : 2048;

    dot_position_x = (((float)adc_values_x / 4095)) * 128;

    pwm_set_gpio_level(PIN_R, (((float)abs(adc_values_x - 2048) / 2048)) * 2000);

    dot_position_y = adc_values_y > 2048?64 - (((float)adc_values_y / 4095)) * 64:(-1*((float)(((float)adc_values_y / 4095) - 1)) * 64);
    pwm_set_gpio_level(PIN_B, (((float)abs(adc_values_y - 2048) / 2048)) * 2000);

    dot_position_x = dot_position_x >= 120?dot_position_x - 15:dot_position_x <= 8?dot_position_x+8:dot_position_x;
    dot_position_y = dot_position_y >= 56?dot_position_y - 15:dot_position_y <= 8?dot_position_y+8:dot_position_y;
    if (adc_values_y == 2048 && adc_values_x == 2048)
    {
        dot_position_x = 60, dot_position_y = 28;
        pwm_set_gpio_level(PIN_R, 0);
        pwm_set_gpio_level(PIN_B, 0);
    }
}