#include "Display_SPD2010.h"
#include "I2C_Driver.h"
#include "TCA9554PWR.h"
#include "board_power.h"
void od_hardware_init(void){od_power_init();I2C_Init();ESP_ERROR_CHECK(EXIO_Init());LCD_Init();Set_Backlight(65);LVGL_Init();}
void od_hardware_loop(void){}

void od_hardware_backlight(int value){Set_Backlight(value);}
