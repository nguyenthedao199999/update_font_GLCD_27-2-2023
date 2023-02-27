/*
 * ST7920_SERIAL.c
 *
 *  Created on: 07-Jun-2019
 *      Author: poe
 */

#include "stm32l4xx_hal.h"
#include "ST7920_SERIAL.h"
#include "delay.h"
#include "font.h"
/* setup below is as follows
 * B4 ---------> SCLK (EN)
 * A15 ---------> CS (RS)
 * B5 ---------> SID (RW)
 * G15 ---------> RST (RST)
 *
 */


#define SCLK_PIN GPIO_PIN_2
#define SCLK_PORT GPIOC
#define CS_PIN GPIO_PIN_3
#define CS_PORT GPIOI
#define SID_PIN GPIO_PIN_3
#define SID_PORT GPIOD
#define RST_PIN GPIO_PIN_7
#define RST_PORT GPIOB


uint8_t startRow, startCol, endRow, endCol; // coordinates of the dirty rectangle
uint8_t numRows = 64;
uint8_t numCols = 128;
uint8_t Graphic_Check = 0;
uint8_t image[(128 * 64)/8];

// A replacement for SPI_TRANSMIT

void SendByteSPI(uint8_t byte)
{
	for(int i=0;i<8;i++)
	{
		if((byte<<i)&0x80)
			{
				HAL_GPIO_WritePin(SID_PORT, SID_PIN, GPIO_PIN_SET);  // SID=1  OR MOSI
			}

		else HAL_GPIO_WritePin(SID_PORT, SID_PIN, GPIO_PIN_RESET);  // SID=0

		HAL_GPIO_WritePin(SCLK_PORT, SCLK_PIN, GPIO_PIN_RESET);  // SCLK =0  OR SCK

		HAL_GPIO_WritePin(SCLK_PORT, SCLK_PIN, GPIO_PIN_SET);  // SCLK=1

	}
}




void ST7920_SendCmd (uint8_t cmd)
{

	HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);  // PUll the CS high

	SendByteSPI(0xf8+(0<<1));  // send the SYNC + RS(0)
	SendByteSPI(cmd&0xf0);  // send the higher nibble first
	SendByteSPI((cmd<<4)&0xf0);  // send the lower nibble
	delay_us(50);
	//HAL_Delay(50);
	HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);  // PUll the CS LOW

}

void ST7920_SendData (uint8_t data)
{

	HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);  // PUll the CS high

	SendByteSPI(0xf8+(1<<1));  // send the SYNC + RS(1)
	SendByteSPI(data&0xf0);  // send the higher nibble first
	SendByteSPI((data<<4)&0xf0);  // send the lower nibble
	delay_us(1);
	//HAL_Delay(50);
	HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);  // PUll the CS LOW
}

void ST7920_SendString(int row, int col, char* string)
{
    switch (row)
    {
        case 0:
            col |= 0x80;
            break;
        case 1:
            col |= 0x90;
            break;
        case 2:
            col |= 0x88;
            break;
        case 3:
            col |= 0x98;
            break;
        default:
            col |= 0x80;
            break;
    }

    ST7920_SendCmd(col);

    while (*string)
    	{
    		ST7920_SendData(*string++);
    	}
}



// switch to graphic mode or normal mode::: enable = 1 -> graphic mode enable = 0 -> normal mode

void ST7920_GraphicMode (int enable)   // 1-enable, 0-disable
{
	if (enable == 1)
	{
		ST7920_SendCmd(0x30);  // 8 bit mode
		HAL_Delay (1);
		ST7920_SendCmd(0x34);  // switch to Extended instructions
		HAL_Delay (1);
		ST7920_SendCmd(0x36);  // enable graphics
		HAL_Delay (1);
		Graphic_Check = 1;  // update the variable
	}

	else if (enable == 0)
	{
		ST7920_SendCmd(0x30);  // 8 bit mode
		HAL_Delay (1);
		Graphic_Check = 0;  // update the variable
	}
}

void ST7920_DrawBitmap(const unsigned char* graphic)
{
	uint8_t x, y;
	for(y = 0; y < 64; y++)
	{
		if(y < 32)
		{
			for(x = 0; x < 8; x++)							// Draws top half of the screen.
			{												// In extended instruction mode, vertical and horizontal coordinates must be specified before sending data in.
				ST7920_SendCmd(0x80 | y);				// Vertical coordinate of the screen is specified first. (0-31)
				ST7920_SendCmd(0x80 | x);				// Then horizontal coordinate of the screen is specified. (0-8)
				ST7920_SendData(graphic[2*x + 16*y]);		// Data to the upper byte is sent to the coordinate.
				ST7920_SendData(graphic[2*x+1 + 16*y]);	// Data to the lower byte is sent to the coordinate.
			}
		}
		else
		{
			for(x = 0; x < 8; x++)							// Draws bottom half of the screen.
			{												// Actions performed as same as the upper half screen.
				ST7920_SendCmd(0x80 | (y-32));			// Vertical coordinate must be scaled back to 0-31 as it is dealing with another half of the screen.
				ST7920_SendCmd(0x88 | x);
				ST7920_SendData(graphic[2*x + 16*y]);
				ST7920_SendData(graphic[2*x+1 + 16*y]);
			}
		}

	}
}


// Update the display with the selected graphics
void ST7920_Update(void)
{
	ST7920_DrawBitmap(image);
}



void ST7920_Clear()
{
	if (Graphic_Check == 1)  // if the graphic mode is set
	{
		uint8_t x, y;
		for(y = 0; y < 64; y++)
		{
			if(y < 32)
			{
				ST7920_SendCmd(0x80 | y);
				ST7920_SendCmd(0x80);
			}
			else
			{
				ST7920_SendCmd(0x80 | (y-32));
				ST7920_SendCmd(0x88);
			}
			for(x = 0; x < 8; x++)
			{
				ST7920_SendData(0);
				ST7920_SendData(0);
			}
		}
	}

	else
	{
		ST7920_SendCmd(0x01);   // clear the display using command
		HAL_Delay(2); // delay >1.6 ms
	}
}


void ST7920_Init (void)
{
	HAL_GPIO_WritePin(RST_PORT, RST_PIN, GPIO_PIN_RESET);  // RESET=0
	HAL_Delay(10);   // wait for 10ms
	HAL_GPIO_WritePin(RST_PORT, RST_PIN, GPIO_PIN_SET);  // RESET=1

	HAL_Delay(50);   //wait for >40 ms


	ST7920_SendCmd(0x30);  // 8bit mode
	delay_us(110);  //  >100us delay
	//HAL_Delay(110);
	ST7920_SendCmd(0x30);  // 8bit mode
	delay_us(40);  // >37us delay
	//HAL_Delay(40);
	ST7920_SendCmd(0x08);  // D=0, C=0, B=0
	delay_us(110);  // >100us delay
	//HAL_Delay(110);
	ST7920_SendCmd(0x01);  // clear screen
	HAL_Delay(12);  // >10 ms delay


	ST7920_SendCmd(0x06);  // cursor increment right no shift
	HAL_Delay(1);  // 1ms delay

	ST7920_SendCmd(0x0C);  // D=1, C=0, B=0
    HAL_Delay(1);  // 1ms delay

	ST7920_SendCmd(0x02);  // return to home
	HAL_Delay(1);  // 1ms delay

}



// set Pixel

void SetPixel(uint8_t x, uint8_t y)
{
  if (y < numRows && x < numCols)
  {
    uint8_t *p = image + ((y * (numCols/8)) + (x/8));
    *p |= 0x80u >> (x%8);

  }

}

void ClearPixel(uint8_t x, uint8_t y)
{
  if (y < numRows && x < numCols)
  {
	  uint8_t *p = image + ((y * (numCols/8)) + (x/8));
	  *p &= (~(0x01u << x%8));
  }
}



/* draw a line
 * start point (X0, Y0)
 * end point (X1, Y1)
 */
void DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
  int dx = (x1 >= x0) ? x1 - x0 : x0 - x1;
  int dy = (y1 >= y0) ? y1 - y0 : y0 - y1;
  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx - dy;

  for (;;)
  {
    SetPixel(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = err + err;
    if (e2 > -dy)
    {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx)
    {
      err += dx;
      y0 += sy;
    }
  }
}




/* Draw rectangle
 * start point (x,y)
 * w -> width
 * h -> height
 */
void DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	/* Check input parameters */
	if (
		x >= numCols ||
		y >= numRows
	) {
		/* Return error */
		return;
	}

	/* Check width and height */
	if ((x + w) >= numCols) {
		w = numCols - x;
	}
	if ((y + h) >= numRows) {
		h = numRows - y;
	}

	/* Draw 4 lines */
	DrawLine(x, y, x + w, y);         /* Top line */
	DrawLine(x, y + h, x + w, y + h); /* Bottom line */
	DrawLine(x, y, x, y + h);         /* Left line */
	DrawLine(x + w, y, x + w, y + h); /* Right line */
}




/* Draw filled rectangle
 * Start point (x,y)
 * w -> width
 * h -> height
 */
void DrawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	uint8_t i;

	/* Check input parameters */
	if (
		x >= numCols ||
		y >= numRows
	) {
		/* Return error */
		return;
	}

	/* Check width and height */
	if ((x + w) >= numCols) {
		w = numCols - x;
	}
	if ((y + h) >= numRows) {
		h = numRows - y;
	}

	/* Draw lines */
	for (i = 0; i <= h; i++) {
		/* Draw lines */
		DrawLine(x, y + i, x + w, y + i);
	}
}




/* draw circle
 * centre (x0,y0)
 * radius = radius
 */
void DrawCircle(uint8_t x0, uint8_t y0, uint8_t radius)
{
  int f = 1 - (int)radius;
  int ddF_x = 1;

  int ddF_y = -2 * (int)radius;
  int x = 0;

  SetPixel(x0, y0 + radius);
  SetPixel(x0, y0 - radius);
  SetPixel(x0 + radius, y0);
  SetPixel(x0 - radius, y0);

  int y = radius;
  while(x < y)
  {
    if(f >= 0)
    {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    SetPixel(x0 + x, y0 + y);
    SetPixel(x0 - x, y0 + y);
    SetPixel(x0 + x, y0 - y);
    SetPixel(x0 - x, y0 - y);
    SetPixel(x0 + y, y0 + x);
    SetPixel(x0 - y, y0 + x);
    SetPixel(x0 + y, y0 - x);
    SetPixel(x0 - y, y0 - x);
  }
}


// Draw Filled Circle

void DrawFilledCircle(int16_t x0, int16_t y0, int16_t r)
{
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

    SetPixel(x0, y0 + r);
    SetPixel(x0, y0 - r);
    SetPixel(x0 + r, y0);
    SetPixel(x0 - r, y0);
    DrawLine(x0 - r, y0, x0 + r, y0);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        DrawLine(x0 - x, y0 + y, x0 + x, y0 + y);
        DrawLine(x0 + x, y0 - y, x0 - x, y0 - y);

        DrawLine(x0 + y, y0 + x, x0 - y, y0 + x);
        DrawLine(x0 + y, y0 - x, x0 - y, y0 - x);
    }
}



// Draw Traingle with coordimates (x1, y1), (x2, y2), (x3, y3)
void DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3)
{
	/* Draw lines */
	DrawLine(x1, y1, x2, y2);
	DrawLine(x2, y2, x3, y3);
	DrawLine(x3, y3, x1, y1);
}



// Draw Filled Traingle with coordimates (x1, y1), (x2, y2), (x3, y3)
void DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3)
{
	int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0,
	yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0,
	curpixel = 0;

#define ABS(x)   ((x) > 0 ? (x) : -(x))

	deltax = ABS(x2 - x1);
	deltay = ABS(y2 - y1);
	x = x1;
	y = y1;

	if (x2 >= x1) {
		xinc1 = 1;
		xinc2 = 1;
	} else {
		xinc1 = -1;
		xinc2 = -1;
	}

	if (y2 >= y1) {
		yinc1 = 1;
		yinc2 = 1;
	} else {
		yinc1 = -1;
		yinc2 = -1;
	}

	if (deltax >= deltay){
		xinc1 = 0;
		yinc2 = 0;
		den = deltax;
		num = deltax / 2;
		numadd = deltay;
		numpixels = deltax;
	} else {
		xinc2 = 0;
		yinc1 = 0;
		den = deltay;
		num = deltay / 2;
		numadd = deltax;
		numpixels = deltay;
	}

	for (curpixel = 0; curpixel <= numpixels; curpixel++)
	{
		DrawLine(x, y, x3, y3);

		num += numadd;
		if (num >= den) {
			num -= den;
			x += xinc1;
			y += yinc1;
		}
		x += xinc2;
		y += yinc2;
	}
}

//create function Sendchar_GLCD
void SendcharGLCD(uint8_t x, uint8_t y, char Character)
{

	uint8_t idx;
	uint8_t idy;
	uint8_t arr;
	//image[idx] = Font[Character - 32][idx];
		for( idx = 0 ; idx < 5; idx++)
		{
			arr = (uint8_t) Font[Character - 32][idx];
			//a=0x30;
			for (idy = 0; idy < 8 ; idy++)
			{
				if (arr & (1 << (7-idy)) )
				{
					SetPixel(x+idx, y+8-idy);

				}
//				else
//				{
//					ClearPixel(x+idx, y+8-idy);
//				}

			}
		}
}

void SendstringGLCD(uint8_t x, uint8_t y, char * string )
{
	while( *string)
	{
		SendcharGLCD(x, y, *string);
		string++;
		x=x+6;
	}
}



void Sendchar5_3GLCD(uint8_t x, uint8_t y, char character)
{
	uint8_t idx;
	uint8_t idy;
	uint8_t arr;
	for (idy = 0; idy < 5; idy++)
	{
		arr = Font3_5[character - '0'][idy];
		for (idx = 0; idx < 3; idx++)
		{
			if (arr & (0x01 << idx))
			{
				SetPixel(x+idx, y+idy);
			}
		}
	}
}

void Sendstring5_3GLCD(uint8_t x, uint8_t y, char* string)
{
	while(* string)
	{
		Sendchar5_3GLCD(x, y, *string);
		string++;
		x= x+4;
	}
}
void Sendchar7_9GLCD(uint16_t x, uint16_t y, char character)
{
	uint8_t idx;
	uint8_t idy;
	uint8_t arr;
	for (idy = 0; idy < 9; idy++)
	{
		arr = Font7x9[character - '0'][idy];
		for(idx = 0 ; idx < 7; idx++)
		{
			if(arr &(0x01 << idx))
			{
				SetPixel(x+idx, y+idy);
			}
		}
	}
}

void Sendstring7_9GLCD(uint8_t x, uint8_t y, char* string)
{
	while(* string)
	{
		Sendchar7_9GLCD(x, y, *string);
		string++;
		x= x+10;
	}
}
