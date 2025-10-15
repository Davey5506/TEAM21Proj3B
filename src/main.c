#include "hat.h"
uint16_t pulse_duration = 0;
void TIM3_IRQHandler(void){
}

int main() {
    init_usart(USART2, 9600);
    init_ssd(10);
    display_num(0, 2);
    init_ultrasound();
    init_gp_timer(TIM3, 1000000, 4000);
    init_timer_IRQ(TIM3, 1);
    while(1){};
    return 0;
}