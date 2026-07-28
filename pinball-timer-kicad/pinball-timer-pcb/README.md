# Pinball 4-Player Timer — Carrier PCB (rev A)

KiCad 8 project. 160 x 100 mm, 2-layer. Open `pinball-timer.kicad_pcb` in KiCad's
PCB editor (pcbnew). All footprints are placed and every pad is already assigned
to its net — the ratsnest (thin white lines) shows you exactly what to route.

## What's on the board

| Ref  | Part                          | Nets on pins                        |
|------|-------------------------------|-------------------------------------|
| J1   | JST-XH 4-pin, Display 1       | 1:+5V  2:GND  3:DISP1_CLK  4:DISP1_DIO |
| J2   | JST-XH 4-pin, Display 2       | 1:+5V  2:GND  3:DISP2_CLK  4:DISP2_DIO |
| J3   | JST-XH 4-pin, Display 3       | 1:+5V  2:GND  3:DISP3_CLK  4:DISP3_DIO |
| J4   | JST-XH 4-pin, Display 4       | 1:+5V  2:GND  3:DISP4_CLK  4:DISP4_DIO |
| J5   | JST-XH 2-pin, Player 1 button | 1:BTN_P1      2:GND                 |
| J6   | JST-XH 2-pin, Player 2 button | 1:BTN_P2      2:GND                 |
| J7   | JST-XH 2-pin, Player 3 button | 1:BTN_P3      2:GND                 |
| J8   | JST-XH 2-pin, Player 4 button | 1:BTN_P4      2:GND                 |
| J9   | JST-XH 2-pin, Action button   | 1:BTN_ACTION  2:GND                 |
| J10  | JST-XH 2-pin, Buzzer          | 1:BUZZER      2:GND                 |
| U1A/B| 2x 1x22 female headers (ESP32-S3 socket) | see GPIO map below       |
| H1-4 | M3 mounting holes             | —                                   |

## GPIO map (ESP32-S3 DevKitC-1 style pinout)

| Signal      | GPIO | Signal      | GPIO |
|-------------|------|-------------|------|
| BTN_P1      | 4    | DISP1_CLK   | 10   |
| BTN_P2      | 5    | DISP1_DIO   | 11   |
| BTN_P3      | 6    | DISP2_CLK   | 12   |
| BTN_P4      | 7    | DISP2_DIO   | 13   |
| BTN_ACTION  | 8    | DISP3_CLK   | 14   |
| BUZZER      | 9    | DISP3_DIO   | 15   |
|             |      | DISP4_CLK   | 16   |
|             |      | DISP4_DIO   | 17   |

Buttons use internal pull-ups (INPUT_PULLUP); pressed = LOW. No external
resistors on the board.

## VERIFY BEFORE ORDERING — do not skip

1. **Header row spacing.** U1A/U1B are placed 22.86 mm (0.9 in) apart, the
   common ESP32-S3 DevKitC-1 spacing. Clone boards vary. Measure YOUR Hosyond
   board center-to-center across the two pin rows and move U1B if needed
   (select footprint, press M, type exact coordinates).
2. **Pin order.** The net assignments assume the standard DevKitC-1 pinout
   (left row top-to-bottom: 3V3, 3V3, RST, 4, 5, 6, 7, 15, 16, 17, 18, 8, 3,
   46, 9, 10, 11, 12, 13, 14, 5V, GND). Check the silkscreen on your actual
   board pin-by-pin. If your board differs, re-assign pad nets in KiCad
   (edit pad -> net) — 5 minutes of checking vs. a scrapped board order.
3. **Display voltage.** Display VCC is routed to the module's 5V pin, which is
   fed by the battery shield through the module's USB port. TM1637 accepts
   3.3 V logic at 5 V supply — standard practice, no level shifter needed.

## Routing (30-60 min by hand, or minutes with autorouter)

1. Open the .kicad_pcb, confirm footprint positions.
2. Route GND and +5V first at 0.6 mm (Power netclass is preconfigured), or
   skip and pour a GND zone on B.Cu + a +5V zone section instead.
3. Route the 14 signal nets at 0.3 mm. There is enormous room; nothing is tight.
4. Run DRC (ladybug icon). Zero errors required.

Optional autorouting: File -> Export -> Specctra DSN, run Freerouting
(freerouting.org), import the .ses back.

## Export Gerbers for PCBWay

1. File -> Fabrication Outputs -> Gerbers.
   Layers: F.Cu, B.Cu, F.SilkS, B.SilkS, F.Mask, B.Mask, Edge.Cuts.
   Format defaults are already set in this project. Click Plot.
2. Same dialog -> Generate Drill Files (Excellon, PTH+NPTH merged is fine).
3. Zip the gerbers/ folder, upload to pcbway.com instant quote.
   Board settings: 2 layers, 160 x 100 mm, 1.6 mm FR4, HASL, 5 pcs.

## Mating connector shopping (for the wire harness)

- JST-XH 2.54... (2.5 mm) housing + crimp terminal kits are sold as assortments;
  get a kit with 2-pin and 4-pin housings and pre-crimped wires if you don't
  own a crimper.
