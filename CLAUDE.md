# CLAUDE.md — Контекст проекта для Claude

Этот файл описывает состояние проекта и важные технические детали для будущих сессий.

---

## О проекте

Zigbee-сенсор CO2 на базе **ESP32-C6 SuperMini** + **Sensirion SCD-41** + **LiPo аккумулятор**.
Интегрируется с **Home Assistant** через ZHA. Собирается на **ESP-IDF 5.3+** с Zigbee SDK.

Репозиторий — форк [florianL21/zigbee-co2-sensor](https://github.com/florianL21/zigbee-co2-sensor).

---

## Железо

| Компонент | Модель | Примечания |
| --------- | ------ | ---------- |
| Контроллер | ESP32-C6 SuperMini | Встроенный контроллер заряда LiPo, B+/B- пады |
| Сенсор CO2 | Sensirion SCD-41 (breakout) | I2C: GPIO19 (SDA), GPIO20 (SCL) |
| Аккумулятор | LiPo 1500 mAh | Подключён к B+/B- |
| LED | WS2812B на GPIO8 | Отключается на старте (`turn_off_onboard_led`) |

**Механическая сборка:** SCD-41 breakout подключается напрямую через контакты (rigid connection),
без гибких проводов — только АКБ провода и резисторы делителя.

**Тестовый образец:** с INA226 (CJMCU-226) на I2C 0x40 — только для измерения реального
потребления, в продакшн не идёт.

---

## Структура кода

- [main/main.c](main/main.c) — точка входа, инициализация
- [main/config.h](main/config.h) — все настраиваемые константы (пины, интервалы, ADC)
- [main/co2_task.c](main/co2_task.c) — задача измерения SCD-41
- [main/zigbee.c](main/zigbee.c) — Zigbee инициализация и кластеры
- [main/zigbee_task.c](main/zigbee_task.c) — основной Zigbee цикл
- [main/idf_component.yml](main/idf_component.yml) — зависимости (led_strip pinned ~2.5.0)
- [main/CMakeLists.txt](main/CMakeLists.txt) — REQUIRES: esp-idf-scd4x, nvs_flash, esp_adc, esp_timer
- [sdkconfig.defaults](sdkconfig.defaults) — `CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP=n`

---

## Текущее состояние (по состоянию на 2026-06-02)

### Работает

- Light sleep между измерениями — данные обновляются каждые ~5:20 в ZHA
- WS2812B LED выключается на старте
- CI (GitHub Actions) собирается без ошибок, деплоит на GitHub Pages автоматически
- **Мониторинг батареи** — делитель B+ → 1МОм → GPIO1 → 1МОм → GND, читает ~3960 mV, показывает ~79% в ZHA
- **Battery percentage reporting** — `esp_zb_zcl_update_reporting_info` вручную создаёт запись в таблице ZBOSS → ZHA получает push-уведомления по `battery_percentage_remaining` (0x0021)
- **Веб-прошивалка** — работает, деплоится автоматически при push в main

### Известные ограничения

- `battery_voltage` (0x0020) — не репортуется (READ_ONLY по ZCL-spec, флаг REPORTING отсутствует в ZBOSS). ZHA может только опросить значение вручную.
- Кнопка "Идентификация" в ZHA приходит, но LED не мигает — обработчик Identify не реализован.

### Планируется

- Deep sleep + MOSFET AO3401 для отключения питания SCD-41 (куплен)
- Сброс к заводским настройкам по долгому нажатию BOOT

---

## Важные технические нюансы

### USB Serial во время light sleep

На ESP32-C6 USB Serial/JTAG-периферия тоже уходит в сон → COM-порт временно исчезает с хоста.
Это нормально, не краш. Логи появятся снова при следующем просыпании.

### Zigbee NVS corruption

После изменения дескрипторов кластеров zb_storage-раздел несовместим → boot loop.
Лечится: `idf.py erase-flash` + повторное сопряжение с ZHA.

### led_strip компонент

В ESP-IDF 5.x `led_strip` вынесен в IDF Component Manager.

- Не добавлять в `CMakeLists REQUIRES`
- В `idf_component.yml`: `espressif/led_strip: "~2.5.0"` (v3.x сломала API — убрала `led_pixel_format`)

### Зарядка LiPo на SuperMini

Встроенный контроллер заряда (не TP4056). Зарядка через USB-C. Ограничение ~4 В.
Индикатор BAT горит во время зарядки. Перезаряд не грозит.

### Мониторинг батареи

Делитель: B+ → 1МОм → GPIO1 (ADC_CHANNEL_1) → 1МОм → GND. Коэффициент = 2.
Читает ~3960 mV, показывает ~79% в ZHA. **Работает.**

**Важно: GPIO0 — strapping pin!** Нельзя использовать для делителя напряжения:
резистор подтягивает GPIO0 к HIGH (~1.85 В) при старте → `boot:0x11 (SDIO_REI_FEO_V1_BOOT)` → устройство не грузится.
GPIO1 — безопасен, уже был в оригинальной конфигурации прошивки.

**ADC пересоздаётся на каждый вызов** `battery_read()` — повторное использование хэндлов через light sleep
вызывало стale-состояние и завышенные показания (>4200 mV → 100%). `adc_oneshot_new_unit` +
`adc_cali_create_scheme_curve_fitting` вызываются при каждом измерении, после чего удаляются.
Влияние на автономность нулевое (десятки мкс раз в 5 минут).

### Потребление в текущем режиме (light sleep)

Среднее ~800 мкА → с 1500 mAh: ~2.5 месяца. С deep sleep + MOSFET → значительно дольше.

### GPIO19/20 для I2C

GPIO19 (USB D+) и GPIO20 (UART0 RX) используются для I2C к SCD-41.
Выбраны намеренно: порядок пинов `GND–3V3–GPIO20–GPIO19` на SuperMini совпадает с
`GND–VCC–SCL–SDA` на SCD-41 breakout → прямое соединение через штырьковые контакты.
На батарейном устройстве USB не подключён → конфликтов нет.

---

## Сборка и прошивка

```bash
idf.py set-target esp32c6
idf.py build
idf.py flash monitor
```

При смене конфигурации кластеров — сначала `idf.py erase-flash`.
