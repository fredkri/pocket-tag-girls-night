#include "debug.h"

#define PLAYER_NAME_LENGTH 17
#define HIT_SONG_LENGTH 17
#define IR_MSG_MAX_LENGTH  30

// ============ FUNCTION HEADERS =============
void flash_and_buzz();
void storage_save_id();
void storage_save_name();
void storage_save_hit_song();


void buzzer_sweep(uint16_t start_freq, uint16_t end_freq, uint16_t duration_ms);


// =========== GLOBAL VARIABLES ==============
volatile uint8_t player_id = 0;
volatile uint8_t player_name[PLAYER_NAME_LENGTH];
volatile uint8_t hit_song[HIT_SONG_LENGTH];
volatile uint16_t got_hit_counter = 0;
uint8_t ir_msg_tx_buf[IR_MSG_MAX_LENGTH];
uint8_t ir_msg_rx_buf[IR_MSG_MAX_LENGTH];

// high level RX decode
volatile uint8_t rx_decode_state = 1;

// millisecond timer
volatile uint32_t time_ms = 0;

// =========== IR message IDs ===============
#define IR_MSG_STARTBYTE 0x77
#define IR_MSG_PLAYER_ID_NAME 0x01
#define IR_MSG_HITSONG 0x02

// =========== IR states ===============
#define IR_WAIT_FOR_STARTBYTE 1
#define IR_GET_LENGTH 2
#define IR_GET_COMMAND 3
#define IR_GET_PAYLOAD 4
#define IR_GET_CHECKSUM 5

// =================================================================================
// =============================== GPIOS and HW CONFIG =============================
// =================================================================================

void init_hardware(){
    // === Configure pins ===
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    // A1 as Buzzer (Output Push-Pull)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // A2 as input from IR receiver
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // C1 as Trigger button with internal pull down
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // C2 as HIT LEDs
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // C4 as output to IR LED (Output Push-Pull)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // ==== TIM2 timer for timing the IR pulses when receiving ====
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_TimeBaseInitStructure.TIM_Prescaler = 24 - 1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
    TIM_Cmd(TIM2, ENABLE);

    // === TIM1 as a convinient system timer, in cs (centi-seconds) ===
     RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    TIM_TimeBaseInitStructure.TIM_Prescaler = 59997;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV4;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);
    TIM_Cmd(TIM1, ENABLE);
}


// =================== GPIO wrappers ================

// TRIGGER BUTTON on C1
uint16_t io_trigger_pressed(){
    return (GPIO_ReadInputData(GPIOC) & GPIO_Pin_1);
}

// HIT LEDs on C2
void io_hit_leds(uint16_t state){
    if(state){
        GPIO_SetBits(GPIOC, GPIO_Pin_2);
    }else{
        GPIO_ResetBits(GPIOC, GPIO_Pin_2);
    }
}



// BUZZER on A1
void io_buzzer(uint16_t state){
    if(state){
        GPIO_SetBits(GPIOA, GPIO_Pin_1);
    }else{
        GPIO_ResetBits(GPIOA, GPIO_Pin_1);
    }
}

// =================================================================================
// ================================ IR SHOOTING DATA ===============================
// =================================================================================
/* 
    Transmit 1s and 0s using the GAP LENGTH MODULATION
    Pulses are seen as LOW on the RX pin, and are always 12 long
    Gaps are seen as HIGH, and are 14 for a "zero" and 18 for a "one"
*/

// ============ IR TX ============

// - lowest level: send PULSES and GAPS of given length
void ir_pulse(uint16_t length){
    while(length){
        GPIO_SetBits(GPIOC, GPIO_Pin_4);
        Delay_Us(8);
        GPIO_ResetBits(GPIOC, GPIO_Pin_4);
        Delay_Us(8);
        length--;
    }
}

void ir_gap(uint16_t length){
    // Note: The pins are RESET and then RESET again, which does not change the state of the GPIO
    // but this way ir_gap() perfectly matches the timings of ir_pulse()
    while(length){
        GPIO_ResetBits(GPIOC, GPIO_Pin_4);
        Delay_Us(8);
        GPIO_ResetBits(GPIOC, GPIO_Pin_4);
        Delay_Us(8);
        length--;
    }
}

// - one level up: send ONES, ZEROS and some special contorl signals using the pulses and gaps
void ir_send_one(){
    ir_gap(28);
    ir_pulse(12);
}

void ir_send_zero(){
    ir_gap(14);
    ir_pulse(12);
}

void ir_send_preamble(){
    ir_pulse(228);
    ir_gap(228);
}

void ir_send_starting_pulse(){
    ir_pulse(12);
}

void ir_send_ending_pulse(){
    ir_pulse(12);
}

void ir_send_break_after_byte(){
    ir_gap(43);
}

// - two levels up: send a byte worth of data using the ONES, ZEROS and control signals
void send_byte(uint8_t byte){
    // start pulse
    ir_send_starting_pulse();
    
    // 8 bit payload
    for(int i=8; i; i--){
        if(byte & 0x80){
            ir_send_one();
        }else{
            ir_send_zero();
        }
        byte<<=1;
    }
    ir_send_break_after_byte();
}

// ========= IR RX ========

// lowest level: get the length of a gap
uint16_t get_gap(){
    // if already high, abort:
    if(GPIO_ReadInputData(GPIOA) & GPIO_Pin_2){return 0xFFFF;}

    // wait to go HIGH
    TIM2->CNT = 0;
    while(!(GPIO_ReadInputData(GPIOA) & GPIO_Pin_2)){if(TIM2->CNT > 10000){return 0xFFFF;}}
    TIM2->CNT = 0;
    // wait to go LOW again
    while(GPIO_ReadInputData(GPIOA) & GPIO_Pin_2){if(TIM2->CNT > 10000){return 0xFFFF;}}

    // return lenght of gap betweeen pulses
    return TIM2->CNT;
}

// top level: receive entire bytes
// recode_rx() returns "1" when a new byte is available (otherwise "0")
// the new byte is stored in "rx_data".
// NOTE: This is an ugly BLOCKING implementation
uint16_t rx_data;
uint16_t decode_rx(){
    uint16_t time;
    
    static uint16_t state = 1;
    static uint16_t data = 0;
    static uint16_t bits_received = 0;
   
    time = get_gap();

    // on timeout, reset state
    if(time == 0xFFFF){
        state = 1;
        rx_data = 0;
        return 0;
    }

    // scale because;
    time >>= 4;
    
    switch(state){        
        // preamble length. Not sure yet so assume its good
        case 1:
            if(time > 250 && time < 500){
                state = 2;
                bits_received = 0;
                data = 0;
            }
            break;
        
        // actual bits
        case 2:
            if(time > 60 || time < 10){
                // sorry! too long or too short! Reset to preamble
                state = 1;
                break;
            }
            
            if(time > 30 && time < 50){
                // looks like a 1!
                data |= 1;
            }

            // count the new bit, see if we have a byte
            bits_received += 1;
            if(bits_received >= 8){
                state = 3;
            }else{
                data <<= 1;
            }
            break;
        
        // end symbol
        case 3:
            if(time> 50 && time < 80){
                // end symbol valid! the data is valid!
                rx_data = data;
                data = 0;
                state = 2;
                bits_received = 0;
                return 1;
            }
            // either way, reset to preamble for the next byte
            state = 1;
            break;
    }
    return 0;
}

// ========= Buzzer ========

// beep with buzzer (also shitty implementation using delay)
void buzzer_tone(uint32_t freq, uint32_t duration_ms){

    if (freq == 0) { Delay_Ms(duration_ms); return; }

    uint16_t half_period_us = (uint16_t)(1000000UL / ((uint32_t)freq * 2));
    uint32_t cycles         = ((uint32_t)freq * duration_ms) / 1000;

    for (uint32_t i = 0; i < cycles; i++) {
        io_buzzer(1);
        Delay_Us(half_period_us);
        io_buzzer(0);
        Delay_Us(half_period_us);
    }
}

void buzzer_sweep(uint16_t start_freq, uint16_t end_freq, uint16_t duration_ms){
    uint16_t step_ms = 5;
    uint16_t steps   = duration_ms / step_ms;
    if (steps == 0) steps = 1;

    for (uint16_t i = 0; i < steps; i++) {
        int32_t freq = (int32_t)start_freq +
                       ((int32_t)((int16_t)(end_freq - start_freq)) * (int32_t)i) / steps;

        uint16_t half_period_us = (uint16_t)(1000000UL / ((uint32_t)freq * 2));
        uint32_t cycles         = ((uint32_t)freq * step_ms) / 1000;

        for (uint32_t j = 0; j < cycles; j++) {
            io_buzzer(1);
            Delay_Us(half_period_us);
            io_buzzer(0);
            Delay_Us(half_period_us);
        }
    }

}

void buzzer_shoot(){
    buzzer_sweep(2500, 200, 120);
}

void buzzer_no_ammo(){
    buzzer_sweep(400, 200, 50);
}

void buzzer_hit(){
    buzzer_sweep(800, 400, 100);
    io_hit_leds(1);
    Delay_Ms(30);
    io_hit_leds(0);

    buzzer_sweep(600, 150, 100);
    io_hit_leds(1);
    Delay_Ms(30);
    io_hit_leds(0);

    buzzer_sweep(350, 60, 100);
    io_hit_leds(1);
    Delay_Ms(30);
    io_hit_leds(0);
  } 

void buzzer_reload(){
    buzzer_sweep(350, 150, 50);
    Delay_Ms(30);
    buzzer_sweep(300, 500, 50);
}

// flash an LED (you guessed it! shitty implementation using delay! Not that it matters here)
void flash_and_buzz(){
    buzzer_hit();

    // blink one more time    
    io_hit_leds(1);
    Delay_Ms(100); 
    io_hit_leds(0);
}

// ============== STORAGE OF DATA IN FLASH MEMORY ===============
// Last 4KB are reserved for the user data, starting from 

#define STORAGE_ADDRESS_ID 0x08003FC0
#define STORAGE_ADDRESS_PLAYER_NAME 0x08003F80
#define STORAGE_ADDRESS_HIT_SONG 0x08003F40

void storage_save_id(){
    FLASH_Unlock_Fast();
    FLASH_ErasePage_Fast(STORAGE_ADDRESS_ID);
    FLASH_ProgramHalfWord(STORAGE_ADDRESS_ID, player_id);
    FLASH_Lock_Fast();
}

void storage_save_name(){
    FLASH_Unlock_Fast();
    FLASH_ErasePage_Fast(STORAGE_ADDRESS_PLAYER_NAME);

    uint16_t two_letters;
    for(uint16_t i=0; i<16; i+=2){
        // combine two letters into a single 16bit half word (smallest allowed write-to-flash size)
        two_letters = player_name[i+1];
        two_letters <<= 8;
        two_letters |= player_name[i];
        FLASH_ProgramHalfWord(STORAGE_ADDRESS_PLAYER_NAME + i, two_letters);
    }
    FLASH_Lock_Fast();
}

void storage_save_hit_song(){
    FLASH_Unlock_Fast();
    FLASH_ErasePage_Fast(STORAGE_ADDRESS_HIT_SONG);

    uint16_t two_notes;
    for(uint16_t i=0; i<16; i+=2){
        // combine two letters into a single 16bit half word (smallest allowed write-to-flash size)
        two_notes = hit_song[i+1];
        two_notes <<= 8;
        two_notes |= hit_song[i];
        FLASH_ProgramHalfWord(STORAGE_ADDRESS_HIT_SONG + i, two_notes);
    }
    FLASH_Lock_Fast();
}

void storage_load(){
    // Load the ID:
    player_id = *((uint8_t *)(STORAGE_ADDRESS_ID));
    // sanity check:
     if(player_id == 0xFF){player_id = 0x00;}

    // Load the player name:
    for(uint16_t i=0; i<16; i+=2){
        player_name[i] = *((uint8_t *)(STORAGE_ADDRESS_PLAYER_NAME + i));
        player_name[i+1] = *((uint8_t *)(STORAGE_ADDRESS_PLAYER_NAME + i+1));
    }
    // sanity check
    if(player_name[0] == 0xFF){player_name[0] = 0x00;}

    // Load the "Hit Song":
    for(uint16_t i=0; i<16; i+=2){
        hit_song[i] = *((uint8_t *)(STORAGE_ADDRESS_HIT_SONG + i));
        hit_song[i+1] = *((uint8_t *)(STORAGE_ADDRESS_HIT_SONG + i+1));
    }

     // sanity check
    if(hit_song[0] == 0xFF){hit_song[0] = 0x00;}
}

// xxxxxxxxxxxxxxxx END OF HARDWARE ABSTRACTION xxxxxxxxxxxxxxxxxxx


// ==================== TOP LEVEL GAME LOGIC ==================

// IR COMMUNICATION 
void process_rx_data(uint16_t rx){
    static uint8_t length = 0;
    static uint8_t command = 0;
    static uint8_t buffer_pointer = 0;
    static uint16_t last_byte_timestamp = 0;

    // check if the local buffer is old and stale
    // meaning that the fresh new byte is actually the start of a new message
    if(TIM1->CNT - last_byte_timestamp > 50){
        rx_decode_state = IR_WAIT_FOR_STARTBYTE;
    }
    last_byte_timestamp = TIM1->CNT;

    switch(rx_decode_state){
        case IR_WAIT_FOR_STARTBYTE:{
            if (rx == IR_MSG_STARTBYTE){
                length = 0;
                rx_decode_state = IR_GET_LENGTH;
            }
            break;
        }
        case IR_GET_LENGTH:{
            length = rx;
            if (length > IR_MSG_MAX_LENGTH){
                rx_decode_state = IR_WAIT_FOR_STARTBYTE;
            } else {
                rx_decode_state = IR_GET_COMMAND;
            }
            break;
        }
        case IR_GET_COMMAND:{
            command = rx;
            // add safety checks
            rx_decode_state = IR_GET_PAYLOAD;
            buffer_pointer = 0;
            break;
        }
        case IR_GET_PAYLOAD:{
            ir_msg_rx_buf[buffer_pointer++] = rx;
            length--;
            if (length == 0) {
                rx_decode_state = IR_GET_CHECKSUM;
            }
            break;
        }
        case IR_GET_CHECKSUM:{
            if (command == IR_MSG_PLAYER_ID_NAME){
                flash_and_buzz();
            }
            rx_decode_state = IR_WAIT_FOR_STARTBYTE;
            break;
        }
    }
}

void ir_send_message(uint8_t *buf, uint8_t len){    
    uint8_t cs = len;
    ir_send_preamble();
    send_byte(IR_MSG_STARTBYTE);
    send_byte(len-1);
    for (uint8_t i = 0; i < len; i++) {
        cs += buf[i];
        send_byte(buf[i]);
    }
    send_byte(cs);
    ir_send_ending_pulse();
}

void ir_send_id_name(){
    uint8_t len = 0;    
    ir_msg_tx_buf[len++] = IR_MSG_PLAYER_ID_NAME;
    ir_msg_tx_buf[len++] = (uint8_t)player_id;
    for (uint16_t i = 0; i < PLAYER_NAME_LENGTH; i++) {
        if (player_name[i] == '\0') break;
        ir_msg_tx_buf[len++] = (uint8_t)player_name[i];
    }

    ir_send_message(ir_msg_tx_buf, len);
}

void ir_send_hitsong(){
    uint8_t len = 0;
    ir_msg_tx_buf[len++] = IR_MSG_HITSONG;
    for(uint16_t i=0; i<HIT_SONG_LENGTH; i++){
        if(hit_song[i] == '\0'){break;}
        ir_msg_tx_buf[len++] = (uint8_t)(hit_song[i]);
    }
    ir_send_message(ir_msg_tx_buf, len);
}

// =========================================================
// ========================== MAIN =========================
// =========================================================
int main(void){
    // Init stuff
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    init_hardware();
    // Load configuration and game state from flash (player name, "hit song", etc)
    storage_load();

    // cool boot sequence - also acts as a simple power on self test
    // boot beep
    buzzer_tone(400, 50);
    Delay_Ms(50);
    buzzer_tone(800, 25);
    // boot blinks
    io_hit_leds(1); Delay_Ms(100); io_hit_leds(0); Delay_Ms(100);
    io_hit_leds(1); Delay_Ms(100); io_hit_leds(0); Delay_Ms(100);
    // boot gun blink
    ir_send_preamble();

    // local variables for main game loop
    uint32_t time_last_shot = 0;
    uint32_t time_last_trigger = 0;
    uint8_t trigger_released = 1;
    uint8_t ammo = 6;
  
    // =========================================================
    // ======================= MAIN LOOP =======================
    // =========================================================
    while(1){

        // ugly timer fix later
        // approx 10-100 counts per 1 ms
        time_ms += 1;
        
        // Check if trigger button pressed, and if so, SHOOT
        if(io_trigger_pressed()){
            if(time_ms > time_last_trigger + 1000 && trigger_released==1){
                time_last_trigger = time_ms; // 10-100 is a ms ishf
                trigger_released = 0;
                if(ammo > 0){
                    ir_send_id_name();
                    buzzer_shoot();
                    ir_send_hitsong();
                    time_last_shot = time_ms;
                    ammo --;
                }else{
                    buzzer_no_ammo();
                } 
            }          
        }else{
            trigger_released = 1;
        }

        //check for reload
        if(time_ms > time_last_shot + 300000 && ammo<6){
            ammo = 6;
            buzzer_reload();
        }

        // check if we were shot - read the incoming IR RECEIVER data
        if (decode_rx()){
            // if there is a new byte, pass the new byte into processing
            process_rx_data(rx_data);
        }
    }
}