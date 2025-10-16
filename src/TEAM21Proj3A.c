#include "hat.h"
#include <stdio.h>
volatile float distance = 0.0;
volatile uint32_t pulse_duration = 0;
volatile uint32_t rise_time = 0;
volatile uint32_t fall_time = 0;
volatile uint8_t new_data_ready = 0;
volatile uint8_t unit = 0; // 0 for cm, 1 for inches

void delay_us(uint32_t us){
    uint32_t start = SysTick->VAL;
    uint32_t ticks = (SYSTEM_FREQ / 1000000) * us;
    while((SysTick->VAL - start) < ticks);
    return;
}

void SysTick_Handler(void){
    write_pin(ULTRA_SOUND.TRIG_PORT, ULTRA_SOUND.TRIG_PIN, HIGH);
    delay_us(10);
    write_pin(ULTRA_SOUND.TRIG_PORT, ULTRA_SOUND.TRIG_PIN, LOW);
}
void EXTI0_IRQHandler(void){
    if(EXTI->PR & EXTI_PR_PR0){
        if(ULTRA_SOUND.ECHO_PORT->IDR & (1 << ULTRA_SOUND.ECHO_PIN)){
            rise_time = TIM2->CNT;
        }else{
            fall_time = TIM2->CNT;
            pulse_duration = (fall_time >= rise_time) ? (fall_time - rise_time) : (0xFFFFFFFF - rise_time + fall_time + 1);
            pulse_duration /= 16; // convert to microseconds
            distance = unit ?  ((pulse_duration) / 148.0) : ((pulse_duration) / 58.0); // in cm
            new_data_ready = 1; // Set a flag to process data in the main loop
        }
        EXTI->PR |= EXTI_PR_PR0;
    }
    return;
}
void EXTI15_10_IRQHandler(void){
    if(EXTI->PR & EXTI_PR_PR13){
        // User button pressed, can implement any functionality here
        EXTI->PR |= EXTI_PR_PR13;
        unit = !unit;
    }
}

int main() {
    init_usart(115200);
    init_ssd(10);
    display_num(0, 0);
    init_ultrasound();
    init_sys_tick(8000000); // 500ms period
    init_gp_timer(TIM2, 1000000, 0xFFFFFFFF); // 1MHz timer for microsecond precision
    
    // Configure EXTI for ultrasound echo pin (PB0)
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; // enable SYSCFG clock
    SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PB; // EXTI0 from PB0
    EXTI->IMR |= EXTI_IMR_IM0; // unmask EXTI0
    EXTI->RTSR |= EXTI_RTSR_TR0; // rising edge trigger
    EXTI->FTSR |= EXTI_FTSR_TR0; // falling edge trigger
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_SetPriority(EXTI0_IRQn, 0);

    // Configure user buttom (PC13)
    set_pin_mode(GPIOC, 13, INPUT);
    set_pin_pull(GPIOC, 13, PULL_UP);
    SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI13_PC; // EXTI13 from PC13
    EXTI->IMR |= EXTI_IMR_IM13; // unmask EXTI13
    EXTI->FTSR |= EXTI_FTSR_TR13; // falling edge trigger
    NVIC_EnableIRQ(EXTI15_10_IRQn);
    NVIC_SetPriority(EXTI15_10_IRQn, 1);

    while(1){
        if (new_data_ready) {
            // Safely read volatile variables
            float current_distance = distance;
            uint32_t current_pulse_duration = pulse_duration;

            if(current_distance > 99.99){
                display_num(9999, 2);
            }else{
                display_num((uint16_t)(current_distance*100), 2);
            }
            char str[40];
            if(unit){
                sprintf(str, "Dist: %.2fin\tWidth: %lu\tTIM2: %lu\r\n", current_distance, current_pulse_duration, TIM2->CNT);
            }else{
                sprintf(str, "Dist: %.2fcm\tWidth: %lu\tTIM2: %lu\r\n", current_distance, current_pulse_duration, TIM2->CNT);
            }
            for(int i = 0; str[i] != '\0'; i++) {
                send_char(USART2, str[i]);
            }
            new_data_ready = 0; // Clear the flag
        }
    };
    return 0;
}