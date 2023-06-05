/*
 * config.c
 *
 *  Created on: Jun 2, 2023
 *      Author: xuanthodo
 */


#include "App/config.h"
#include "Device/eeprom.h"

#define EEPROM_CONFIG_ADDRESS	0x00F0

static CONFIG_t config;

bool CONFIG_init(){
	EEPROM_read(EEPROM_CONFIG_ADDRESS, (uint8_t*)&config, sizeof(CONFIG_t));
}

CONFIG_t * CONFIG_get(){
	return &config;
}

void CONFIG_set(CONFIG_t * _config){
	memcpy(&config, _config, sizeof(config));
	EEPROM_write(EEPROM_CONFIG_ADDRESS, &config, sizeof(CONFIG_t));
}