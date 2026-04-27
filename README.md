# Zigbee CO2 Сенсор с питанием от АКБ

Zigbee-устройство на базе ESP32-C6 для измерения CO2, температуры и влажности.
Питается от LiPo-аккумулятора, заряжается через USB-C. Автономность — до 9 месяцев.

---

> Форк проекта [florianL21/zigbee-co2-sensor](https://github.com/florianL21/zigbee-co2-sensor).
> Адаптирован под питание от аккумулятора и ESP32-C6 SuperMini.

---

## Возможности

- Измерение **CO2**, **температуры** и **влажности**
- Передача данных по **Zigbee** каждые 5 минут
- Интеграция с **Home Assistant** (ZHA)
- Питание от **LiPo 400 mAh** — до 8–9 месяцев без подзарядки
- Заряд через **USB-C**
- Лёгкий сон (light sleep) между измерениями для экономии энергии

## Железо

| Компонент | Модель |
| --------- | ------ |
| Контроллер | [ESP32-C6 SuperMini](https://www.aliexpress.com/item/1005006325592150.html) |
| Сенсор CO2 | [Adafruit SCD-41](https://www.adafruit.com/product/5190) |
| Аккумулятор | LiPo 502035p 3.7V 400 mAh |

## Подключение

| GPIO | Функция |
| ---- | ------- |
| GPIO22 | I2C SDA → SCD-41 |
| GPIO23 | I2C SCL → SCD-41 |

```text
SCD-41 (Adafruit)       ESP32-C6 SuperMini
      SDA        →         GPIO 22
      SCL        →         GPIO 23
      VIN        →         3.3V
      GND        →         GND
```

Аккумулятор подключается к пинам `B+` / `B-` на плате SuperMini.
Зарядка происходит автоматически при подключении USB-C.

## Прошивка

Прошить можно прямо в браузере (Chrome / Edge):

**[Открыть веб-прошивалку](https://djubb.github.io/zigbee-co2-sensor/)**

1. Подключите ESP32-C6 по USB
2. Нажмите кнопку «Install»
3. Выберите COM-порт устройства
4. Дождитесь окончания прошивки

Если устройство не определяется — зажмите кнопку **BOOT** перед подключением USB.

## Сборка из исходников

Требуется [ESP-IDF v5.3+](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html).

```bash
git clone --recursive https://github.com/djubb/zigbee-co2-sensor.git
cd zigbee-co2-sensor
idf.py set-target esp32c6
idf.py build
idf.py flash
```

## Подключение к Home Assistant

1. Перейдите в **Настройки → Устройства и службы → ZHA**
2. Нажмите **Добавить устройство**
3. Включите питание сенсора — он автоматически войдёт в режим сопряжения
4. После подключения будут доступны: CO2 (ppm), температура (°C), влажность (%)

## Сброс к заводским настройкам

Если нужно отвязать устройство от координатора:

- Удалите устройство из ZHA-интерфейса — сенсор перейдёт в режим ожидания сопряжения
- Либо используйте [веб-прошивалку](https://djubb.github.io/zigbee-co2-sensor/) с опцией **Erase device**

## Потребление энергии

| Режим | Ток |
| ----- | --- |
| Измерение (SCD-41 + радио) | ~20–100 мА (пик, ~5 сек) |
| Light sleep | < 1 мА |
| Среднее за сутки | **~54 мкА** |

С аккумулятором 400 mAh при среднем токе 64 мкА (с учётом КПД регулятора):

```text
400 mAh / 0.064 mA ≈ 6250 часов ≈ 260 дней ≈ 8–9 месяцев
```

## Лицензия

GPL-3.0 — см. [LICENSE](LICENSE).

Основано на [florianL21/zigbee-co2-sensor](https://github.com/florianL21/zigbee-co2-sensor),
который в свою очередь основан на [@xmow49/ESP32H2-Zigbee-Demo](https://github.com/xmow49/ESP32H2-Zigbee-Demo).
