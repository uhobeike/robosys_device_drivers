#include <fstream>
#include <iostream>
#include <string>
#include "unistd.h"

#define FILE_LCD_1          "/dev/lcd_row_10"
#define FILE_LCD_2          "/dev/lcd0_row_20"
#define FILE_LCD_CLEAR      "/dev/lcd_clear0"
#define FILE_ANALOG_READ    "/dev/analog_read0"
#define FILE_LED_BLINK0     "/dev/led_blink0"
#define FILE_LED_BLINK1     "/dev/led_blink1"
#define FILE_LED_BLINK2     "/dev/led_blink2"

#define ANALOG_RESOLUTION_MIN 0
#define ANALOG_RESOLUTION_MAX 4096

using std::string;
using std::stoi;
using std::ifstream;
using std::ofstream;
using std::cout;
using std::endl;

void lcd_clear()
{
    std::ofstream ofs(FILE_LCD_CLEAR);
    
    ofs << std::endl;
}

void lcd_write_10(string lcd_str)
{
    std::ofstream ofs(FILE_LCD_1);
    
    ofs << lcd_str;
}

void lcd_write_20(string lcd_str)
{
    std::ofstream ofs(FILE_LCD_2);
    
    ofs << lcd_str;
}

string analog_read()
{
    ifstream ifs(FILE_ANALOG_READ);
    string str;

    getline(ifs, str);

    return str;
}

void led_blink(char status_0, char status_1, char status_2)
{
    std::ofstream ofs_0(FILE_LED_BLINK0);
    std::ofstream ofs_1(FILE_LED_BLINK1);
    std::ofstream ofs_2(FILE_LED_BLINK2);

    if (status_0){
        ofs_0 << 1;
    } else if (!status_0){
        ofs_0 << 0;
    }

    if (status_1){
        ofs_1 << 1;
    } else if (!status_1){
        ofs_1 << 0;
    }

    if (status_2){
        ofs_2 << 1;
    } else if (!status_2){
        ofs_2 << 0;
    }
}

void led_control(string analog_val_str)
{
    int analog_val = stoi(analog_val_str);
    int led_threshold = ANALOG_RESOLUTION_MAX / 5;
    
    if (0 <= analog_val && 
            analog_val <= led_threshold){
        led_blink(0, 0, 0);
        lcd_clear();
        lcd_write_10("LED: 0");
        lcd_write_20("VAL:" + analog_val_str);
    } else if (led_threshold < analog_val && 
            analog_val <= (led_threshold * 2)){ 
        led_blink(1, 0, 0);
        lcd_clear();
        lcd_write_10("LED: 1");
        lcd_write_20("VAL:" + analog_val_str);
    } else if ((led_threshold * 2) < analog_val && 
            analog_val <= (led_threshold * 3)){
        led_blink(1, 1, 0);
        lcd_clear();
        lcd_write_10("LED: 2");
        lcd_write_20("VAL:" + analog_val_str);
    }
    else if ((led_threshold * 3) < analog_val && 
            analog_val <= (led_threshold * 4)){
        led_blink(1, 1, 1);
        lcd_clear();
        lcd_write_10("LED: 3");
        lcd_write_20("VAL:" + analog_val_str);
    }

    usleep(10000);
}

int main(void) 
{
    string lcd_str;

    /* lcdの初期化*/
    lcd_clear();

    while (true){
        led_control(analog_read());
    }
    return 0;
}
