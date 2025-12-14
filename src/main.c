#include <stdint.h>
#include "xparameters.h"
#include "xuartps_hw.h"
#include "xil_types.h"
#include "sleep.h" // usleep()
#include "xiic.h"
#include "xiic_l.h"

/******************************************************************
 * AXI BRAM: character storage
 * BRAM[0] = display_char (for LCD)
 * BRAM[1] = braille_char (for servos)
 ******************************************************************/
#define BRAM_BASE_ADDR XPAR_AXI_BRAM_CTRL_0_BASEADDR

/******************************************************************
 * AXI GPIO 0: LEDs (channel 1) + Switches (channel 2)
 ******************************************************************/
#define GPIO_LED_BASE_ADDR XPAR_AXI_GPIO_0_BASEADDR

// Channel 1: LEDs
#define GPIO_LED_DATA (*(volatile u32*)(GPIO_LED_BASE_ADDR + 0x0))
#define GPIO_LED_TRI (*(volatile u32*)(GPIO_LED_BASE_ADDR + 0x4))

// Channel 2: Switches
#define GPIO_SW_DATA (*(volatile u32*)(GPIO_LED_BASE_ADDR + 0x8))
#define GPIO_SW_TRI (*(volatile u32*)(GPIO_LED_BASE_ADDR + 0xC))

/******************************************************************
 * AXI GPIO 1: Buttons (channel 1)
 ******************************************************************/
#define GPIO_BTN_BASE_ADDR XPAR_AXI_GPIO_1_BASEADDR
#define GPIO_BTN_DATA (*(volatile u32*)(GPIO_BTN_BASE_ADDR + 0x0))
#define GPIO_BTN_TRI (*(volatile u32*)(GPIO_BTN_BASE_ADDR + 0x4))

/******************************************************************
 * AXI GPIO 2: Keypad (dual channel)
 * - Channel 1: rows (outputs)
 * - Channel 2: columns (inputs)
 ******************************************************************/
#define GPIO_KP_BASE_ADDR XPAR_AXI_GPIO_2_BASEADDR

#define KP_ROWS_DATA (*(volatile u32*)(GPIO_KP_BASE_ADDR + 0x0)) // ch1 data
#define KP_ROWS_TRI (*(volatile u32*)(GPIO_KP_BASE_ADDR + 0x4)) // ch1 tri
#define KP_COLS_DATA (*(volatile u32*)(GPIO_KP_BASE_ADDR + 0x8)) // ch2 data
#define KP_COLS_TRI (*(volatile u32*)(GPIO_KP_BASE_ADDR + 0xC)) // ch2 tri

/******************************************************************
 * AXI GPIO 3: Servos (Braille cell)
 * - Channel 2 (DATA @ base+0x8): servos 1..3 (JC1,JC2,JC3)
 * - Channel 1 (DATA @ base+0x0): servos 4..6 (JC7,JC8,JC9)
 ******************************************************************/
#define GPIO3_BASEADDR XPAR_AXI_GPIO_3_BASEADDR

// Channel 1
#define GPIO3_CH1_DATA (*(volatile uint32_t*)(GPIO3_BASEADDR + 0x0))
#define GPIO3_CH1_TRI (*(volatile uint32_t*)(GPIO3_BASEADDR + 0x4))

// Channel 2
#define GPIO3_CH2_DATA (*(volatile uint32_t*)(GPIO3_BASEADDR + 0x8))
#define GPIO3_CH2_TRI (*(volatile uint32_t*)(GPIO3_BASEADDR + 0xC))

// Bits for six servos
// Channel 2: servos 1..3
#define SERVO1_BIT 0u
#define SERVO1_MASK (1u << SERVO1_BIT)

#define SERVO2_BIT 1u
#define SERVO2_MASK (1u << SERVO2_BIT)

#define SERVO3_BIT 2u
#define SERVO3_MASK (1u << SERVO3_BIT)

// Channel 1: servos 4..6
#define SERVO4_BIT 0u
#define SERVO4_MASK (1u << SERVO4_BIT)

#define SERVO5_BIT 1u
#define SERVO5_MASK (1u << SERVO5_BIT)

#define SERVO6_BIT 2u
#define SERVO6_MASK (1u << SERVO6_BIT)

/******************************************************************
 * UART base addresses
 * - UART1: USB-UART (PC / terminal)
 * - UART0: unused here
 ******************************************************************/
#define UART1_BASE_ADDR XPAR_XUARTPS_1_BASEADDR
#define UART0_BASE_ADDR XPAR_XUARTPS_0_BASEADDR // unused

/******************************************************************
 * Servo timing constants
 ******************************************************************/
#define PWM_PERIOD_US 20000u // 20 ms -> 50 Hz

// Tunable for SG90
#define SERVO_MIN_US 600u // pulse for ≈ 0°
#define SERVO_MAX_US 2400u // pulse for ≈ 180°

/******************************************************************
 * Braille mapping (a..z)
 * Order of dots: 1 4
 * 2 5
 * 3 6
 ******************************************************************/
static const uint8_t braille_letters[26][6] = {
 {1,0,0,0,0,0}, // a
 {1,1,0,0,0,0}, // b
 {1,0,0,1,0,0}, // c
 {1,0,0,1,1,0}, // d
 {1,0,0,0,1,0}, // e
 {1,1,0,1,0,0}, // f
 {1,1,0,1,1,0}, // g
 {1,1,0,0,1,0}, // h
 {0,1,0,1,0,0}, // i
 {0,1,0,1,1,0}, // j
 {1,0,1,0,0,0}, // k
 {1,1,1,0,0,0}, // l
 {1,0,1,1,0,0}, // m
 {1,0,1,1,1,0}, // n
 {1,0,1,0,1,0}, // o
 {1,1,1,1,0,0}, // p
 {1,1,1,1,1,0}, // q
 {1,1,1,0,1,0}, // r
 {0,1,1,1,0,0}, // s
 {0,1,1,1,1,0}, // t
 {1,0,1,0,0,1}, // u
 {1,1,1,0,0,1}, // v
 {0,1,0,1,1,1}, // w
 {1,0,1,1,0,1}, // x
 {1,0,1,1,1,1}, // y
 {1,0,1,0,1,1} // z
};

/******************************************************************
 * Keypad map (4x4) – physical symbols
 ******************************************************************/
static const char KeypadMap[4][4] = {
 { '1', '2', '3', 'U' },
 { '4', '5', '6', 'D' },
 { '7', '8', '9', 'E' },
 { 'L', '0', 'R', '\r' }
};

/******************************************************************
 * Helper: convert angle (0..180) to pulse width in microseconds
 ******************************************************************/
static uint32_t angle_to_pulse_us(int angle_deg)
{
 if (angle_deg < 0) angle_deg = 0;
 if (angle_deg > 180) angle_deg = 180;

 uint32_t span = SERVO_MAX_US - SERVO_MIN_US;
 return SERVO_MIN_US + (span * (uint32_t)angle_deg) / 180u;
}

/******************************************************************
 * Struct describing one servo pulse inside a 20 ms frame
 ******************************************************************/
typedef struct {
 volatile uint32_t *data_reg; // pointer to DATA register (CH1 or CH2)
 uint32_t mask; // bit for this servo
 uint32_t high_us; // pulse width
} ServoEvent;

/******************************************************************
 * Generate one 20 ms PWM period for SIX servos
 ******************************************************************/
static void six_servo_period(ServoEvent events[6])
{
 // 1) all outputs HIGH at t = 0
 for (int i = 0; i < 6; i++) {
 *(events[i].data_reg) |= events[i].mask;
 }

 // 2) sort by pulse length (ascending)
 for (int i = 0; i < 5; i++) {
 for (int j = i + 1; j < 6; j++) {
 if (events[j].high_us < events[i].high_us) {
 ServoEvent tmp = events[i];
 events[i] = events[j];
 events[j] = tmp;
 }
 }
 }

 // 3) turn off each pulse at its target time
 uint32_t prev_time = 0;
 for (int i = 0; i < 6; i++) {
 uint32_t target_time = events[i].high_us;
 uint32_t delta = target_time - prev_time;
 usleep(delta);
 prev_time = target_time;
 *(events[i].data_reg) &= ~(events[i].mask);
 }

 // 4) wait remaining time to complete 20 ms frame
 if (prev_time < PWM_PERIOD_US) {
 usleep(PWM_PERIOD_US - prev_time);
 }
}

/******************************************************************
 * Convert ASCII letter -> 6-bit Braille pattern
 ******************************************************************/
static uint8_t ascii_to_braille(char c)
{
 // accept both lowercase and uppercase
 if (c >= 'A' && c <= 'Z') {
 c = (char)(c - 'A' + 'a');
 }

 if (c < 'a' || c > 'z') {
 return 0x00; // unsupported character: all dots down
 }

 int idx = (int)(c - 'a'); // 0..25

 uint8_t pattern = 0;
 for (int i = 0; i < 6; i++) {
 if (braille_letters[idx][i]) {
 pattern |= (1u << i); // dot i+1 -> bit i
 }
 }
 return pattern;
}

/******************************************************************
 * Apply a Braille pattern to servos for ONE 20 ms period
 ******************************************************************/
static void braille_servos_apply(uint8_t pattern)
{
 ServoEvent ev[6];

 // Servos 1..3 on channel 2
 int angle1 = (pattern & (1u << 0)) ? 90 : 0;
 int angle2 = (pattern & (1u << 1)) ? 90 : 0;
 int angle3 = (pattern & (1u << 2)) ? 90 : 0;

 ev[0].data_reg = &GPIO3_CH2_DATA; ev[0].mask = SERVO1_MASK; ev[0].high_us = angle_to_pulse_us(angle1);
 ev[1].data_reg = &GPIO3_CH2_DATA; ev[1].mask = SERVO2_MASK; ev[1].high_us = angle_to_pulse_us(angle2);
 ev[2].data_reg = &GPIO3_CH2_DATA; ev[2].mask = SERVO3_MASK; ev[2].high_us = angle_to_pulse_us(angle3);

 // Servos 4..6 on channel 1
 int angle4 = (pattern & (1u << 3)) ? 90 : 0;
 int angle5 = (pattern & (1u << 4)) ? 90 : 0;
 int angle6 = (pattern & (1u << 5)) ? 90 : 0;

 ev[3].data_reg = &GPIO3_CH1_DATA; ev[3].mask = SERVO4_MASK; ev[3].high_us = angle_to_pulse_us(angle4);
 ev[4].data_reg = &GPIO3_CH1_DATA; ev[4].mask = SERVO5_MASK; ev[4].high_us = angle_to_pulse_us(angle5);
 ev[5].data_reg = &GPIO3_CH1_DATA; ev[5].mask = SERVO6_MASK; ev[5].high_us = angle_to_pulse_us(angle6);

 six_servo_period(ev);
}

/******************************************************************
 * AXI IIC 0 base and LCD I2C address
 ******************************************************************/
#define IIC0_BASE_ADDR XPAR_XIIC_0_BASEADDR
#define LCD_I2C_ADDR 0x27

// PCF8574-to-LCD bit mapping:
#define LCD_RS 0x01u
#define LCD_RW 0x02u
#define LCD_EN 0x04u
#define LCD_BL 0x08u

/******************************************************************
 * Low-level I2C helper
 ******************************************************************/
static void IicWriteByte(uint8_t data)
{
 XIic_Send(IIC0_BASE_ADDR, LCD_I2C_ADDR, &data, 1, XIIC_STOP);
 usleep(50);
}

/******************************************************************
 * LCD low-level helpers (4-bit mode via PCF8574)
 ******************************************************************/
static void LcdPulseEnable(uint8_t data)
{
 IicWriteByte(data | LCD_EN | LCD_BL);
 IicWriteByte((uint8_t)((data & (uint8_t)~LCD_EN) | LCD_BL));
}

static void LcdWrite4Bits(uint8_t nibble, uint8_t rs)
{
 uint8_t data = (nibble & 0xF0);
 if (rs) {
 data |= LCD_RS;
 }
 data |= LCD_BL;
 LcdPulseEnable(data);
}

static void LcdSendByte(uint8_t value, uint8_t rs)
{
 uint8_t high = value & 0xF0;
 uint8_t low = (uint8_t)((value << 4) & 0xF0);

 LcdWrite4Bits(high, rs);
 LcdWrite4Bits(low, rs);
}

static void LcdCommand(uint8_t cmd)
{
 LcdSendByte(cmd, 0);
 usleep(2000);
}

static void LcdWriteChar(char c)
{
 LcdSendByte((uint8_t)c, 1);
}

/******************************************************************
 * LCD init sequence
 ******************************************************************/
static void LcdInit(void)
{
 XIic_DynInit(IIC0_BASE_ADDR);

 usleep(50000);

 LcdWrite4Bits(0x30, 0);
 usleep(4500);
 LcdWrite4Bits(0x30, 0);
 usleep(4500);
 LcdWrite4Bits(0x30, 0);
 usleep(150);

 LcdWrite4Bits(0x20, 0);
 usleep(150);

 LcdCommand(0x28);
 LcdCommand(0x08);
 LcdCommand(0x01);
 usleep(2000);
 LcdCommand(0x06);
 LcdCommand(0x0C);
}

/******************************************************************
 * Small delay for keypad scanning (crude debounce)
 ******************************************************************/
static void small_delay(void)
{
 volatile int i;
 for (i = 0; i < 2000; ++i) {
 /* simple NOP loop */
 }
}

/******************************************************************
 * Scan keypad once; return 0 if no key, or ASCII code
 ******************************************************************/
static char keypad_get_char(void)
{
 u32 row, col;

 KP_ROWS_DATA = 0x0F; // idle

 for (row = 0; row < 4; ++row) {

 KP_ROWS_DATA = (~(1U << row)) & 0x0F; // one row low, others high
 small_delay();

 u32 cols = KP_COLS_DATA & 0x0F;

 if (cols != 0x0F) {
 for (col = 0; col < 4; ++col) {
 if (!(cols & (1U << col))) {
 KP_ROWS_DATA = 0x0F;
 return KeypadMap[row][col];
 }
 }
 }
 }

 KP_ROWS_DATA = 0x0F;
 return 0;
}

/******************************************************************
 * Map keypad symbol -> Braille character
 *
 * Example mapping:
 * '1' -> 'a'
 * '2' -> 'b'
 * '3' -> 'c'
 * ...
 * '9' -> 'i'
 * '0' -> 'j'
 *
 * So:
 * - LCD shows the digit pressed: '1'
 * - Servos show Braille for the corresponding letter: 'a'
 ******************************************************************/
static char keypad_to_braille_char(char key)
{
 switch (key) {
 case '1': return 'a';
 case '2': return 'b';
 case '3': return 'c';
 case '4': return 'd';
 case '5': return 'e';
 case '6': return 'f';
 case '7': return 'g';
 case '8': return 'h';
 case '9': return 'i';
 case '0': return 'j';
 default:
 // For U, D, E, L, R, Enter, etc., use the same symbol
 return key;
 }
}

/******************************************************************
 * MAIN
 ******************************************************************/
int main(void)
{
 /***** 1) Configure LEDs as outputs *****/
 GPIO_LED_TRI = 0x00000000U;
 GPIO_LED_DATA = 0x00000000U;

 /***** 2) Configure switches as inputs *****/
 GPIO_SW_TRI = 0x0000000FU;

 /***** 3) Configure buttons as inputs *****/
 GPIO_BTN_TRI = 0x0000000FU;

 /***** 4) Configure servo GPIOs as outputs *****/
 GPIO3_CH2_TRI &= ~(SERVO1_MASK | SERVO2_MASK | SERVO3_MASK);
 GPIO3_CH1_TRI &= ~(SERVO4_MASK | SERVO5_MASK | SERVO6_MASK);

 GPIO3_CH2_DATA &= ~(SERVO1_MASK | SERVO2_MASK | SERVO3_MASK);
 GPIO3_CH1_DATA &= ~(SERVO4_MASK | SERVO5_MASK | SERVO6_MASK);

 /***** 5) Configure keypad GPIO *****/
 KP_ROWS_TRI = 0x00000000U;
 KP_COLS_TRI = 0x0000000FU;
 KP_ROWS_DATA = 0x0000000FU;

 /***** 6) BRAM pointer and initial values *****/
 volatile u8 *bram_ptr = (volatile u8 *)BRAM_BASE_ADDR;
 bram_ptr[0] = 0x00; // display_char
 bram_ptr[1] = 0x00; // braille_char

 /***** 7) Servo pattern *****/
 uint8_t current_pattern = 0x00;

 /***** 8) Button state for edge detection *****/
 u32 prev_btn_state = GPIO_BTN_DATA;

 /***** 9) Keypad last key for change detection *****/
 char last_key = 0;

 /***** 10) Initialise LCD *****/
 LcdInit();
 usleep(500000u);

 /***** 11) Main loop *****/
 while (1) {

 /* (A) mirror switches on LEDs */
 u32 sw_state = GPIO_SW_DATA & 0x0000000FU;
 u32 led_data = GPIO_LED_DATA & ~0x0000000FU;
 led_data |= sw_state;
 GPIO_LED_DATA = led_data;

 u8 input_enabled = (sw_state & 0x1U) ? 1u : 0u;
 u8 use_keypad = (sw_state & 0x2U) ? 1u : 0u;

 /* (B) drive servos with current Braille pattern */
 braille_servos_apply(current_pattern);

 /* (C) if switch 0 is DOWN, skip input + buttons */
 if (!input_enabled) {
 prev_btn_state = GPIO_BTN_DATA;
 last_key = 0;
 continue;
 }

 /* (D) update BRAM[0] (display) and BRAM[1] (braille) */
 if (!use_keypad) {
 /* PC keyboard mode */
 last_key = 0;

 if (XUartPs_IsReceiveData(UART1_BASE_ADDR)) {
 u8 received_char = XUartPs_RecvByte(UART1_BASE_ADDR);

 if (received_char != '\r' && received_char != '\n') {
 bram_ptr[0] = received_char; // display_char
 bram_ptr[1] = received_char; // braille_char
 }
 }
 } else {
 /* Keypad mode */
 char key = keypad_get_char();

 if (key != 0 && key != last_key) {
 char display_char = key;
 char braille_char = keypad_to_braille_char(key);

 bram_ptr[0] = (u8)display_char;
 bram_ptr[1] = (u8)braille_char;
 }

 last_key = key;
 }

 /* (E) buttons */
 u32 btn_state = GPIO_BTN_DATA;

 // Button 0: LCD + servos
 if ((btn_state & 0x1U) && !(prev_btn_state & 0x1U)) {
 if (bram_ptr[0] != 0x00) {
 char c_disp = (char)bram_ptr[0];
 char c_braille = (char)bram_ptr[1];

 current_pattern = ascii_to_braille(c_braille);
 LcdWriteChar(c_disp);
 }
 }

 // Button 1: clear LCD
 if ((btn_state & 0x2U) && !(prev_btn_state & 0x2U)) {
 LcdCommand(0x01);
 usleep(2000);
 }

 prev_btn_state = btn_state;
 }

 // not reached
 // return 0;
}