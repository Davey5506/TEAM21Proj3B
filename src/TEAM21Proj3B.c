#include "hat.h"
#include <stdio.h>
volatile float distance = 0.0;
volatile uint32_t pulse_duration = 0;
volatile uint32_t rise_time = 0;
volatile uint32_t fall_time = 0;
volatile uint8_t new_data_ready = 0;
volatile uint8_t unit = 0; // 0 for cm, 1 for inches
volatile uint32_t pulse_width=0;

void delay_us(uint32_t us){
    uint32_t start = SysTick->VAL;
    uint32_t ticks = (SYSTEM_FREQ / 1000000) * us;
    while((SysTick->VAL - start) < ticks);
    return;
}

void trigger_pulse(void){ //sends a 10us pulse to the trigger pin
    write_pin(ULTRA_SOUND.TRIG_PORT, ULTRA_SOUND.TRIG_PIN, HIGH);
    delay_us(10);
    write_pin(ULTRA_SOUND.TRIG_PORT, ULTRA_SOUND.TRIG_PIN, LOW);
}

void SysTick_Handler(void){

}
void EXTI0_IRQHandler(void){ 
    if(EXTI->PR & EXTI_PR_PR0){
        if(ULTRA_SOUND.ECHO_PORT->IDR & (1 << ULTRA_SOUND.ECHO_PIN)){
            rise_time = TIM2->CNT;
        }else{
            fall_time = TIM2->CNT;
            pulse_duration = (fall_time >= rise_time) ? (fall_time - rise_time) : (0xFFFFFFFF - rise_time + fall_time + 1);
            pulse_duration /= 16; // convert to microseconds
            pulse_width= pulse_duration; //to store echo pulse width
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

void servo_angle_set(int angle){
    pulse_width= 1500 - (500 * (angle/45.0));
    TIM8->CCR1= pulse_width;
}

void PWM_Output_PC6_Init(void){
    TIM8->PSC= (SYSTEM_FREQ / 1000000) - 1; // 1MHz timer
    TIM8->ARR= 19999;
    TIM8->CCMR1 &= ~(TIM_CCMR1_OC1M);
    TIM8->CCMR1 |= (6 << TIM_CCMR1_OC1M_Pos); // PWM mode 1
    TIM8->CCMR1 |= TIM_CCMR1_OC1PE; // Pre
    TIM8->CCER |= TIM_CCER_CC1E; // Enable output
    TIM8->CR1 |= TIM_CR1_CEN; // Enable timer
    TIM8->EGR = TIM_EGR_UG;  
    TIM8->CR1 |= TIM_CR1_CEN; 
}

int main(){
    SERVO_t ultrasound_servo= {
        .SERVO_PIN_PORT = GPIOC,
        .SERVO_PWM_PIN = 9,
        .SERVO_FEEDBACK_PIN = 0
    };
    init_servo(&ultrasound_servo);


    init_usart(115200);
    init_ssd(10);
    init_ultrasound();
    init_sys_tick(8000000); // 500ms period
    init_gp_timer(TIM2, 1000000, 0xFFFFFFFF, 1); // 1MHz timer for microsecond precision
    init_gp_timer(TIM8, 1000000, 0xFFFFFFFF, 0); // 1MHz timer for microsecond precision
    PWM_Output_PC6_Init();
    TIM8->BDTR |= TIM_BDTR_MOE; // Main output enable for TIM8

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

    servo_angle_set(0);
    for(volatile uint32_t i=0; i<1000000; i++);

    while(1){
        for(int angle= -45; angle<= 45; angle += 5){
            servo_angle_set(angle);

            for(volatile uint32_t i=0; i<1000000; i++); 
            trigger_pulse();
        

            if (new_data_ready) {
            // Safely read volatile variables
                float current_distance = distance;

                if(current_distance > 99.99){
                    display_num(9999, 2);
                }else{
                    display_num((uint16_t)(current_distance*100), 2);
                }
                char str[100];
                if(unit){
                    sprintf(str, "angle(deg): %d, pulsewidth(us): %lu, Dist: %.2fin\r\n", angle, pulse_width, current_distance);
                }else{
                    sprintf(str, "angle(deg): %d, pulsewidth(us): %lu, Dist: %.2fcm\r\n", angle, pulse_width, current_distance);
                }

                for(int i = 0; str[i] != '\0'; i++) {
                    send_char(USART2, str[i]);
                }
                new_data_ready = 0;
            }
        } 

        for(volatile uint32_t i=0; i<1000000; i++); 

        for(int angle= 45; angle>= -45; angle -= 5){ //for reverse sweeping
            servo_angle_set(angle);

            for(volatile uint32_t i=0; i<1000000; i++); 
            trigger_pulse();

            if(new_data_ready){
                float current_distance = distance;

                if(current_distance > 99.99){
                    display_num(9999, 2);
                }else{
                    display_num((uint16_t)(current_distance*100), 2);
                }

                char str[100];
                if(unit){

                    sprintf(str, "angle(deg): %d, pulsewidth(us): %lu, Dist: %.2fin\r\n", angle, pulse_width, current_distance);
                }else{
                    sprintf(str, "angle(deg): %d, pulsewidth(us): %lu, Dist: %.2fcm\r\n", angle, pulse_width, current_distance);
                }

                for(int i = 0; str[i] != '\0'; i++) {
                    send_char(USART2, str[i]);
                }
                new_data_ready = 0;
            }
            for(volatile uint32_t i=0; i<1000000; i++); //simple delay
        }
    }
    return 0;
}