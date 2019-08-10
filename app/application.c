#include <application.h>

#define RADIO_DELAY 10000

#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)


// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

// Accelerometer
bc_lis2dh12_t acc;
bc_lis2dh12_result_g_t result;

bc_lis2dh12_alarm_t alarm;


bc_tick_t startSeconds = 0;
bc_tick_t endSeconds = 0;

bool playing = false;

void lis2_event_handler(bc_lis2dh12_t *self, bc_lis2dh12_event_t event, void *event_param)
{
    float holdTime;

    if (event == BC_LIS2DH12_EVENT_UPDATE)
    {
        bc_lis2dh12_get_result_g(&acc, &result);
    }
    else if (event == BC_LIS2DH12_EVENT_ALARM && playing)
    {
        endSeconds = bc_tick_get();
        playing = false;
        static bc_tick_t radio_delay = 0;
        bc_led_set_mode(&led, BC_LED_MODE_OFF);
        if (bc_tick_get() >= radio_delay)
        {
            // Make longer pulse when transmitting
            bc_led_pulse(&led, 100);

            holdTime = ((endSeconds - startSeconds) / (float)1000);

            bc_radio_pub_float("hold-time", &holdTime);

            radio_delay = bc_tick_get() + RADIO_DELAY;

        }
    }
    else
    {
        bc_log_debug("Not playing");
    }
}

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{ 
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        playing = true;
        startSeconds = bc_tick_get();
        bc_led_set_mode(&led, BC_LED_MODE_ON);
        bc_radio_pub_event_count(BC_RADIO_PUB_EVENT_PUSH_BUTTON, NULL);
    }

}

// This function dispatches battery events
void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    // Update event?
    if (event == BC_MODULE_BATTERY_EVENT_UPDATE)
    {
        float voltage;

        // Read battery voltage
        if (bc_module_battery_get_voltage(&voltage))
        {
            bc_log_info("APP: Battery voltage = %.2f", voltage);

            // Publish battery voltage
            bc_radio_pub_battery(&voltage);
        }
    }
}

void application_init(void)
{
    // Initialize logging
    bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_pulse(&led, 2000);

    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, 0);
    bc_button_set_event_handler(&button, button_event_handler, NULL);


    // Initialize battery
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize radio
    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);

    // Send radio pairing request
    bc_radio_pairing_request("still-position-detector", VERSION);

    bc_lis2dh12_init(&acc, BC_I2C_I2C0, 0x19);
    bc_lis2dh12_set_event_handler(&acc, lis2_event_handler, NULL);

    memset(&alarm, 0, sizeof(alarm));

    // Activate alarm when axis Z is above 0.25 or below -0.25 g
    alarm.z_high = true;
    alarm.threshold = 0.7;
    alarm.duration = 0;

    bc_lis2dh12_set_alarm(&acc, &alarm);
}
