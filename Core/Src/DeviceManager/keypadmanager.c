/*
 * keypadmanager.c
 *
 *  Created on: Jun 3, 2023
 *      Author: xuanthodo
 */


#include "DeviceManager/keypadmanager.h"
#include "Device/keypad.h"
#include "Lib/scheduler/scheduler.h"

// Keypad manager
static uint8_t keypad_buf[KEYPAD_BUF_SIZE];
static uint8_t keypad_buf_len = 0;

// Keypad status
static const counter_for_long_press = LONG_PRESSED_TIME / DEBOUNCE_TIME;
static uint16_t counter_for_entered_long = 0;
static uint16_t counter_for_cancelled_long = 0;
static uint16_t keypad_status_debounce = 0x0000;
static uint16_t keypad_prev_status = 0x0000;
static uint16_t keypad_status = 0x0000;
static bool is_entered = false;
static bool is_entered_long = false;
static bool is_cancelled = false;
static bool is_cancelled_long = false;
static bool timeout_for_debounce = true;


// Private function
static bool KEYPADMNG_is_curr_press(uint16_t keypad_status, uint8_t key);
static bool KEYPADMNG_is_pressed(uint16_t keypad_status, uint16_t keypad_prev_status, uint8_t key);
static bool KEYPADMNG_is_release(uint16_t keypad_status, uint16_t keypad_prev_status, uint8_t key);
static void KEYPADMNG_timeout_for_debounce();

void KEYPADMNG_init(){

}

void KEYPADMNG_run(){
	if(timeout_for_debounce){
		timeout_for_debounce = false;
		// Get keypad status
		keypad_status_debounce = KEYPAD_get_status();
		if(keypad_status == keypad_status_debounce){
			// Check number is pressed
			for (int key = KEY_0; key <= KEY_9; ++key) {
				if(KEYPADMNG_is_pressed(keypad_status, keypad_prev_status, key)){
					keypad_buf[keypad_buf_len] = key;
					keypad_buf_len = (keypad_buf_len + 1) % KEYPAD_BUF_SIZE;
					break;
				}
			}
			// Check entered
			is_entered = KEYPADMNG_is_pressed(keypad_status, keypad_prev_status, KEY_ENTER_OR_STAR);
			// For long pressed
			if(KEYPADMNG_is_curr_press(keypad_status, KEY_ENTER_OR_STAR)){
				counter_for_entered_long++;
			}else{
				counter_for_entered_long = 0;
			}
			if(counter_for_entered_long > counter_for_long_press){
				is_entered_long = true;
			}
			// Check cancelled
			is_cancelled = KEYPADMNG_is_pressed(keypad_status, keypad_prev_status, KEY_CANCEL_OR_SHAPH);
			if(KEYPADMNG_is_curr_press(keypad_status, KEY_CANCEL_OR_SHAPH)){
				counter_for_cancelled_long++;
			}else{
				counter_for_cancelled_long = 0;
			}
			if(counter_for_cancelled_long > counter_for_long_press){
				is_cancelled_long = true;
			}
			keypad_prev_status = keypad_status;
		}
		keypad_status = KEYPAD_get_status();
		SCH_Add_Task(KEYPADMNG_timeout_for_debounce, DEBOUNCE_TIME, 0);
	}
}

bool KEYPADMNG_is_entered(){
	return is_entered;
}

bool KEYPADMNG_is_entered_long(){
	return is_entered_long;
}

void KEYPADMNG_clear_entered(){
	is_entered = false;
	is_entered_long = false;
}

bool KEYPADMNG_is_cancelled(){
	return is_cancelled;
}

bool KEYPADMNG_is_cancelled_long(){
	return is_cancelled_long;
}

void KEYPADMNG_clear_cancelled(){
	is_cancelled = false;
	is_cancelled_long = false;
}

void KEYPADMNG_get_data(uint8_t * data, size_t * data_len){
	memcpy(data, keypad_buf, keypad_buf_len);
	* data_len = keypad_buf_len;
}

void KEYPADMNG_clear_data(){
	memset(keypad_buf, 0, sizeof(keypad_buf));
	keypad_buf_len = 0;
}

static bool KEYPADMNG_is_curr_press(uint16_t keypad_status, uint8_t key){
	return (keypad_status & (1 << key));
}

static bool KEYPADMNG_is_pressed(uint16_t keypad_status, uint16_t keypad_prev_status, uint8_t key){
	return (keypad_status & (1 << key)) && !(keypad_prev_status & (1 << key));
}

static bool KEYPADMNG_is_release(uint16_t keypad_status, uint16_t keypad_prev_status, uint8_t key){
	return !(keypad_status & (1 << key)) && (keypad_prev_status & (1 << key));
}

static void KEYPADMNG_timeout_for_debounce(){
	timeout_for_debounce = true;
}
