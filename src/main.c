#include "hat.h"
uint16_t pulse_duration = 0;
void TIM3_IRQHandler(void){
    if(TIM3->SR & TIM_SR_UIF){
        TIM3->SR &= ~TIM_SR_UIF;
        // Send 10us pulse on TRIG pin
        toggle_pin(ULTRA_SOUND.TRIG_PORT, ULTRA_SOUND.TRIG_PIN);
        uint16_t current_time = TIM3->CNT;
        while(TIM3->CNT - current_time < 10);
        toggle_pin(ULTRA_SOUND.TRIG_PORT, ULTRA_SOUND.TRIG_PIN);

        // Wait for ECHO pin to go high
        while(!read_pin(ULTRA_SOUND.ECHO_PORT, ULTRA_SOUND.ECHO_PIN));
        uint16_t start_time = TIM3->CNT;
        // Wait for ECHO pin to go low
        while(read_pin(ULTRA_SOUND.ECHO_PORT, ULTRA_SOUND.ECHO_PIN));
        pulse_duration = TIM3->CNT - start_time;
        display_num(pulse_duration / 58, 2); // Convert to cm and display
    }
}

int main() {
    init_ssd(500);
    display_num(0, 2);
    init_ultrasound();
    init_gp_timer(TIM3, 1000000, 4000);
    init_timer_IRQ(TIM3, 1);
    while(1){};
    return 0;
}