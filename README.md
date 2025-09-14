# LS020 Linux Framebuffer Driver

Драйвер ядра Linux для TFT LCD дисплея LS020 от телефона Siemens S65.

## Технические характеристики

- **Разрешение**: 176 x 132 пикселя
- **Глубина цвета**: 16 бит (RGB565)
- **Интерфейс**: SPI
- **Напряжение питания**: 3.3V (совместимо с 2.9V)
- **Подсветка**: 9-12V (опционально)

## Распиновка LS020

```
Пин  | Сигнал | Описание
-----|--------|----------
1    | LED-   | Катод подсветки
2    | LED+   | Анод подсветки (9-12V)
3    | NC     | Не подключен
4    | GND    | Земля
5    | VCC    | Питание +2.9V (3.3V работает)
6    | MOSI   | SPI Data (Master Out Slave In)
7    | SCL    | SPI Clock
8    | CS     | Chip Select (активный низкий)
9    | RST    | Reset (активный низкий)
10   | RS/DC  | Register Select / Data Command
```

## Подключение к Orange Pi Zero 2W

| LS020 | Orange Pi | GPIO  | Функция |
|-------|-----------|-------|---------|
| 6     | PH7       | -     | SPI1_MOSI |
| 7     | PH6       | -     | SPI1_CLK |
| 8     | PH5       | -     | SPI1_CS0 |
| 9     | PI14      | 270   | RST |
| 10    | PH4       | 228   | RS/DC |
| 5     | 3.3V      | -     | VCC |
| 4     | GND       | -     | GND |

## Установка

1. **Клонирование и сборка:**
   ```bash
   cd /home/orangepi/ls020_driver
   sudo ./install.sh
   ```

2. **Ручная установка:**
   ```bash
   # Компиляция драйвера
   make

   # Компиляция Device Tree Overlay
   dtc -@ -I dts -O dtb -o ls020-overlay.dtbo ls020-overlay.dts

   # Установка драйвера
   sudo make install

   # Копирование overlay
   sudo cp ls020-overlay.dtbo /boot/overlays/
   ```

3. **Настройка загрузки:**
   
   Добавьте в `/boot/armbianEnv.txt`:
   ```
   overlays=ls020-overlay
   ```
   
   Или в `/boot/config.txt`:
   ```
   dtoverlay=ls020-overlay
   ```

4. **Перезагрузка:**
   ```bash
   sudo reboot
   ```

## Использование

1. **Загрузка модуля:**
   ```bash
   sudo modprobe ls020_fb
   ```

2. **Проверка:**
   ```bash
   ls /dev/fb*  # должен появиться /dev/fb1
   dmesg | grep ls020  # проверить логи
   ```

3. **Основные операции:**
   ```bash
   # Включить дисплей
   echo 0 > /sys/class/graphics/fb1/blank
   
   # Заливка цветом (красный)
   dd if=/dev/zero bs=46464 count=1 | tr '\000' '\377' > /dev/fb1
   
   # Случайная картинка
   cat /dev/urandom > /dev/fb1
   
   # Очистка (черный)
   dd if=/dev/zero of=/dev/fb1 bs=46464 count=1
   ```

## Программирование

### Пример на C

```c
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#define FB_WIDTH 176
#define FB_HEIGHT 132

int main() {
    int fd = open("/dev/fb1", O_RDWR);
    uint16_t *fb = mmap(NULL, FB_WIDTH * FB_HEIGHT * 2, 
                        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    // Заливка красным цветом (RGB565: 0xF800)
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb[i] = 0xF800;
    }
    
    munmap(fb, FB_WIDTH * FB_HEIGHT * 2);
    close(fd);
    return 0;
}
```

### Пример на Python

```python
import mmap
import struct

FB_WIDTH = 176
FB_HEIGHT = 132

with open('/dev/fb1', 'r+b') as f:
    with mmap.mmap(f.fileno(), FB_WIDTH * FB_HEIGHT * 2) as fb:
        # Заливка синим цветом (RGB565: 0x001F)
        for i in range(0, len(fb), 2):
            fb[i:i+2] = struct.pack('<H', 0x001F)
```

## Цветовая схема RGB565

| Цвет    | Hex    | RGB      |
|---------|--------|----------|
| Черный  | 0x0000 | 0,0,0    |
| Красный | 0xF800 | 255,0,0  |
| Зеленый | 0x07E0 | 0,255,0  |
| Синий   | 0x001F | 0,0,255  |
| Белый   | 0xFFFF | 255,255,255 |

## Отладка

1. **Проверка SPI:**
   ```bash
   ls /sys/class/spi_master/
   cat /sys/class/spi_master/spi1/device/modalias
   ```

2. **Проверка GPIO:**
   ```bash
   sudo cat /sys/kernel/debug/gpio
   ```

3. **Логи драйвера:**
   ```bash
   dmesg | grep ls020
   journalctl -f | grep ls020
   ```

4. **Удаление модуля:**
   ```bash
   sudo modprobe -r ls020_fb
   ```

## Известные проблемы

1. **Медленная работа** - SPI работает на максимальной скорости 32MHz, но может потребоваться снижение
2. **Мерцание** - используется deferred I/O с частотой 25 FPS
3. **Ориентация** - по умолчанию поворот 0, может потребоваться изменение

## Настройка производительности

В коде драйвера можно изменить:
- `spi->max_speed_hz` для скорости SPI
- `ls020_defio.delay = HZ / FPS` для частоты обновления
- `par->orientation` для поворота дисплея

## Лицензия

GPL v2 - совместимо с ядром Linux

## Автор

Основано на Arduino библиотеке Yaroslav Kashapov (@kashapovd)
Адаптировано для Linux kernel framebuffer