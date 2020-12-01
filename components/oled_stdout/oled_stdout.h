#ifndef OLED_STDOUT_H
#define OLED_STDOUT_H

#include <esp_system.h>
#include <esp_log.h>

void lcd_putchar(int col, int row, char c);
void lcd_puts(int col, int row, const char *s);
int lcd_write(void *cookie, const char *buf, int n);
void lcd_gotoxy(int col, int row);
void lcd_clear(void);
esp_err_t lcd_init(int port);

#endif // OLED_STDOUT_H
// vim: set sw=4 ts=4 indk= et si:
