#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "pico/bootrom.h"
#include "hardware/clocks.h"
#include "lib/numeros.h"

#include "characters_display.pio.h"

// Define os pinos dos LEDs e botões
const uint led_pin_green = 11;
const uint led_pin_blue = 12;
const uint button_A = 5;
const uint button_B = 6;
const uint button_J = 22;

// Define o número de LEDs e o pino do LED Matrix
#define NUM_PIXELS 25
#define MATRIX_PIN 7

// Define os pinos do display OLED e o endereço I2C
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

// Inicializa a estrutura do display
ssd1306_t ssd;
bool cor = true; //Cor do display

static volatile uint32_t last_time = 0; // Armazena o tempo da última interrupção 
int num = 0; //Armazena o número atualmente exibido

double* nums[10] = {num_zero, num_um, num_dois, num_tres, num_quatro, num_cinco, num_seis, num_sete, num_oito, num_nove}; //Ponteiros para os desenhos dos números

// Rotina da interrupção dos botões
static void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time()); // Armazena o tempo atual
    if (current_time - last_time > 200000) { // Debounce de 200ms
        last_time = current_time;
        switch (gpio) {
            case button_A: // Interrupção do botão A
                printf("Botão A pressionado\n");
                gpio_put(led_pin_green, !gpio_get(led_pin_green)); // Inverte o estado do LED verde
                gpio_get(led_pin_green) ? printf("LED verde ON\n") : printf("LED verde OFF\n"); // Imprime no serial monitor o estado do LED verde
                ssd1306_fill(&ssd, !cor); // Limpa o display
                ssd1306_draw_string(&ssd, "Pressionado:", 15, 10); 
                ssd1306_draw_string(&ssd, "Botao A", 32, 20);
                gpio_get(led_pin_green) ? ssd1306_draw_string(&ssd, "LED verde ON", 15, 40) : ssd1306_draw_string(&ssd, "LED verde OFF", 15, 40); // Exibe no display o estado do LED verde
                ssd1306_send_data(&ssd); // Atualiza o display
                break;
            case button_B: // Interrupção do botão B
                printf("Botão B pressionado\n");
                gpio_put(led_pin_blue, !gpio_get(led_pin_blue)); // Inverte o estado do LED azul
                gpio_get(led_pin_blue) ? printf("LED azul ON\n") : printf("LED azul OFF\n"); // Imprime no serial monitor o estado do LED azul
                ssd1306_fill(&ssd, !cor); // Limpa o display
                ssd1306_draw_string(&ssd, "Pressionado:", 15, 10);
                ssd1306_draw_string(&ssd, "Botao B", 32, 20);
                gpio_get(led_pin_blue) ? ssd1306_draw_string(&ssd, "LED azul ON", 15, 40) : ssd1306_draw_string(&ssd, "LED azul OFF", 15, 40); // Exibe no display o estado do LED azul
                ssd1306_send_data(&ssd); // Atualiza o display
                break;
            case button_J: // Interrupção do botão J (Feito apenas para entrar no modo bootsel de maneira mais fácil)
                printf("Botão J pressionado\n");
                printf("Entrando no modo bootsel\n");
                reset_usb_boot(0,0); // Habilita o modo de gravação do microcontrolador
            break;
        }
    }
}

// Rotina para definição da intensidade de cores do led
uint32_t matrix_rgb(double r, double g, double b) {
    unsigned char R, G, B;
    R = r * 255;
    G = g * 255;
    B = b * 255;
    return (G << 24) | (R << 16) | (B << 8);
}

// Rotina para acionar a matrix de leds - ws2812b
void desenho_pio(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b) {
    for (int16_t i = 0; i < NUM_PIXELS; i++) {
        valor_led = matrix_rgb(r=0.0, desenho[24-i], b=0.0);
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}

// Configura as GPIOs
void setup_GPIOs() {
    gpio_init(led_pin_green);
    gpio_set_dir(led_pin_green, GPIO_OUT);
    gpio_init(led_pin_blue);
    gpio_set_dir(led_pin_blue, GPIO_OUT);

    gpio_init(button_A);
    gpio_set_dir(button_A, GPIO_IN);
    gpio_pull_up(button_A);

    gpio_init(button_B);
    gpio_set_dir(button_B, GPIO_IN);
    gpio_pull_up(button_B);

    gpio_init(button_J);
    gpio_set_dir(button_J, GPIO_IN);
    gpio_pull_up(button_J);
}

int main()
{
    stdio_init_all();

    setup_GPIOs(); // Configura as GPIOs

    // Configurações da PIO e matriz de leds
    PIO pio = pio0; 
    bool ok;
    uint32_t valor_led;
    double r = 0.0, g = 0.0 , b = 0.0;

    //Coloca a frequência de clock para 128 MHz, facilitando a divisão pelo clock
    ok = set_sys_clock_khz(128000, false);

    //Configurações da PIO
    uint offset = pio_add_program(pio, &pio_matrix_program);
    uint sm = pio_claim_unused_sm(pio, true);
    pio_matrix_program_init(pio, sm, offset, MATRIX_PIN);

    // Inicia a porta I2C a 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    // Configura os pinos SDA e SCL para a porta I2C
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Configura o pino SDA para a porta I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Configura o pino SCL para a porta I2C
    gpio_pull_up(I2C_SDA); // Ativa o pull up no pino SDA
    gpio_pull_up(I2C_SCL); // Ativa o pull up no pino SCL
     // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    gpio_set_irq_enabled_with_callback(button_J, GPIO_IRQ_EDGE_FALL, 1, & gpio_irq_handler); // Interrupção do botão J
    gpio_set_irq_enabled_with_callback(button_A, GPIO_IRQ_EDGE_FALL, 1 , & gpio_irq_handler); // Interrupção do botão A
    gpio_set_irq_enabled_with_callback(button_B, GPIO_IRQ_EDGE_FALL, 1 , & gpio_irq_handler); // Interrupção do botão B

    while (true) {
        char c;
        if (scanf("%c", &c) == 1 && stdio_usb_connected()) {
            printf("Recebido: %c\n", c);

            if (c >= 'A' && c <= 'Z') {
                ssd1306_fill(&ssd, !cor);
                ssd1306_draw_string(&ssd, "Recebido:", 30, 30);
                ssd1306_draw_char(&ssd, c, 60, 40);
            } else if (c >= 'a' && c <= 'z') {
                ssd1306_fill(&ssd, !cor);
                ssd1306_draw_string(&ssd, "Recebido:", 30, 30);
                ssd1306_draw_char(&ssd, c, 60, 40);
            } else if (c >= '0' && c <= '9') {
                num = c - '0';
                ssd1306_fill(&ssd, !cor);
                ssd1306_draw_string(&ssd, "Recebido:", 30, 30);
                ssd1306_draw_char(&ssd, c, 60, 40);
                desenho_pio(nums[num], valor_led, pio, sm, r, g, b);
            }
        }
        ssd1306_send_data(&ssd); // Atualiza o display
        sleep_ms(100); 
    }
}
