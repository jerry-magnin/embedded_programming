#include <stdio.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_sleep.h"

#define S_TO_US(x) (x*1000000)
#define SLEEP_DURATION_S 10
#define LIGHTSENSOR GPIO_NUM_12
#define DETECTOR GPIO_NUM_33

RTC_DATA_ATTR uint8_t count = 0;
RTC_DATA_ATTR uint16_t light_threshold = 2048; // Half the value
uint16_t light_measure = 2000;

bool check_pin(gpio_num_t pin) {
    return(gpio_get_level(pin));
}

esp_err_t pulse_port(gpio_num_t pin, uint32_t ms_delay) {
    esp_err_t ret = gpio_set_level(pin, 1);
    if(ret!=ESP_OK) {
        return(ret);
    }
    vTaskDelay(ms_delay/portTICK_PERIOD_MS);
    ret = gpio_set_level(pin, 0);
    return(ret);
}

adc_oneshot_unit_handle_t adc_handle;
adc_oneshot_unit_init_cfg_t adc_config = {
    .unit_id = ADC_UNIT_1,
};
adc_oneshot_chan_cfg_t channel_config = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_12,
};
uint16_t measure_light_sensor(adc_channel_t channel) {
    int adc_raw = 0;
    adc_oneshot_read(adc_handle, channel, &adc_raw);
    return(adc_raw);
}

RTC_DATA_ATTR bool lumiere=false;
#define RELAY_ON_PIN GPIO_NUM_26
#define RELAY_OFF_PIN GPIO_NUM_27

#define GPIO_OUTPUT_SEL ((1ULL<<RELAY_ON_PIN) | (1ULL<<RELAY_OFF_PIN))
gpio_config_t gpio_conf = {
    .intr_type = GPIO_INTR_DISABLE, // Désactiver les interrupts
    .mode = GPIO_MODE_OUTPUT, // Sortie uniquement
    .pin_bit_mask = GPIO_OUTPUT_SEL, // Nos pins précédemment sélectionnés
    .pull_down_en = 0, // Nous n’activons pas le pull-down intégré…
    .pull_up_en = 0, // … Ni les pull-ups
};

void init_gpio() {
    gpio_set_direction(RELAY_ON_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(RELAY_OFF_PIN, GPIO_MODE_OUTPUT);
    // could do with gpio_config()
}

void app_main(void)
{   
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    //init_gpio();
    gpio_config(&gpio_conf);
    gpio_set_level(RELAY_OFF_PIN, 0);
    gpio_set_level(RELAY_ON_PIN, 0);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    // Configuration de l’ADC
    adc_oneshot_new_unit(&adc_config, &adc_handle);
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_0, &channel_config);

    printf("Cycle %d – ", count);
    ++count;
    switch(wakeup_cause) {
        case ESP_SLEEP_WAKEUP_EXT0:
            printf("Woke-up from EXT0 — ");
            if(!lumiere){
                pulse_port(RELAY_ON_PIN, 10);
                printf("Light OFF, switching ON — ");
                lumiere = true;
            }
            vTaskDelay(2000/portTICK_PERIOD_MS);
        break;
        case ESP_SLEEP_WAKEUP_TIMER:
            printf("Woke-up from timer — ");
            light_measure = measure_light_sensor(ADC_CHANNEL_0);
            printf("Light level = %d – ", light_measure);
            if(lumiere){
                printf("Light ON, switching OFF – ");
                pulse_port(RELAY_OFF_PIN, 10);
                lumiere = false;
            }
        break;
        default:
            printf("Unknown wake-up cause — ");
        break;
    };

    esp_sleep_enable_timer_wakeup(S_TO_US(SLEEP_DURATION_S));

    if(light_measure < light_threshold) {
        printf("Waking from EXT0 enabled – ");
        esp_sleep_enable_ext0_wakeup(DETECTOR, 1);
    } else {
        printf("Waking from EXT0 disabled — ");
    }
    
    printf("Going to sleep\n");
    esp_deep_sleep_start();
}
