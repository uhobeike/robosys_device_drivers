#include <fstream>
#include <iostream>
#include <string>
#include "unistd.h"

#define FILE_LCD10          "/dev/lcd_row_10"
#define FILE_LCD20          "/dev/lcd0_row_20"
#define FILE_LCD_CLEAR      "/dev/lcd_clear0"
#define FILE_ANALOG_READ    "/dev/analog_read0"
#define FILE_LED_BLINK0     "/dev/led_blink0"
#define FILE_LED_BLINK1     "/dev/led_blink1"
#define FILE_LED_BLINK2     "/dev/led_blink2"

#define BUFF_SIZE 256

int analog_read()
{
    std::ifstream ifs(FILE_ANALOG_READ);
    std::string str;

    if (ifs.fail()){
        std::cerr << "Failed to open file." << std::endl;
        return -1;
    }

    while (getline(ifs, str)) {
        std::cout << str << std::endl;
        usleep(1000);//スリープ入れないと負荷率高くて音が出る
    }
}

int main(void) 
{
    analog_read();

    return 0;
}
