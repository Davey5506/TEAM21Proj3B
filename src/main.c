#include "hat.h"
uint16_t pulse_duration = 0;
uint16_t rise_time = 0;
uint16_t fall_time = 0;
uint8_t is_first_capture = 1;

void TIM3_IRQHandler(void){
    if(TIM3->SR & TIM_SR_CC1IF){
        TIM3->SR &= ~TIM_SR_CC1IF;
        if(is_first_capture){
            rise_time = TIM3->CCR1;
            is_first_capture = !is_first_capture;
        }else{
            fall_time = TIM3->CCR1;
            if(fall_time > rise_time){
                pulse_duration = fall_time - rise_time;
            }else{
                pulse_duration = (0xFFFFU - rise_time) + fall_time;
            }
            is_first_capture = !is_first_capture;
            display_num(pulse_duration / 58, 2); // Convert to cm and display
            char str[5];
            int_to_string(pulse_duration / 58, str, 5); 
            int i = 0;
            while(str[i] != '\0'){
                send_char(USART2, str[i]);
                i++;
            }
        }
    }
}

int main() {
    init_usart(9600);
    init_ssd(10);
    display_num(0, 2);
    init_ultrasound();
    
    init_gp_timer(TIM3, 1000000, 9999, 0); // 1 MHz, max count 9999 (10ms period), do not start yet
    
    // Configure TIM3 Channel 1 for Input Capture
    TIM3->CCMR2 &= ~TIM_CCMR2_CC3S;   // Clear CC1S bits
    TIM3->CCMR2 |= TIM_CCMR2_CC3S_0;  // CC1S = 01: CC1 channel is configured as input, IC1 is mapped on TI1
    TIM3->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP); // Clear polarity bits
    TIM3->CCER |= TIM_CCER_CC3P | TIM_CCER_CC3NP;    // Capture on both rising and falling edges
    TIM3->CCER |= TIM_CCER_CC3E;      // Enable capture on channel 1
    TIM3->DIER |= TIM_DIER_CC3IE; // Enable Capture/Compare 1 interrupt

    // Configure TIM3 Channel 2 for PWM Output (Trigger Pulse)
    TIM3->CCMR1 &= ~TIM_CCMR1_CC2S;   // CC2S = 00: CC2 channel is configured as output
    TIM3->CCMR1 |= (6 << TIM_CCMR1_OC2M_Pos); // OC2M = 110: PWM mode 1
    TIM3->CCMR1 |= TIM_CCMR1_OC2PE; // Enable preload for CCR2
    TIM3->CCR2 = 15; // 10us pulse width (10 counts at 1MHz)
    TIM3->CCER |= TIM_CCER_CC2E; // Enable output on channel 2

    // Enable TIM3 interrupt in NVIC
    //init_timer_IRQ(TIM3, 2); // Set a priority for TIM3 interrupts

    // Start the timer
    //TIM3->CR1 |= TIM_CR1_CEN;

    while(1){};
    return 0;
}