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

#include "HL_UI_LED.h"

#define SUCCESS_FLAG 760302
#define RETRY_TIMES 3

#define SPI_BUS 0
#define SPI_DEV 1

HL_UI_LED *
HL_UI_LED_Init(uint32_t led_num)
{
	HL_UI_LED *handle;
	int count = 0;
	int ret;

	handle = (HL_UI_LED*)malloc(sizeof(HL_UI_LED));
	if(handle == NULL)
	{
		return NULL;
	}
	handle->number = led_num;
	handle->brightness = 0xFF;
	handle->pixels = (uint8_t *)malloc(handle->number * 4);
	if(handle->pixels == NULL)
	{
		free(handle);
		return NULL;
	}

	while(count < RETRY_TIMES)
	{
		if(peripheral_spi_open(SPI_BUS, SPI_DEV, &(handle->hnd_spi)) == 0)
		{
			printf("spi open success!\n");
			count = SUCCESS_FLAG;
			if((ret = peripheral_spi_set_frequency(handle->hnd_spi, BITRATE)) != 0)
			{
				printf("Frequency Failed : 0x%x\n", ret);
			}
			if((ret = peripheral_spi_set_bits_per_word(handle->hnd_spi, 8)) != 0)
			{
				printf("BIT_WORD Failed : 0x%x\n", ret);
			}
			if((ret = peripheral_spi_set_bit_order(handle->hnd_spi,PERIPHERAL_SPI_BIT_ORDER_MSB)) != 0)
			{
				printf("BIT_ORDER Failed : 0x%x\n", ret);
			}
			if((ret = peripheral_spi_set_mode(handle->hnd_spi,PERIPHERAL_SPI_MODE_1)) != 0)
			{
				printf("SPI Mode Failed : 0x%x\n", ret);
			}
			break;
		}
		else
		{
			count++;
			continue;
		}
	}
	if(count == SUCCESS_FLAG)
	{
		HL_UI_LED_Clear_All(handle);
		return handle;
	}
	else
	{
		free(handle->pixels);
		free(handle);
		return NULL;
	}
}

void
HL_UI_LED_Change_Brightness(HL_UI_LED *handle, uint8_t brightness)
{
	if (brightness > 31)
		handle->brightness = 0xFF;
	else
		handle->brightness = 0xE0 | (0x1F & brightness);
	HL_UI_LED_Refresh(handle);
}

int
HL_UI_LED_Get_Brightness(HL_UI_LED *handle)
{
	return handle->brightness & 0x1F;
}

void
HL_UI_LED_Set_Pixel_RGB(HL_UI_LED *handle, uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
{
	if (index < handle->number) {
		uint8_t *ptr = &(handle->pixels[index * 4]);
		ptr[R_OFF_SET] = red;
		ptr[G_OFF_SET] = green;
		ptr[B_OFF_SET] = blue;
	}
}

void
HL_UI_LED_Get_Pixel_RGB(HL_UI_LED *handle, uint32_t index, uint8_t *red, uint8_t *green, uint8_t *blue)
{
	if (index < handle->number) {
		uint8_t *ptr = &(handle->pixels[index * 4]);
		*red = ptr[R_OFF_SET];
		*green = ptr[G_OFF_SET];
		*blue = ptr[B_OFF_SET];
	}
}

void
HL_UI_LED_Set_Pixel_4byte(HL_UI_LED *handle, uint32_t index, uint32_t colour)
{
	uint8_t  r, g, b;
	uint8_t *ptr = (uint8_t *)&colour;
	r = ptr[R_OFF_SET];
	g = ptr[G_OFF_SET];
	b = ptr[B_OFF_SET];
	HL_UI_LED_Set_Pixel_RGB(handle, index, r, g, b);
}

uint32_t
HL_UI_LED_Get_Pixel_4byte(HL_UI_LED *handle, uint32_t index)
{
	uint8_t r=0, g=0, b=0;
	uint32_t colour = 0;
	uint8_t *ptr = (uint8_t *)&colour;
	HL_UI_LED_Get_Pixel_RGB(handle, index, &r, &g, &b);
	ptr[R_OFF_SET] = r;
	ptr[G_OFF_SET] = g;
	ptr[B_OFF_SET] = b;
	return colour;
}

void
HL_UI_LED_Clear_All(HL_UI_LED *handle)
{
	uint8_t *ptr;
	uint32_t i;
	for(ptr = handle->pixels, i=0; i<handle->number; i++, ptr += 4) {
		ptr[1] = 0x00;
		ptr[2] = 0x00;
		ptr[3] = 0x00;
	}
	HL_UI_LED_Refresh(handle);
}

int
HL_UI_LED_Refresh(HL_UI_LED *handle)
{
	int ret;
	uint32_t i;
	uint32_t buf_len = 4 + 4 * handle->number + (handle->number + 15) / 16 + 1;
	uint8_t *ptr, *qtr;
	uint8_t *tx = (uint8_t *)malloc(buf_len);

	if( tx == NULL )
	{
		return -1;
	}
	// start frame
	for (i = 0; i < 4; i++)
		*(tx + i) = 0x00;

	// LED data
	qtr = tx + 4;

	for(ptr = handle->pixels, i=0; i<handle->number; i++, ptr += 4, qtr += 4) {
		qtr[0] = handle->brightness;
		qtr[1] = ptr[1];
		qtr[2] = ptr[2];
		qtr[3] = ptr[3];
	}

	// end frame
	for (i = handle->number * 4 + 4; i < buf_len; i++)
	{
		*(tx + i) = 0x00;
	}

	ret = peripheral_spi_write(handle->hnd_spi, tx, buf_len);
	free(tx);
	if (ret != 0)
	{
		fprintf(stdout, "[Error] can't send spi message\n");
		return -2;
	}

	return 0;
}

int
HL_UI_LED_Show(HL_UI_LED *handle)
{
	return HL_UI_LED_Refresh(handle);
}

void
HL_UI_LED_Close(HL_UI_LED *handle)
{
	HL_UI_LED_Clear_All(handle);
	peripheral_spi_close(handle->hnd_spi);

	if (handle->pixels) {
		free(handle->pixels);
	}

	free(handle);
}
