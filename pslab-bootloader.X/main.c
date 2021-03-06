/**
 * @file main.c
 * @brief PSLab bootloader.
 *
 * The bootloader loads the main application, if one exists. If the GPIO pin
 * (RC5) is grounded, the device will stay in bootloader mode even if an
 * application exists. While in bootloader mode, a new application can be
 * flashed with the Unified Bootloader Host Application, or a similar tool.
 */

#include "mcc_generated_files/system.h"
#include "mcc_generated_files/boot/boot_process.h"
#include "mcc_generated_files/pin_manager.h"

#include "delay.h"
#include "rgb_led.h"

int main(void) {
    // Initialize the device.
    SYSTEM_Initialize();

    Light_RGB(20, 0, 0);
    DELAY_ms(200);
    Light_RGB(0, 20, 0);
    DELAY_ms(200);
    Light_RGB(0, 0, 20);

    GPIO_PIN_SetDigitalOutput();
    GPIO_PIN_SetHigh();
    DELAY_us(1000); // Wait for GPIO to go high.

    // If GPIO is grounded or no application is detected, stay in bootloader.
    if (GPIO_PIN_GetValue() && BOOT_Verify()) {
        BOOT_StartApplication();
    } else {
        uint16_t i = 0;

        while (1) {
            // Monitor serial bus for commands, e.g. flashing new application.
            BOOT_ProcessCommand();

            // Blink system LED while in bootloader mode.
            if (!i++) STATUS_LED_Toggle();
        }
    }

    return 1;
}
