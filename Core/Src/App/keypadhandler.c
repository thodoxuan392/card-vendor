/*
 * keypadhandler.c
 *
 *  Created on: Jun 9, 2023
 *      Author: xuanthodo
 */


#include "main.h"
#include "config.h"
#include "App/keypadhandler.h"
#include "DeviceManager/lcdmanager.h"
#include "Device/rtc.h"
#include "Lib/utils/utils_string.h"
#include "Lib/utils/utils_logger.h"

enum {
	FN_TIME = 1,
	FN_CARD_PRICE,
	FN_PASSWORD,
	FN_TOTAL_CARD,
	FN_DELETE_TOTAL_CARD,
	FN_TOTAL_AMOUNT,
	FN_DELETE_TOTAL_AMOUNT,
	FN_TOTAL_CARD_BY_DAY,
	FN_TOTAL_CARD_BY_MONTH
};

enum {
	KEYPADHANDLER_STATE_NOT_IN_SETTING,
	KEYPADHANDLER_STATE_PASSWORD,
	KEYPADHANDLER_STATE_IN_SETTING,
	KEYPADHANDLER_STATE_IN_SETTING_DATA
};

enum {
	KEYPADHANDLER_PASSWD_NOT_PRESSED,
	KEYPADHANDLER_PASSWD_PRESSED_BUT_NOT_ENTERED,
	KEYPADHANDLER_PASSWD_ENTERED
};

enum {
	KEYPADHANDLER_SETTING_DATA_NOT_PRESSED,
	KEYPADHANDLER_SETTING_DATA_PRESSED_BUT_NOT_ENTERED,
	KEYPADHANDLER_SETTING_DATA_ENTERED,
	KEYPADHANDLER_SETTING_DATA_CONFIRMED,
	KEYPADHANDLER_SETTING_DATA_CANCELLED
};


static uint8_t prev_state = KEYPADHANDLER_STATE_NOT_IN_SETTING;
static uint8_t state = KEYPADHANDLER_STATE_NOT_IN_SETTING;
static uint8_t fn = FN_CARD_PRICE;
static char * state_name[] = {
		[KEYPADHANDLER_STATE_NOT_IN_SETTING] = "KEYPADHANDLER_STATE_NOT_IN_SETTING\r\n",
		[KEYPADHANDLER_STATE_PASSWORD] = "KEYPADHANDLER_STATE_PASSWORD\r\n",
		[KEYPADHANDLER_STATE_IN_SETTING] = "KEYPADHANDLER_STATE_IN_SETTING\r\n",
		[KEYPADHANDLER_STATE_IN_SETTING_DATA] = "KEYPADHANDLER_STATE_IN_SETTING_DATA\r\n"
};
static uint8_t keypad_buf[KEYPADHANDLER_BUFFER_LEN];
static size_t keypad_buf_len = 0;
static size_t prev_keypad_buf_len = 0;
static is_entered = false;
static is_confirmed = false;

static bool KEYPADHANDLER_execute(uint8_t fn, uint8_t *data, size_t data_len, uint8_t pressed_state);
static void KEYPADHANDLER_card_prices(uint8_t *data, size_t data_len, uint8_t pressed_state);
static void KEYPADHANDLER_time(uint8_t *data, size_t data_len, uint8_t pressed_state);
static void KEYPADHANDLER_password(uint8_t *data, size_t data_len, uint8_t pressed_state);
static void KEYPADHANDLER_total_card( uint8_t *data, size_t data_len, uint8_t pressed_state);
static void KEYPADHANDLER_delete_total_card( uint8_t *data, size_t data_len, uint8_t pressed_state);
static void KEYPADHANDLER_total_amount( uint8_t *data, size_t data_len, uint8_t pressed_state);
static void KEYPADHANDLER_delete_total_amount( uint8_t *data, size_t data_len, uint8_t pressed_state);
static void KEYPADHANDLER_parse_time(void *data, size_t data_len, RTC_t * rtc);
static uint32_t KEYPADHANDLER_cal_int(uint8_t *data, size_t data_len);
static void KEYPADHANDLER_int_to_str(uint8_t * str, uint8_t *data, size_t data_len);
static bool KEYPADHANDLER_check_password_valid(uint8_t *data, size_t data_len, char* password);
static bool KEYPADHANDLER_clear_data();
static void KEYPADHANDLER_printf();

bool KEYPADHANDLER_init(){

}

bool KEYPADHANDLER_run(){
	CONFIG_t *config;
	RTC_t rtc;
	switch (state) {
		case KEYPADHANDLER_STATE_NOT_IN_SETTING:
			if(KEYPADMNG_is_entered_long() && KEYPADMNG_is_cancelled_long()){
				KEYPADMNG_clear_entered();
				KEYPADMNG_clear_cancelled();
				KEYPADHANDLER_clear_data();
				LCDMNG_set_password_screen(NULL, 0, KEYPADHANDLER_PASSWD_NOT_PRESSED, false);
				state = KEYPADHANDLER_STATE_PASSWORD;
			}
			break;
		case KEYPADHANDLER_STATE_PASSWORD:
			KEYPADMNG_get_data(keypad_buf, &keypad_buf_len);
			if(keypad_buf_len > prev_keypad_buf_len){
				prev_keypad_buf_len = keypad_buf_len;
				LCDMNG_set_password_screen(keypad_buf, keypad_buf_len, KEYPADHANDLER_PASSWD_PRESSED_BUT_NOT_ENTERED, false);
			}

			if(KEYPADMNG_is_entered()){
				config = CONFIG_get();
				if(KEYPADHANDLER_check_password_valid(keypad_buf, keypad_buf_len, config->password)){
					LCDMNG_set_setting_screen();
					utils_log_info("Enter setting mode\r\n");
					state = KEYPADHANDLER_STATE_IN_SETTING;
				}else{
					LCDMNG_set_password_screen(keypad_buf, keypad_buf_len, KEYPADHANDLER_PASSWD_ENTERED, false);
				}
				KEYPADMNG_clear_entered();
				KEYPADHANDLER_clear_data();
			}
			if(KEYPADMNG_is_cancelled()){
				KEYPADMNG_clear_cancelled();
				// Clear buffered data
				state = KEYPADHANDLER_STATE_NOT_IN_SETTING;
				LCDMNG_clear_password_screen();
				KEYPADHANDLER_clear_data();
			}
			break;
		case KEYPADHANDLER_STATE_IN_SETTING:
			if (KEYPADMNG_is_cancelled()){
				KEYPADMNG_clear_cancelled();
				KEYPADHANDLER_clear_data();
				LCDMNG_clear_setting_screen();
				state = KEYPADHANDLER_STATE_NOT_IN_SETTING;
			}
			KEYPADMNG_get_data(keypad_buf, &keypad_buf_len);
			if(keypad_buf_len > 0){
				if(keypad_buf[0] >= 0 && keypad_buf[0] <= 9){
					fn = keypad_buf[0];
					KEYPADHANDLER_execute(fn, NULL , 0, KEYPADHANDLER_SETTING_DATA_NOT_PRESSED);
					KEYPADHANDLER_clear_data();
					state = KEYPADHANDLER_STATE_IN_SETTING_DATA;
				}
			}
			break;
		case KEYPADHANDLER_STATE_IN_SETTING_DATA:
			KEYPADMNG_get_data(keypad_buf, &keypad_buf_len);
			if(keypad_buf_len > prev_keypad_buf_len){
				prev_keypad_buf_len = keypad_buf_len;
				KEYPADHANDLER_execute(fn, keypad_buf, keypad_buf_len , KEYPADHANDLER_SETTING_DATA_PRESSED_BUT_NOT_ENTERED);
			}
			if(KEYPADMNG_is_entered()){
				KEYPADMNG_clear_entered();
				if(!is_entered){
					is_entered = true;
					KEYPADHANDLER_execute(fn, keypad_buf, keypad_buf_len , KEYPADHANDLER_SETTING_DATA_ENTERED);
				}else{
					is_entered = false;
					KEYPADHANDLER_execute(fn, keypad_buf, keypad_buf_len , KEYPADHANDLER_SETTING_DATA_CONFIRMED);
				}
			}
			if (KEYPADMNG_is_cancelled()){
				is_entered = false;
				if(keypad_buf_len == 0){
					// Exit screen
					LCDMNG_clear_setting_data_screen();
					state = KEYPADHANDLER_STATE_IN_SETTING;
				}else{
					// Just clear data
					KEYPADHANDLER_clear_data();
					KEYPADHANDLER_execute(fn, keypad_buf, keypad_buf_len , KEYPADHANDLER_SETTING_DATA_CANCELLED);
				}
				KEYPADMNG_clear_cancelled();
			}
			break;
		default:
			break;
	}
	KEYPADHANDLER_printf();
	prev_state = state;
}

static bool KEYPADHANDLER_execute(uint8_t fn, uint8_t *data, size_t data_len, uint8_t pressed_state){
	switch (fn) {
		case FN_TIME:
			KEYPADHANDLER_time(data, data_len, pressed_state);
			break;
		case FN_CARD_PRICE:
			KEYPADHANDLER_card_prices(data, data_len, pressed_state);
			break;
		case FN_PASSWORD:
			KEYPADHANDLER_password(data, data_len, pressed_state);
			break;
		case FN_TOTAL_CARD:
			KEYPADHANDLER_total_card(data, data_len, pressed_state);
			break;
		case FN_DELETE_TOTAL_CARD:
			KEYPADHANDLER_delete_total_card(data, data_len, pressed_state);
			break;
		case FN_TOTAL_AMOUNT:
			KEYPADHANDLER_total_amount(data, data_len, pressed_state);
			break;
		case FN_DELETE_TOTAL_AMOUNT:
			KEYPADHANDLER_delete_total_amount(data, data_len, pressed_state);
			break;
		default:
			break;
	}
	return true;
}

static void KEYPADHANDLER_time(uint8_t *data, size_t data_len, uint8_t pressed_state){
	RTC_t rtc;
	uint8_t lcdmng_setting_data_state = LCDMNG_SETTING_DATA_NOT_ENTERED;
	switch (pressed_state) {
		case KEYPADHANDLER_SETTING_DATA_NOT_PRESSED:
			rtc = RTC_get_time();
			break;
		case KEYPADHANDLER_SETTING_DATA_PRESSED_BUT_NOT_ENTERED:
			KEYPADHANDLER_parse_time(data, data_len, &rtc);
			break;
		case KEYPADHANDLER_SETTING_DATA_ENTERED:
			KEYPADHANDLER_parse_time(data, data_len, &rtc);
			lcdmng_setting_data_state = LCDMNG_SETTING_DATA_ENTERED;
			break;
		case KEYPADHANDLER_SETTING_DATA_CONFIRMED:
			KEYPADHANDLER_parse_time(data, data_len, &rtc);
			RTC_set_time(&rtc);
			lcdmng_setting_data_state = LCDMNG_SETTING_DATA_CONFIRMED;
			break;
		case KEYPADHANDLER_SETTING_DATA_CANCELLED:
			rtc = RTC_get_time();
			break;
		default:
			break;
	}
	LCDMNG_set_setting_data_screen(fn, &rtc , sizeof(rtc), lcdmng_setting_data_state);
}

static void KEYPADHANDLER_card_prices(uint8_t *data, size_t data_len, uint8_t pressed_state){
	CONFIG_t * config = CONFIG_get();
	uint32_t card_price;
	uint8_t lcdmng_setting_data_state = LCDMNG_SETTING_DATA_NOT_ENTERED;
	switch (pressed_state) {
		case KEYPADHANDLER_SETTING_DATA_NOT_PRESSED:
			card_price = config->card_price;
			break;
		case KEYPADHANDLER_SETTING_DATA_PRESSED_BUT_NOT_ENTERED:
			card_price = KEYPADHANDLER_cal_int(data, data_len);
			break;
		case KEYPADHANDLER_SETTING_DATA_ENTERED:
			card_price = KEYPADHANDLER_cal_int(data, data_len);
			lcdmng_setting_data_state = LCDMNG_SETTING_DATA_ENTERED;
			break;
		case KEYPADHANDLER_SETTING_DATA_CONFIRMED:
			card_price = KEYPADHANDLER_cal_int(data, data_len);
			config->card_price = card_price;
			CONFIG_set(config);
			utils_log_info("New card prices: %d\r\n", card_price);
			lcdmng_setting_data_state = LCDMNG_SETTING_DATA_CONFIRMED;
			break;
		case KEYPADHANDLER_SETTING_DATA_CANCELLED:
			card_price = config->card_price;
			break;
		default:
			break;
	}
	LCDMNG_set_setting_data_screen(FN_CARD_PRICE, &card_price, sizeof(card_price), lcdmng_setting_data_state);
}

static void KEYPADHANDLER_password( uint8_t *data, size_t data_len, uint8_t pressed_state){
	CONFIG_t * config = CONFIG_get();
	char password[SETTING_MODE_PASSWORD_MAX_LEN];
	size_t password_len = data_len;
	uint8_t lcdmng_setting_data_state = LCDMNG_SETTING_DATA_NOT_ENTERED;
	if(data_len > SETTING_MODE_PASSWORD_MAX_LEN){
		password_len = SETTING_MODE_PASSWORD_MAX_LEN;
	}
	switch (pressed_state) {
		case KEYPADHANDLER_SETTING_DATA_NOT_PRESSED:
			memset(password, 0 , sizeof(password));
			strcpy(password, config->password);
			break;
		case KEYPADHANDLER_SETTING_DATA_PRESSED_BUT_NOT_ENTERED:
			memset(password, 0 , sizeof(password));
			KEYPADHANDLER_int_to_str(password, data, password_len);
			break;
		case KEYPADHANDLER_SETTING_DATA_ENTERED:
			memset(password, 0 , sizeof(password));
			KEYPADHANDLER_int_to_str(password, data, password_len);
			lcdmng_setting_data_state = LCDMNG_SETTING_DATA_ENTERED;
			break;
		case KEYPADHANDLER_SETTING_DATA_CONFIRMED:
			memset(config->password, 0 , sizeof(config->password));
			KEYPADHANDLER_int_to_str(config->password, data, password_len);
			CONFIG_set(config);
			utils_log_info("New password: %s\r\n", config->password);
			lcdmng_setting_data_state = LCDMNG_SETTING_DATA_CONFIRMED;
			break;
		case KEYPADHANDLER_SETTING_DATA_CANCELLED:
			memset(password, 0 , sizeof(password));
			strcpy(password, config->password);
			break;
		default:
			break;
	}
	LCDMNG_set_setting_data_screen(FN_PASSWORD, password, SETTING_MODE_PASSWORD_MAX_LEN, lcdmng_setting_data_state);
}

static void KEYPADHANDLER_total_card( uint8_t *data, size_t data_len, uint8_t pressed_state){
	CONFIG_t * config = CONFIG_get();
	uint8_t lcdmng_setting_data_state = LCDMNG_SETTING_DATA_NOT_ENTERED;
	utils_log_info("Total card: %d\r\n",config->total_card);
	uint32_t total_card[] = {
		config->total_card,
		config->total_card_by_day,
		config->total_card_by_month
	};
	LCDMNG_set_setting_data_screen(FN_TOTAL_CARD, total_card, sizeof(total_card), lcdmng_setting_data_state);
}

static void KEYPADHANDLER_delete_total_card( uint8_t *data, size_t data_len, uint8_t pressed_state){
	CONFIG_t * config = CONFIG_get();
	uint8_t lcdmng_setting_data_state = LCDMNG_SETTING_DATA_NOT_ENTERED;
	switch (pressed_state) {
		case KEYPADHANDLER_SETTING_DATA_NOT_PRESSED:
		case KEYPADHANDLER_SETTING_DATA_PRESSED_BUT_NOT_ENTERED:
			lcdmng_setting_data_state = LCDMNG_SETTING_DATA_NOT_ENTERED;
			break;
		case KEYPADHANDLER_SETTING_DATA_ENTERED:
		case KEYPADHANDLER_SETTING_DATA_CONFIRMED:
			config->total_card = 0;
			config->total_card_by_day = 0;
			config->total_card_by_month = 0;
			CONFIG_set(config);
			utils_log_info("Deleted total card\r\n");
			lcdmng_setting_data_state = LCDMNG_SETTING_DATA_CONFIRMED;
			break;
		case KEYPADHANDLER_SETTING_DATA_CANCELLED:
			break;
		default:
			break;
	}
	LCDMNG_set_setting_data_screen(FN_DELETE_TOTAL_CARD, NULL, 0, lcdmng_setting_data_state);
}

static void KEYPADHANDLER_total_amount( uint8_t *data, size_t data_len, uint8_t pressed_state){
	CONFIG_t * config = CONFIG_get();
	uint8_t lcdmng_setting_data_state = LCDMNG_SETTING_DATA_NOT_ENTERED;
	utils_log_info("Total amount %d\r\n",config->total_amount);
	LCDMNG_set_setting_data_screen(FN_TOTAL_AMOUNT, &config->total_amount, sizeof(config->total_amount), lcdmng_setting_data_state);
}

static void KEYPADHANDLER_delete_total_amount( uint8_t *data, size_t data_len, uint8_t pressed_state){
	CONFIG_t * config = CONFIG_get();
	uint8_t lcdmng_setting_data_state = LCDMNG_SETTING_DATA_NOT_ENTERED;
	switch (pressed_state) {
		case KEYPADHANDLER_SETTING_DATA_NOT_PRESSED:
		case KEYPADHANDLER_SETTING_DATA_PRESSED_BUT_NOT_ENTERED:
			lcdmng_setting_data_state = LCDMNG_SETTING_DATA_NOT_ENTERED;
			break;
		case KEYPADHANDLER_SETTING_DATA_ENTERED:
		case KEYPADHANDLER_SETTING_DATA_CONFIRMED:
			config->total_amount = 0;
			utils_log_info("Delete total amount\r\n");
			CONFIG_set(config);
			lcdmng_setting_data_state = LCDMNG_SETTING_DATA_CONFIRMED;
			break;
		case KEYPADHANDLER_SETTING_DATA_CANCELLED:
			break;
		default:
			break;
	}
	LCDMNG_set_setting_data_screen(FN_DELETE_TOTAL_AMOUNT, NULL, 0, lcdmng_setting_data_state);
}


static void KEYPADHANDLER_parse_time(void *data, size_t data_len, RTC_t * rtc){
	uint8_t * data_u8_p = (uint8_t *)data;
	rtc->hour = data_u8_p[0] * 10 + data_u8_p[1];
	rtc->minute = data_u8_p[2] * 10 + data_u8_p[3];
	rtc->date = data_u8_p[4] * 10 + data_u8_p[5];
	rtc->month = data_u8_p[6] * 10 + data_u8_p[7];
	rtc->year = data_u8_p[8] * 1000 + data_u8_p[9] * 100 + data_u8_p[10] * 10 + data_u8_p[11];
}

static uint32_t KEYPADHANDLER_cal_int(uint8_t *data, size_t data_len){
	uint32_t value = 0;
	for (size_t var = 0; var < data_len; ++var) {
		value = value * 10 + data[var];
	}
	return value;
}

static void KEYPADHANDLER_int_to_str(uint8_t * str, uint8_t *data, size_t data_len){
	for (size_t var = 0; var < data_len; ++var) {
		str[var] = data[var] + '0';
	}
}

static bool KEYPADHANDLER_check_password_valid(uint8_t *data, size_t data_len, char* password){
	if(data_len != strlen(password)){
		utils_log_error("Password is not valid\r\n");
		return false;
	}
	for (int var = 0; var < data_len; ++var) {
		if(password[var] != data[var] + '0'){
			return false;
		}
	}
	return true;
}

static bool KEYPADHANDLER_clear_data(){
	KEYPADMNG_clear_data();
	memset(keypad_buf, 0 , sizeof(keypad_buf));
	keypad_buf_len = 0;
	prev_keypad_buf_len = 0;
}

static void KEYPADHANDLER_printf(){
	if(prev_state != state){
		utils_log_info(state_name[state]);
	}
}

