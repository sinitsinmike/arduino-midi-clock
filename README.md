# Arduino MIDI Clock с Tap Tempo

## НОВОЕ

### Краткая инструкция

**Arduino Nano, MIDI OUT на D1/TX0 — НЕ ОТКРЫВАЙТЕ Serial Monitor во время работы устройства.**

- **TAP, короткие нажатия:** задать BPM (не менее 3 тапов).
- **TAP ≥ 5 секунд:** переключение `ClockOnlyWhenPlay` (`CALL ↔ CPLY`), состояние сохраняется в EEPROM.
- **A1, короткое нажатие:** `Start / Stop` одной кнопкой.
- **A1 ≥ 3 секунд:** калибровка центра нуджа (`CAL`).
- **TAP + A1 ≥ 4 секунд:** `LOCK / UNLK` нуджа, состояние сохраняется в EEPROM.
- **(Опция) Фаза:** мигает средний сегмент (`SEG_G`) у первой цифры слева один раз за такт.

---

## MIDI CLOCK — V3.3

**Файл:** `MIDI_CLOCK_V3_3.ino`  
**FW_VERSION:** `V3.3`  
**Build:** `__DATE__ __TIME__`

### Переключатели компиляции

По умолчанию все дополнительные функции отключены:

```cpp
// #define ENABLE_SYNC 1
// SYNC OUT + меню делителя/полярности

// #define ENABLE_A1_DOUBLECLICK 1
// Двойной клик A1 = Restart from bar 1 (SPP=0 + Start)

// #define ENABLE_BEAT_SEG 1
// Мигание среднего сегмента G у первой цифры слева один раз за такт

// #define BEAT_SEG_ONLY_WHEN_PLAY 1
// Если включить — фазовый индикатор мигает только в режиме PLAY
```

---

# О проекте

Arduino MIDI Clock с функцией Tap Tempo.

Проект представляет собой компактный MIDI Master Clock для синхронизации синтезаторов, драм-машин, секвенсоров и другого MIDI-оборудования.

Устройство позволяет задавать темп с помощью TAP-кнопки, изменять BPM потенциометром, управлять MIDI Start/Stop и отображать текущий BPM на дисплее TM1637.

---

# Подключения

## MIDI OUT

MIDI OUT используется на:

```text
D1 / TX0
```

Для стандартного MIDI OUT необходимо использовать соответствующую MIDI-схему с DIN-5 и резисторами.

> **Важно:** D1/TX0 используется для MIDI. Не открывайте Serial Monitor во время работы устройства, поскольку вывод Serial может конфликтовать с MIDI-передачей.

Для MIDI OUT:

- подключите MIDI GND к GND Arduino;
- подключите питание MIDI согласно стандартной MIDI-схеме;
- используйте резисторы согласно MIDI Electrical Specification;
- MIDI сигнал передаётся через D1/TX0.

---

## TAP

Кнопка TAP подключается к:

```text
D2
```

---

## BPM Nudge

Потенциометр подключается к:

```text
A0
```

Он используется для изменения BPM.

---

## Start / Stop

Кнопка управления Start/Stop подключается к:

```text
A1
```

Одна кнопка используется для переключения между PLAY и STOP.

---

## Tempo LED

Индикатор темпа подключается к:

```text
D5
```

---

## TM1637

Для отображения BPM используется 4-разрядный дисплей TM1637.

---

# Использование

После включения Arduino начинает генерировать MIDI Clock с темпом **100 BPM**, либо с последним сохранённым значением BPM, если оно было сохранено в EEPROM.

### Tap Tempo

Сделайте минимум **3 коротких нажатия TAP**.

После последнего тапа устройство рассчитывает темп по интервалам между нажатиями и устанавливает новый BPM.

---

# Основные функции

- **MIDI Clock** — выход на D1/TX0.
- **Tap Tempo** — установка BPM по TAP.
- **BPM Nudge** — изменение темпа потенциометром на A0.
- **Tempo LED** — светодиодный индикатор темпа на D5.
- **MIDI Start / Stop** — управление с помощью одной кнопки на A1.
- **BPM EEPROM** — сохранение BPM между включениями.
- **LOCK / UNLK** — блокировка влияния потенциометра на BPM.
- **Clock Only When PLAY** — передача MIDI Clock только во время PLAY.
- **TM1637 Display** — отображение BPM и состояния устройства.
- **Опциональный SYNC OUT** — включается отдельно через compile-time switch.
- **Опциональный Restart from bar 1** — SPP=0 + MIDI Start по двойному клику A1.
- **Опциональный фазовый индикатор** — мигание сегмента G один раз за такт.

---

# Управление

## TAP

### Короткие нажатия

Минимум 3 тапа:

```text
TAP → TAP → TAP
```

Устройство рассчитывает BPM и обновляет MIDI Clock.

### TAP ≥ 5 секунд

Переключает:

```text
CALL ↔ CPLY
```

где:

- `CALL` — Clock Always, MIDI Clock передаётся постоянно;
- `CPLY` — Clock Only When PLAY, MIDI Clock передаётся только во время PLAY.

Настройка сохраняется в EEPROM.

---

## A1

### Короткое нажатие

Переключает:

```text
STOP → PLAY
PLAY → STOP
```

При этом передаются MIDI Real-Time команды:

```text
START
STOP
```

### A1 ≥ 3 секунд

Запускает калибровку центра потенциометра.

На дисплее появляется:

```text
CAL
```

---

## TAP + A1 ≥ 4 секунд

Переключает блокировку потенциометра:

```text
LOCK ↔ UNLK
```

### LOCK

Потенциометр больше не влияет на BPM.

Это защищает темп от случайного вращения ручки.

### UNLK

Влияние потенциометра на BPM снова включается.

Состояние сохраняется в EEPROM.

---

# Опциональные функции

## SYNC OUT

По умолчанию отключён:

```cpp
// #define ENABLE_SYNC 1
```

После раскомментирования добавляются:

- SYNC OUT;
- меню SYNC;
- выбор делителя;
- инверсия полярности;
- сохранение настроек SYNC в EEPROM.

Доступные делители:

```text
1/2
1/4
1/8
1/16
```

Ширина SYNC импульса:

```text
15 мс
```

---

## Restart from bar 1

По умолчанию отключён:

```cpp
// #define ENABLE_A1_DOUBLECLICK 1
```

После включения двойной клик A1 выполняет:

```text
SPP = 0
+
MIDI Start
```

Это позволяет запустить воспроизведение с начала.

---

## Фазовый индикатор

По умолчанию отключён:

```cpp
// #define ENABLE_BEAT_SEG 1
```

При включении один раз за такт мигает средний сегмент:

```text
SEG_G
```

у первой цифры слева.

Дополнительно можно включить:

```cpp
// #define BEAT_SEG_ONLY_WHEN_PLAY 1
```

Тогда фазовый индикатор будет работать только во время PLAY.

---

# EEPROM

Устройство сохраняет настройки в EEPROM.

В зависимости от конфигурации могут сохраняться:

- BPM;
- центр потенциометра;
- состояние `LOCK / UNLK`;
- состояние `Clock Only When PLAY`;
- настройки SYNC.

Для уменьшения износа EEPROM запись выполняется только тогда, когда значение действительно изменилось.

---

# MIDI

MIDI OUT использует аппаратный UART Arduino:

```text
D1 / TX0
```

Стандартная скорость MIDI:

```text
31250 baud
```

MIDI Timing Clock работает с частотой:

```text
24 MIDI Clock ticks на четверть ноты
```

> **Не открывайте Serial Monitor во время работы устройства. D1/TX0 используется для MIDI OUT.**

---

# Старый проект

Первоначальная версия проекта была опубликована на LittleBits:

http://littlebits.cc/projects/littlebits-arduino-midi-master-clock-with-tap-tempo

Проект также можно реализовать на обычном Arduino, однако потребуется дополнительная работа с подключением кнопок и потенциометра.

---

# Как сделать MIDI-разъём для Arduino

Полезное руководство по созданию MIDI-разъёма для Arduino:

http://www.instructables.com/id/Send-and-Receive-MIDI-with-Arduino/step3/Send-MIDI-Messages-with-Arduino-Hardware/

Также можно ознакомиться с электрической спецификацией MIDI:

https://www.midi.org/articles/midi-electrical-specifications

---

# Старые ветки проекта

## master

Версия для Arduino Leonardo / LittleBits Arduino.

## arduino-uno

Версия для Arduino Uno и совместимых устройств.

D0/D1 используются для MIDI RX/TX, поэтому отладочная консоль недоступна.

## olimex-midi-shield

Версия на основе Arduino Uno с изменениями для работы с MIDI Shield от Olimex.

Проект находится в стадии разработки.

---

# GitHub

Исходный код проекта:

https://github.com/sinitsinmike/arduino-midi-clock

---

# Автор

**Michael Sinitsin**

📧 sinitsinmike@yahoo.com  
📧 michaelsinitsin@mail.ru

📞 +7 (985) 604-10-70

Telegram: **@Miharussian**

Telegram группа: **@SamodelnieSintezotory**

VK:  
https://vk.com/michaelsinitsin

Avito:  
https://www.avito.ru/brands/9d6afa796500fd0747cbfbc1f8305452

---

© 2026 Michael Sinitsin

# Arduino MIDI clock with tap tempo
NEW:

 * Quick guide (Arduino Nano, MIDI out on D1/TX0 — НЕ открывайте Serial Monitor):
 * - TAP короткие: задать BPM (≥3 тапа).
 * - TAP ≥5 c: ClockOnlyWhenPlay (CALL ↔ CPLY), сохраняется в EEPROM.
 * - A1 короткое: Start/Stop (одна кнопка).
 * - A1 ≥3 c: калибровка центра нуджа ("CAL").
 * - TAP + A1 ≥4 c: LOCK/UNLK нудж, сохраняется в EEPROM.
 * - (опция) Фаза: мигает средний сегмент (SEG_G) у 1-й слева цифры раз в такт.
  
  
 * ======================= MIDI CLOCK — V3.3 (Beat on left-G segment opt.) ======================= *
 * File: MIDI_CLOCK_V3_3.ino
 * FW_VERSION: "V3.3"   Build: __DATE__ __TIME__
 *
 * Compile-time switches (по умолчанию ОТКЛЮЧЕНО):
 *   // #define ENABLE_SYNC 1               // SYNC-выход + меню делителя/полярности
 *   // #define ENABLE_A1_DOUBLECLICK 1     // A1 double-click = Restart from bar 1 (SPP=0+Start)
 *   // #define ENABLE_BEAT_SEG 1           // мигание сегмента G у левой цифры раз в такт
 *   // #define BEAT_SEG_ONLY_WHEN_PLAY 1   // если включить — мигает ТОЛЬКО в PLAY



   




OLD:

As seen on LittleBits:
http://littlebits.cc/projects/littlebits-arduino-midi-master-clock-with-tap-tempo

You can also do this with a regular Arduino, but you'll have some extra work soldering the button(s)/dimmer

## How to make a Arduino MIDI connector
See the excellent tutorial at this link:
http://www.instructables.com/id/Send-and-Receive-MIDI-with-Arduino/step3/Send-MIDI-Messages-with-Arduino-Hardware/ .

Or check the electrical spec here:
https://www.midi.org/articles/midi-electrical-specifications

## Connections

### For MIDI out

- Connect MIDI ground to Arduino ground
- Connect MIDI 5V to Arduino 5V **with a 220 Ohm resistor in between**
- Connect MIDI signal line to Arduino serial input pin (D1)

### For tap in

Connect a button to D2

## Usage

- Upon startup, Arduino will start sending 100 BPM MIDI clock signal (or last saved BPM value if available).
- Tap the tempo (minimum 3 times)
- After the last tap, clock tempo will be updated and MIDI clock signal will send new BPM

### Features:
- **MIDI clock output** on pin D1 TX
- **Tap tempo** input
- **Dimmer input** when a dimmer is connected to A0 to set the tempo by twisting the knob!
- Tempo blinking **LED** on pin 5
- Sync signal on pin 9 (for example to sync with Korg Monotribe...)
- **MIDI real-time start/stop** is sent when button press is detected on A1 port
- **BPM storage in EEPROM** and restores it on power up
- **MIDI forwarding** if a MIDI input is present on pin D0 RX
- Output of the BPM to a TM1637 **LED display**


