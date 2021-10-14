#include <application.h>

#define RADIO_DELAY (5000)

#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)


#define APPLICATION_TASK_ID 0


// LED instance
twr_led_t led;

// Button instance
twr_button_t button;

// Accelerometer
twr_lis2dh12_t acc;
twr_lis2dh12_result_g_t result;

twr_lis2dh12_alarm_t alarm;

twr_tick_t radio_delay = 0;

bool washing = false;

void lis2_event_handler(twr_lis2dh12_t *self, twr_lis2dh12_event_t event, void *event_param)
{
    float holdTime;

    if (event == TWR_LIS2DH12_EVENT_UPDATE)
    {
        twr_lis2dh12_get_result_g(&acc, &result);
    }
    else if (event == TWR_LIS2DH12_EVENT_ALARM && washing)
    {
        // Make longer pulse when transmitting
        twr_led_pulse(&led, 100);

        radio_delay = twr_tick_get() + RADIO_DELAY;
        twr_log_debug("updating");
    }
    else
    {
        twr_log_debug("not washing");
    }
}

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    if (event == TWR_BUTTON_EVENT_PRESS)
    {
        radio_delay = twr_tick_get() + RADIO_DELAY;
        washing = true;
        twr_led_set_mode(&led, TWR_LED_MODE_ON);
        twr_radio_pub_event_count(TWR_RADIO_PUB_EVENT_PUSH_BUTTON, NULL);
    }

}

// This function dispatches battery events
void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    // Update event?
    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        float voltage;

        // Read battery voltage
        if (twr_module_battery_get_voltage(&voltage))
        {
            twr_log_info("APP: Battery voltage = %.2f", voltage);

            // Publish battery voltage
            twr_radio_pub_battery(&voltage);
        }
    }
}

void application_init(void)
{
    // Initialize logging
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_pulse(&led, 2000);

    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, 0);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize radio
    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);

    // Send radio pairing request
    twr_radio_pairing_request("washing-machine-detector", VERSION);

    twr_lis2dh12_init(&acc, TWR_I2C_I2C0, 0x19);
    twr_lis2dh12_set_event_handler(&acc, lis2_event_handler, NULL);

    memset(&alarm, 0, sizeof(alarm));

    // Activate alarm when axis Z is above 0.25 or below -0.25 g
    alarm.x_high = true;
    alarm.threshold = 0.3;
    alarm.duration = 0;

    twr_lis2dh12_set_alarm(&acc, &alarm);

    twr_scheduler_plan_now(APPLICATION_TASK_ID);

}



void application_task(void)
{
    if((twr_tick_get() >= radio_delay) && washing)
    {
        washing = false;
        twr_led_set_mode(&led, TWR_LED_MODE_OFF);
        twr_log_debug("finished");
        twr_radio_pub_bool("washing/finished", true);
    }

    twr_scheduler_plan_current_relative(1000);
}
