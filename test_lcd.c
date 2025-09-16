#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>

#define FB_WIDTH 176
#define FB_HEIGHT 132
#define FB_SIZE (FB_WIDTH * FB_HEIGHT * 2)

// RGB565 цвета
#define COLOR_BLACK   0x0000
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_WHITE   0xFFFF
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void fill_screen(uint16_t *fb, uint16_t color) {
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb[i] = color;
    }
}

void draw_pixel(uint16_t *fb, int x, int y, uint16_t color) {
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        fb[y * FB_WIDTH + x] = color;
    }
}

void draw_line(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        draw_pixel(fb, x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color) {
    draw_line(fb, x, y, x + w - 1, y, color);
    draw_line(fb, x, y, x, y + h - 1, color);
    draw_line(fb, x + w - 1, y, x + w - 1, y + h - 1, color);
    draw_line(fb, x, y + h - 1, x + w - 1, y + h - 1, color);
}

void fill_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            draw_pixel(fb, x + i, y + j, color);
        }
    }
}

void test_colors(uint16_t *fb) {
    printf("Тест цветов...\n");
    
    uint16_t colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, 
                        COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA, COLOR_WHITE};
    char* color_names[] = {"Красный", "Зеленый", "Синий", 
                          "Желтый", "Циан", "Пурпурный", "Белый"};
    
    for (int i = 0; i < 7; i++) {
        printf("  %s\n", color_names[i]);
        fill_screen(fb, colors[i]);
        sleep(1);
    }
    
    fill_screen(fb, COLOR_BLACK);
}

void test_patterns(uint16_t *fb) {
    printf("Тест паттернов...\n");
    
    // Шахматная доска
    printf("  Шахматная доска\n");
    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            uint16_t color = ((x / 8) + (y / 8)) % 2 ? COLOR_WHITE : COLOR_BLACK;
            draw_pixel(fb, x, y, color);
        }
    }
    sleep(2);
    
    // Градиент
    printf("  Градиент\n");
    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            uint8_t intensity = (x * 255) / FB_WIDTH;
            uint16_t color = rgb_to_rgb565(intensity, intensity, intensity);
            draw_pixel(fb, x, y, color);
        }
    }
    sleep(2);
    
    fill_screen(fb, COLOR_BLACK);
}

void test_graphics(uint16_t *fb) {
    printf("Тест графики...\n");
    
    fill_screen(fb, COLOR_BLACK);
    
    // Прямоугольники
    fill_rect(fb, 10, 10, 50, 30, COLOR_RED);
    draw_rect(fb, 70, 10, 50, 30, COLOR_GREEN);
    fill_rect(fb, 130, 10, 40, 30, COLOR_BLUE);
    
    // Линии
    draw_line(fb, 0, 50, FB_WIDTH-1, 50, COLOR_YELLOW);
    draw_line(fb, 88, 0, 88, FB_HEIGHT-1, COLOR_CYAN);
    
    // Диагонали
    draw_line(fb, 0, 0, FB_WIDTH-1, FB_HEIGHT-1, COLOR_WHITE);
    draw_line(fb, 0, FB_HEIGHT-1, FB_WIDTH-1, 0, COLOR_MAGENTA);
    
    sleep(3);
    fill_screen(fb, COLOR_BLACK);
}

int main(int argc, char *argv[]) {
    printf("LS020 Framebuffer Test\n");
    printf("======================\n");
    
    // Открыть framebuffer
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        perror("Ошибка открытия /dev/fb0");
        printf("Убедитесь что:\n");
        printf("1. Драйвер загружен: sudo modprobe ls020_fb\n");
        printf("2. Device tree overlay установлен\n");
        printf("3. Дисплей подключен правильно\n");
        return 1;
    }
    
    // Маппинг памяти
    uint16_t *fb = mmap(NULL, FB_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
        perror("Ошибка mmap");
        close(fd);
        return 1;
    }
    
    printf("Framebuffer успешно открыт: %dx%d, %d байт\n", 
           FB_WIDTH, FB_HEIGHT, FB_SIZE);
    
    // Включить дисплей
    system("echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null");
    
    // Тесты
    if (argc > 1 && strcmp(argv[1], "colors") == 0) {
        test_colors(fb);
    } else if (argc > 1 && strcmp(argv[1], "patterns") == 0) {
        test_patterns(fb);
    } else if (argc > 1 && strcmp(argv[1], "graphics") == 0) {
        test_graphics(fb);
    } else {
        // Полный тест
        test_colors(fb);
        test_patterns(fb);
        test_graphics(fb);
    }
    
    printf("Тест завершен\n");
    
    // Очистка
    munmap(fb, FB_SIZE);
    close(fd);
    
    return 0;
}