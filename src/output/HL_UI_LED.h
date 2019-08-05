/*
 * Copyright © 2019 Samsung Electronics co., Ltd. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __HL_UI_LED_H__
#define __HL_UI_LED_H__
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <peripheral_io.h>

#define B_OFF_SET 1
#define G_OFF_SET 2
#define R_OFF_SET 3

#define BITRATE 8000000

typedef struct{
	uint32_t number;
	peripheral_spi_h hnd_spi;
	uint8_t  *pixels;
	uint8_t  brightness;
}HL_UI_LED;

/**
 * @brief: Initialise a set of apa102 LEDs
 *
 * @param[in] led_num: Number of leds (0-255)
 *
 * @returns:  pointer of handler\ Success
 *            NULL\ Error
 */
HL_UI_LED *HL_UI_LED_Init(uint32_t led_num);

/**
 * @brief: Change the global brightness and fresh
 *
 * @param[in] handle: handler of HL_UI_LED
 * @param[in] brightness: New brightness value
 */
void HL_UI_LED_Change_Brightness(HL_UI_LED *handle, uint8_t brightness);

/**
 * @brief: Get the brightness
 *
 * @param[in] handle: handler of HL_UI_LED
 * @return current brightness value (0-31)
 */
int HL_UI_LED_Get_Brightness(HL_UI_LED *handle);

/**
 * @brief: Set color for a specific pixel by giving R, G and B value separately
 *
 * @param[in] handle: handler of HL_UI_LED
 * @param[in] index: Index of the target led (0-255)
 * @param[in] red: Intensity of red colour (0-255)
 * @param[in] green: Intensity of green colour (0-255)
 * @param[in] blue: Intensity of blue colour (0-255)
 */
void HL_UI_LED_Set_Pixel_RGB(HL_UI_LED *handle, uint32_t index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief: Get colour form a specific pixel for R, G and B separately
 *
 * @param[in] handle: handler of HL_UI_LED
 * @param[in] index: Index of the target led (0-255)
 * @param[out] red: Intensity of red colour (0-255)
 * @param[out] green: Intensity of green colour (0-255)
 * @param[out] blue: Intensity of blue colour (0-255)
 */
void HL_UI_LED_Get_Pixel_RGB(HL_UI_LED *handle, uint32_t index, uint8_t *red, uint8_t *green, uint8_t *blue);

/**
 * @brief: Set color for a specific pixel by using 4byte date
 *
 * @param[in] handle: handler of HL_UI_LED
 * @param[in] index: Index of the target led (0-255)
 * @param[in] red: Intensity of red colour (0-255)
 * @param[in] green: Intensity of green colour (0-255)
 * @param[in] blue: Intensity of blue colour (0-255)
 *
 * @example: HL_UI_LED_Get_Pixel_RGB(1, 0xFF0000) sets the 1st LED to red colour
 */
void HL_UI_LED_Set_Pixel_4byte(HL_UI_LED *handle, uint32_t index, uint32_t colour);

/**
 * @brief: Get colour form a specific pixel
 *
 * @param[in] handle: handler of HL_UI_LED
 * @param[in] index: Index of the target led (0-255)
 *
 * @returns: 32 bits colour data
 */
uint32_t HL_UI_LED_Get_Pixel_4byte(HL_UI_LED *handle, uint32_t index);

/**
 * @brief: Clear all the pixels
 *
 * @param[in] handle: handler of HL_UI_LED
 */
void HL_UI_LED_Clear_All(HL_UI_LED *handle);

/**
 * @brief: Refresh display (After modifing pixel colour)
 */
int HL_UI_LED_Refresh(HL_UI_LED *handle);

/**
 * @brief: Show display (After modifing pixel colour)
 *
 * @param[in] handle: handler of HL_UI_LED
 */
int HL_UI_LED_Show(HL_UI_LED *handle);


/**
 * @brief: Close SPI file, release memory
 *
 * @param[in] handle: handler of HL_UI_LED
 */
void HL_UI_LED_Close(HL_UI_LED *handle);
#endif
