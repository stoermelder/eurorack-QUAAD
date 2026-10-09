# QUAAD
**quad 4-step Eurorack CV sequencer**

## This fork: firmware differences

The hardware (PCB V1.1, Panel V1.1) is unchanged. The firmware in `arduino_code/` was rewritten and extended:

- **Modes:** besides the original behaviour there are two chaining modes, selected with a shift gesture on the reset button and the division knobs (see [Modes](#modes) below).
- **12 patterns** instead of 6, with 6-8 step patterns and two random patterns (see the table under Features). The pattern CV now wraps around over the whole ADC range.
- **Shorter interrupt:** the timer interrupt only detects clock/reset edges and drives the outputs. All analog reads happen in `loop()`, so clock and reset edges are no longer missed or delayed.
- **Division hysteresis** on the division knobs, so a knob near a slot boundary no longer flickers between two divisions.
- **Gate mute and deferred division:** the division knob has an extra end stop beyond ÷32 that mutes the channel's gate output. The sequencer keeps running silently, so the channel stays in time, and the CV and step LED still follow the steps. The knob now has 11 positions (off, ÷32 ... ÷1). A new position is only applied once the knob has rested there for about 0.5 s, so turning the knob through all divisions does not run through them.
- **Code structure:** split into small modules in `arduino_code/src/` (`sequencer`, `channels`, `modes`, `inputs`, `shift`) instead of a single `.ino`, and built with PlatformIO (`arduino_code/platformio.ini`) instead of the Arduino IDE.

### Modes

The module has three modes. The active mode is stored in the EEPROM and restored at power-up.

| Mode | Behaviour |
|---|---|
| **Normal** (default) | Four independent sequencers, all clocked by the clock input. |
| **Chain pairs** | B is chained to A and D is chained to C. A and C are clocked by the clock input as usual; B and D are clocked by the completed sequences of A and C instead. |
| **Chain all** | All channels are chained: A > B > C > D. Only A follows the clock input; B advances with the sequences of A, C with those of B, and D with those of C. |

In the chain modes, a chained channel still uses its own division knob: set to 1 it advances one step on every sequence of the channel before it, set to 2 only on every second sequence, and so on. A sequence starts when the channel before it plays its first step, so after a reset the whole chain starts on the first clock. Chaining the channels gives long cycles, for example 4 x 4 x 4 x 4 = 256 clocks with all divisions at 1.

To change the mode, hold the reset button for about one second (this acts as "shift"), then turn the clock division knob of the channel that selects the mode:

| Division knob | Mode |
|---|---|
| A | Normal |
| B | Chain pairs |
| C | Chain all |

While shift is held, the division knobs do not change any division, and a knob has to be turned a fair bit before it counts. When you release the button, the mode is saved. A knob that you turned keeps its previous division until you turn it back through the position it was at (pickup), so the division does not jump. If you turn no knob, nothing changes, so a long gate on the reset jack cannot change the mode by accident. There is no display for the mode, so listen to the outputs to check which one is active.

### Tuning the knobs

The knobs are not linear, so where each pattern and each division starts on the knob is set in tables in `arduino_code/src/calibration.h`: `patternStart[]` and `divisionStart[]` list the lowest ADC reading (0-1023) at which each position starts. If a position starts too early (the knob points to 3 but the division is 2), raise its entry, and the ones after it if you want to keep their widths; if it starts too late, lower it. This is the only place that needs to change for different pots or panel marks.

- **Division knob** (off, ÷32 ... ÷1): the "off" stop at the low end is narrow on purpose, and ÷1 only gets the last few counts at the top of the travel. The ADC cannot read above 1023, so the ÷1 entry must stay below what a fully turned knob really reads. `DIV_SETTLE_MS` in `arduino_code/src/inputs.cpp` sets how long the knob must rest before a new division is applied.
- **Pattern knob** (12 patterns): the last entry of `patternStart[]` is where the list repeats (master knob, more CV).

### Sequencer

This module is basically 4 independent 4-step sequencers. Each of them features a CV output (0-5V), a clock divider (1-2-3-4-5-7-8-12-16-32 divisions) and a pattern setting.
There are 12 patterns available. Patterns can be changed per-sequence via a dedicated knob or via CV, aswell as globally using the MSTR PTRN knob, which basically adds a CV offset to each of the pattern CV inputs. The pattern knob alone covers all 12 patterns (about 0.4 V of the summed pattern input per pattern, so 0-5 V of CV covers them too); the position of each pattern on the knob is set in a table in `arduino_code/src/calibration.h` (`patternStart[]`), see below. The patterns are looped, so that when, for example, you are on the last of the 12 patterns and add some positive CV offset, you will go back to the first one and then further on...

| # | Pattern | Steps |
|---|---|---|
| 1 | up | 1-2-3-4 |
| 2 | | 1-2-4-3 |
| 3 | | 2-4-1-3 |
| 4 | | 3-4-2-1 |
| 5 | | 4-2-3-1 |
| 6 | down | 4-3-2-1 |
| 7 | ping-pong | 1-2-3-4-3-2 |
| 8 | reverse ping-pong | 4-3-2-1-2-3 |
| 9 | repeats | 1-1-2-2-3-3-4-4 |
| 10 | home base | 1-2-1-3-1-4 |
| 11 | random | a random step on every clock |
| 12 | random walk | moves one step up or down at random and turns around at the ends (no jump between 4 and 1) |

Patterns 1-6 are the original patterns: no step order repeats between them. The step lists are defined in `patterns[]` in `arduino_code/src/sequencer.cpp`. A sequence is as long as its pattern (4, 6 or 8 steps); the two random patterns count as 4 steps when channels are chained.

The sequencer can be reset via a gate or manually using a button.
With nothing patched into the clock input, the module is clocked internally, the tempo can be set via a potentiometer. The clock output outputs this internal clock when no external clock input is patched in, otherwise, the incoming clock signal is buffered through this output.

## Hardware

The module is 22HP, no 5V source required from the rack.

![1](https://user-images.githubusercontent.com/66487560/161437107-493f1656-3058-4b45-add9-1960b430ef7d.jpg)

![2](https://user-images.githubusercontent.com/66487560/161437244-39107ae3-6e29-4cdc-b105-9865ff89313c.jpg)

Not much to say here, the build is fairly straight forward. It features SMD components, the passives are 0603. 

## Firmware

Complete step-by-step flashing guide available in the pdf file in this repository.

The Atmega328P is running on Arduino code - this means an Arduino bootloader needs to be flashed before the .ino code. To flash it, you can use an arduino (uno or nano for example) and a couple of jumper wires. I recommend to flash the module disconnected from the rack - the 5V needed to run the chip can be provided via the ICSP header. I used a USBASP from aliexpress, it works great but was quite hard to get working, so I cannot recommend it. If you do go down the USBASP route, note that you need to place a jumper on the programmer that slows down the data transfer frequency - a factory fresh atmega328p won't be able to accept the bootloader at USBASP default speed.

Here's the pinout of the ICSP header used for flashing the chip:

![image](https://user-images.githubusercontent.com/66487560/161437515-88f70fad-4fca-49ac-b76e-4368312f6c80.png)

Finally, TX and RX pins are available on the back of the module. These can theoretically be used to debug the module, and could prove useful for testing new firmware. However, clock div out C and D have to be disabled in such case.

## License

Orginal work at [https://github.com/mzourack/QUAAD](https://github.com/mzourack/QUAAD) by MŽOURACK.

Shield: [![CC BY 4.0][cc-by-shield]][cc-by]

This work is licensed under a
[Creative Commons Attribution 4.0 International License][cc-by].

[![CC BY 4.0][cc-by-image]][cc-by]

[cc-by]: http://creativecommons.org/licenses/by/4.0/
[cc-by-image]: https://i.creativecommons.org/l/by/4.0/88x31.png
[cc-by-shield]: https://img.shields.io/badge/License-CC%20BY%204.0-lightgrey.svg
