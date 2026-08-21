/*
 * blm_utils.c
 *
 *  Created on: Oct 22, 2025
 *      Author: tando
 */

#include "stm32h7xx_hal.h"
#include "blm_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/**
  * @brief  Updates the RTC date and time using integer values.
  * @param  hrtc: Pointer to RTC handle (e.g., &hrtc)
  * @param  day: Day of the month (1–31)
  * @param  month: Month (1–12)
  * @param  year: Full year (e.g., 2025)
  * @param  hour: Hour (0–23)
  * @param  minute: Minute (0–59)
  * @param  second: Second (0–59)
  * @retval HAL status (HAL_OK / HAL_ERROR / HAL_BUSY / HAL_TIMEOUT)
 */
HAL_StatusTypeDef RTC_UpdateDateTime(RTC_HandleTypeDef *hrtc, const DateCommand *dateCmd, const TimeCommand *timeCmd)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    HAL_StatusTypeDef status;

    // Validate input pointers
    if (hrtc == NULL || dateCmd == NULL || timeCmd == NULL)
        return HAL_ERROR;

    // ---- Populate RTC Time structure ----
    sTime.Hours   = timeCmd->fields.hour;
    sTime.Minutes = timeCmd->fields.minute;
    sTime.Seconds = timeCmd->fields.second;
    sTime.TimeFormat = RTC_HOURFORMAT12_AM;

    // ---- Populate RTC Date structure ----
    uint8_t year = dateCmd->fields.year;
    if (year >= 100)      // In case full years like 2025 are used
        year -= 2000;

    sDate.Year  = year;                  // Two-digit year (00–99)
    sDate.Month = dateCmd->fields.month; // 1–12
    sDate.Date  = dateCmd->fields.day;   // 1–31

    // ---- Apply new Time ----
    status = HAL_RTC_SetTime(hrtc, &sTime, RTC_FORMAT_BIN);
    if (status != HAL_OK)
        return status;

    return HAL_RTC_SetDate(hrtc, &sDate, RTC_FORMAT_BIN);
}

// Data processing
void ExtractDateFromCommand(const CommandPacket *cmdPkt, DateCommand *dateCmd)
{
    if (cmdPkt == NULL || dateCmd == NULL)
        return;

    // Clear the destination union to avoid leftover bits
    dateCmd->value = 0;

    // Extract the 24-bit data field from the command packet
    uint32_t rawData = cmdPkt->fields.data & 0xFFFFFF;

    // Field by field: a bulk copy would swap year and day.
    dateCmd->fields.year  = (rawData >> 16) & 0xFF;
    dateCmd->fields.month = (rawData >> 8) & 0xFF;
    dateCmd->fields.day   = rawData & 0xFF;
}

void ExtractTimeFromCommand(const CommandPacket *cmdPkt, TimeCommand *timeCmd)
{
    if (cmdPkt == NULL || timeCmd == NULL)
        return;

    // Clear the destination union to avoid leftover bits
    timeCmd->value = 0;

    // Extract the 24-bit data field from the command packet
    uint32_t rawData = cmdPkt->fields.data & 0xFFFFFF;

    // Field by field: a bulk copy would swap hour and second.
    timeCmd->fields.hour   = (rawData >> 16) & 0xFF;
    timeCmd->fields.minute = (rawData >> 8) & 0xFF;
    timeCmd->fields.second = rawData & 0xFF;
}

uint32_t get_32_bit_count(TIM_HandleTypeDef* master, TIM_HandleTypeDef* slave){

	if(slave == NULL){
		return __HAL_TIM_GET_COUNTER(master);
	}

	else{
		uint16_t lsw = __HAL_TIM_GET_COUNTER(master);
		uint16_t msw1 = __HAL_TIM_GET_COUNTER(slave);
		uint16_t msw2 = __HAL_TIM_GET_COUNTER(slave);
		uint16_t msw  = 0;

//		Check if master clock overflowed
		if(msw1>msw2){
			msw = 0xffff-((msw1-msw2))-1;
		}

		else if(msw1 < msw2){
			msw = (msw2-msw1)-1;
		}

		else{
			msw = msw2;
		}

		int32_t number =  ((uint32_t)msw<<16) | (uint32_t)lsw;
		return number;
	}

}

void rest_channel_count(Pulse_input* channel){
	if(channel->slave_timer == NULL){
		__HAL_TIM_SET_COUNTER(channel->master_timer, 0);
	}

	else{
		__HAL_TIM_SET_COUNTER(channel->master_timer, 0);
		__HAL_TIM_SET_COUNTER(channel->slave_timer, 0);
	}
}

// Add to blm_utils.c, after the existing functions

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
HAL_StatusTypeDef GenerateChannelCSV(RTC_HandleTypeDef *hrtc, Pulse_input *channel1, Pulse_input *channel2, Pulse_input *channel3, Pulse_input *channel4, Pulse_input *channel5, Pulse_input *channel6, char *csvBuffer, uint32_t bufferSize)
{
    // Validate input pointers
    if (hrtc == NULL || channel1 == NULL || channel2 == NULL || channel3 == NULL ||
        channel4 == NULL || channel5 == NULL || channel6 == NULL || csvBuffer == NULL)
    {
        return HAL_ERROR;
    }

    // Get current date and time from RTC
    RTC_DateTypeDef sDate = {0};
    RTC_TimeTypeDef sTime = {0};

    // Time first: the shadow registers stay locked until date is read.
    HAL_RTC_GetTime(hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(hrtc, &sDate, RTC_FORMAT_BIN);


    uint8_t year_lower = sDate.Year;
    uint8_t month = sDate.Month;
    uint8_t day = sDate.Date;

    uint8_t hour = sTime.Hours;
    uint8_t min = sTime.Minutes;
    uint8_t sec = sTime.Seconds;

    // Get channel readings
    uint32_t channel1_reading = get_32_bit_count(channel1->master_timer, channel1->slave_timer);
    uint32_t channel2_reading = get_32_bit_count(channel2->master_timer, channel2->slave_timer);
    uint32_t channel3_reading = get_32_bit_count(channel3->master_timer, channel3->slave_timer);
    uint32_t channel4_reading = get_32_bit_count(channel4->master_timer, channel4->slave_timer);
    uint32_t channel5_reading = get_32_bit_count(channel5->master_timer, channel5->slave_timer);
    uint32_t channel6_reading = get_32_bit_count(channel6->master_timer, channel6->slave_timer);

    // Reset channel counts
    rest_channel_count(channel1);
    rest_channel_count(channel2);
    rest_channel_count(channel3);
    rest_channel_count(channel4);
    rest_channel_count(channel5);
    rest_channel_count(channel6);

    // clear buffer
    csvBuffer[0] = '\0';


    // Format the CSV string: YYYY-MM-DD HH:MM:SS,channel1,channel2,channel3,channel4,channel5,channel6
    uint32_t year = year_lower + 2000; // Convert two-digit year to four-digit
    int written = snprintf(csvBuffer, bufferSize, "%04lu-%02u-%02u %02u:%02u:%02u,%lu,%lu,%lu,%lu,%lu,%lu\n",
                           year, month, day,
                           hour, min, sec,
                           channel1_reading, channel2_reading, channel3_reading,
                           channel4_reading, channel5_reading, channel6_reading);

    // Check if the buffer was large enough
    if (written < 0 || written >= bufferSize)
    {
        csvBuffer[0] = '\0'; // Clear buffer on error
        return HAL_ERROR;
    }

    return HAL_OK;
}


/**
  * @brief  Act on one 4-byte command word from the control station.
  *         The opcode is the only thing separating a date from a time.
  */
HAL_StatusTypeDef BLM_HandleCommand(RTC_HandleTypeDef *hrtc, uint32_t word,
                                    uint32_t *period_s)
{
    if (hrtc == NULL || period_s == NULL)
        return HAL_ERROR;

    CommandPacket packet;
    packet.value = word;

    uint8_t opcode = (word >> 24) & 0xFF;
    uint32_t data = word & 0xFFFFFF;

    RTC_TimeTypeDef currentTime = {0};
    RTC_DateTypeDef currentDate = {0};
    DateCommand dateCmd = {0};
    TimeCommand timeCmd = {0};

    switch (opcode)
    {
    case BLM_CMD_SET_PERIOD:
        if (data < BLM_PERIOD_MIN_S)
            data = BLM_PERIOD_MIN_S;
        if (data > BLM_PERIOD_MAX_S)
            data = BLM_PERIOD_MAX_S;
        *period_s = data;
        return HAL_OK;

    case BLM_CMD_SET_DATE:
        // Written as a pair, so read back the half we are not changing.
        HAL_RTC_GetTime(hrtc, &currentTime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(hrtc, &currentDate, RTC_FORMAT_BIN);
        timeCmd.fields.hour = currentTime.Hours;
        timeCmd.fields.minute = currentTime.Minutes;
        timeCmd.fields.second = currentTime.Seconds;

        ExtractDateFromCommand(&packet, &dateCmd);
        if (dateCmd.fields.month < 1 || dateCmd.fields.month > 12 ||
            dateCmd.fields.day < 1 || dateCmd.fields.day > 31)
            return HAL_ERROR;
        return RTC_UpdateDateTime(hrtc, &dateCmd, &timeCmd);

    case BLM_CMD_SET_TIME:
        HAL_RTC_GetTime(hrtc, &currentTime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(hrtc, &currentDate, RTC_FORMAT_BIN);
        dateCmd.fields.year = currentDate.Year;
        dateCmd.fields.month = currentDate.Month;
        dateCmd.fields.day = currentDate.Date;

        ExtractTimeFromCommand(&packet, &timeCmd);
        if (timeCmd.fields.hour > 23 || timeCmd.fields.minute > 59 ||
            timeCmd.fields.second > 59)
            return HAL_ERROR;
        return RTC_UpdateDateTime(hrtc, &dateCmd, &timeCmd);

    default:
        return HAL_ERROR;
    }
}
