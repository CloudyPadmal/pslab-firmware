#include "../commands.h"
#include "../helpers/delay.h"
#include "../registers/timers/tmr5.h"
#include "../registers/converters/adc1.h"
#include "../registers/memory/dma.h"
#include "../bus/uart/uart1.h"
#include "../registers/system/pin_manager.h"
#include "../registers/converters/ctmu.h"

response_t MULTIMETER_GetVoltage(void) {

    uint8_t channel = UART1_Read();

    ADC1_InterruptDisable();
    ADC1_InterruptFlagClear();

    DMA_ChannelDisable(DMA_CHANNEL_0);

    ADC1_Disable();

    ADC1_InitializeCON1();
    AD1CON1bits.AD12B = 1;
    AD1CON1bits.SSRC = 0b111;

    ADC1_InitializeCON2();

    AD1CON3bits.ADRC = 1;
    AD1CON3bits.SAMC = 0b11111;
    AD1CON3bits.ADCS = 0b00001001;

    ADC1_InitializeCON4();

    AD1CHS0bits.CH0SA = channel & 0xF;
    AD1CHS0bits.CH0NA = 0;

    AD1CSSH = 0x0000;
    AD1CSSL = 0x0000;

    ADC1_Enable();
    DELAY_us(20);
    ADC1_SoftwareTriggerEnable();

    while (!ADC1_IsConversionComplete(channel_CTMU));

    UART1_WriteInt(ADC1_ConversionResultGet(channel_CTMU));

    return SUCCESS;
}

response_t MULTIMETER_GetVoltageSummed(void) {

    uint8_t channel = UART1_Read();

    ADC1_SetOperationMode(ADC1_12BIT_AVERAGING_MODE, channel, 0);

    ADC1_Enable();
    DELAY_us(20);
    ADC1_AutomaticSamplingEnable();

    ADC1_InterruptFlagClear();
    while (!_AD1IF);
    ADC1_InterruptFlagClear();

    while (!ADC1_IsConversionComplete(channel_CTMU));

    ADC1_AutomaticSamplingDisable();
    ADC1_Disable();

    uint16_t voltage_sum =
            (ADC1BUF0) + (ADC1BUF1) + (ADC1BUF2) + (ADC1BUF3) +
            (ADC1BUF4) + (ADC1BUF5) + (ADC1BUF6) + (ADC1BUF7) +
            (ADC1BUF8) + (ADC1BUF9) + (ADC1BUFA) + (ADC1BUFB) +
            (ADC1BUFC) + (ADC1BUFD) + (ADC1BUFE) + (ADC1BUFF);

    UART1_WriteInt(voltage_sum);

    return SUCCESS;
}

response_t MULTIMETER_ChargeCapacitor(void) {

    uint8_t charge = UART1_Read();
    uint16_t period = UART1_ReadInt();

    CAP_OUT_SetDigitalOutput();
    CAP_OUT_SetDigital();

    if (charge) {
        CAP_OUT_SetHigh();
    } else {
        CAP_OUT_SetLow();
    }

    // Wait out until the capacitor is charged
    DELAY_us(period);

    CAP_OUT_SetLow();
    // Switch to high impedance state
    CAP_OUT_SetDigitalInput();
    CAP_OUT_SetAnalog();
    Nop();

    return SUCCESS;
}

response_t MULTIMETER_ReadCapacitance(void) {

    uint8_t current_range = UART1_Read();
    uint8_t trim = UART1_Read();
    uint16_t charge_time = UART1_ReadInt();

    LED_SetHigh();

    uint16_t reading = 0;

    ADC1_SetOperationMode(ADC1_CTMU_MODE, 5, 0);

    CTMU_Initialize();
    // Enables edge delay generation
    CTMUCON1bits.TGEN = 1;
    // Set current level for current source
    CTMUICONbits.ITRIM = trim;
    // Current range for current source (0.53uA for base current)
    CTMUICONbits.IRNG = current_range;

    TMR5_Initialize();
    // Clock pre-scaled by 64
    T5CONbits.TCKPS = 0b10;
    TMR5_Period16BitSet(charge_time);

    // TODO: Check the viability of the following block as it does nothing important
    CAP_OUT_SetDigitalOutput();
    CAP_OUT_SetDigital();
    CAP_OUT_SetLow();
    DELAY_us(50000);
    // Switch to high impedance state
    CAP_OUT_SetDigitalInput();
    CAP_OUT_SetAnalog();

    // Begin ADC sampling
    ADC1_Enable();
    ADC1_InterruptEnable();
    DELAY_us(20);
    ADC1_SoftwareTriggerEnable();

    CTMU_Enable();
    DELAY_us(1000);
    // Analog current source output is grounded
    CTMUCON1bits.IDISSEN = 1;
    DELAY_us(1500);
    // Disconnect current source from ground
    CTMUCON1bits.IDISSEN = 0;
    // Edge 1 has occurred and capacitor is now being charged
    CTMUCON2bits.EDG1STAT = 1;

    TMR5_Start();

    while (!_T5IF);
    _T5IF = 0;

    ADC1_InterruptFlagClear();
    ADC1_SoftwareTriggerDisable();

    // Pause charging the capacitor
    CTMUCON2bits.EDG1STAT = 0;

    while (!_AD1IF);
    ADC1_InterruptFlagClear();

    while (!ADC1_IsConversionComplete(channel_CTMU));

    reading = (ADC1BUF0) & 0xFF;

    // Reset CTMU module
    CTMU_Initialize();

    ADC1_Disable();

    UART1_WriteInt(reading);

    LED_SetLow();

    return SUCCESS;
}
