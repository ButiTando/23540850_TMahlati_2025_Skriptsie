/*
 * blm_utils.h
 *
 *  Created on: Oct 22, 2025
 *      Author: tando
 */

#ifndef INC_BLM_UTILS_H_
#define INC_BLM_UTILS_H_
#include "stm32h7xx_hal.h"
#include "main.h"
#include <stdlib.h>
#include <stdio.h>

// Opcodes, matching radiation_systems/dosimeter.py.
#define BLM_CMD_SET_PERIOD 0x01
#define BLM_CMD_SET_DATE   0x02
#define BLM_CMD_SET_TIME   0x03

#define BLM_PERIOD_MIN_S 1
#define BLM_PERIOD_MAX_S 3600

// Packet formatting
typedef union {
    uint32_t value;  // Full 32-bit view

    struct {
        uint32_t data    : 24;  // lower 24 bits
        uint32_t command : 8;   // upper 8 bits
    } fields;

} CommandPacket;

// Date formatting
typedef union {
    uint32_t value;  // Full 32-bit view

    struct {
        uint32_t year    : 8;
        uint32_t month : 8;
        uint32_t day: 8;
    } fields;

} DateCommand;

// Time formatting
typedef union {
    uint32_t value;  // Full 32-bit view

    struct {
        uint32_t hour    : 8;   // 0–23
        uint32_t minute  : 8;   // 0–59
        uint32_t second  : 8;   // 0–59
    } fields;

} TimeCommand;

// Type to organise timers
typedef struct pulse_in{
	uint8_t channel_number;
	TIM_HandleTypeDef* master_timer;
	TIM_HandleTypeDef* slave_timer;
}Pulse_input;

HAL_StatusTypeDef RTC_UpdateDateTime(RTC_HandleTypeDef *hrtc, const DateCommand *dateCmd, const TimeCommand *timeCmd);

// void return
void ExtractDateFromCommand(const CommandPacket *cmdPkt, DateCommand *dateCmd);
void ExtractTimeFromCommand(const CommandPacket *cmdPkt, TimeCommand *timeCmd);

// Act on one command word (host order, opcode in the top 8 bits).
HAL_StatusTypeDef BLM_HandleCommand(RTC_HandleTypeDef *hrtc, uint32_t word,
                                    uint32_t *period_s);

// uint32_t return
uint32_t get_32_bit_count(TIM_HandleTypeDef* master, TIM_HandleTypeDef* slave);
void rest_channel_count(Pulse_input* channel);

// Add to blm_utils.h, just before the #endif
/**
 * @brief  Generates a CSV string with the current RTC date/time and readings from six channels.
 * @param  hrtc: Pointer to RTC handle (e.g., &hrtc)
 * @param  channel1: First channel's Pulse_input structure
 * @param  channel2: Second channel's Pulse_input structure
 * @param  channel3: Third channel's Pulse_input structure
 * @param  channel4: Fourth channel's Pulse_input structure
 * @param  channel5: Fifth channel's Pulse_input structure
 * @param  channel6: Sixth channel's Pulse_input structure
 * @param  csvBuffer: Pointer to the buffer to store the CSV string
 * @param  bufferSize: Size of the csvBuffer
 * @retval HAL status (HAL_OK / HAL_ERROR)
 */
HAL_StatusTypeDef GenerateChannelCSV(RTC_HandleTypeDef *hrtc,
									 Pulse_input *channel1,
									 Pulse_input *channel2,
									 Pulse_input *channel3,
									 Pulse_input *channel4,
									 Pulse_input *channel5,
									 Pulse_input *channel6,
									 char *csvBuffer,
									 uint32_t bufferSize);


#endif /* INC_BLM_UTILS_H_ */
