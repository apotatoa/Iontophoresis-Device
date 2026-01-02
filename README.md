# Open Source Iontophoresis Device

## ⚠️ SAFETY WARNING: READ BEFORE BUILDING

> **DANGER:** This project involves the design and construction of a device that applies electrical current to the human body. Improper construction, component failure, or misuse can result in **electric shock, burns, injury, or death**.

* **Galvanic Isolation is mandatory:** Never bypass the isolation barriers designed into this PCB.
* **Battery Power Only:** This device is designed solely for battery operation.
* **Educational Use:** This project is for educational and research purposes only. It is not an FDA-approved medical device.
* **Liability:** The authors and contributors are not responsible for any harm caused by the assembly or use of this device. **Build and use at your own risk.**

---

## Project Overview

This repository contains the complete hardware, firmware, and mechanical design files for a **User-Controlled Iontophoresis Device** intended for the treatment of hyperhidrosis.

The system uses a regulated DC current (typically 0–30mA) applied through water baths to treat excessive sweating of the hands or feet. Unlike simple resistive devices, this project features active closed-loop current regulation, ensuring consistent treatment current regardless of water resistance or skin impedance changes.

To learn more about this project, check out this blog post on maker.io:
> Coming Soon: Link to DigiKey blog post on maker.io

## Key Features

* **Galvanic Isolation:** Physical and electrical separation between the control logic (ESP32) and the high-voltage treatment side using an isolated DC-DC converter (Ag7200) and optocouplers.
* **Active Current Control:** PWM-driven Op-Amp and MOSFET current sink maintains steady current.
* **Automatic Polarity Switching:** Integrated H-Bridge relays automatically reverse polarity halfway through treatment to ensure even effect on both limbs.
* **Safety Fuse:** Hardware-level protection in a quick-blow fuse that physically blocks the current path if it exceeds safety limits (independent of software).

## Repository Structure

```text
Iontophoresis-Device/
├── README.md         # Project documentation and safety warnings
├── 3d/
│   ├── CAD/          # .step files for the enclosure body and lid
│   └── blender/      # .blend files and .mp4 renders for visualization
├── firmware/
│   └── Ionto_Sketch.ino # Arduino source code for the ESP32-S3
└── hardware/
    ├── Ionto.kicad_pro  # KiCad 9.0 Project File
    ├── Ionto.kicad_sch  # Schematic File
    ├── Current_Regulator.kicad_sch    #heirarchal schematic for current regulation 
    ├── H-Bridge.kicad_sch             #heirarchal schematic for H-Bridge
    ├── Ionto.kicad_pcb  # 6-Layer PCB Layout File
    └── jlcpcb/          # Production files (Gerbers, BOM, CPL)
```

## Hardware Design (KiCad)

The PCB is a 6-layer board designed in KiCad 9.0. It utilizes a strict zonal separation layout to maintain safety standards.

* **Dirty Zone (Logic):** ESP32-S3-WROOM-1, USB-C (Programming/Charging), User Controls.
* **Isolation Bridge:** Ag7200 Module (Power) and Optocouplers (Signal).
* **Clean Zone (High Voltage):** Boosted 40V rail, LM358 Op-Amp, IRF540N MOSFET, Relays, and Output Banana Jacks.

### Critical Components
* **MCU:** ESP32-S3 Dev Module
* **Isolation:** Ag7200 (DC-DC), PC817 (Opto)
* **Power:** 11.1V 3S LiPo Battery
* **Current Regulation:** LM358 Op-Amp + IRF540N MOSFET

## Firmware (Arduino/ESP32)

The firmware handles the control loop, user interface, and treatment timing.

### Dependencies
* **Board:** ESP32S3 Dev Module
* **Libraries Required:**
    * `U8g2` (by oliver) - For the OLED Display menu system.
    * `MUIU8g2` - For the menu user interface.
    * `Wire` - Standard I2C library.

### Flashing Instructions
1.  Open `firmware/Ionto_Sketch.ino` in the Arduino IDE.
2.  Install the required libraries via the Library Manager.
3.  Select Board: **ESP32S3 Dev Module**.
4.  Upload to the device.

## Controls

* **Button 1 (Left):** Decrease Value
* **Button 2 (Center):** Increase Value
* **Button 3 (Right):** Enter & Next Field / Start / Stop

## Mechanical (3D & CAD)

The enclosure is designed to be 3D printed.

* **Files:** Located in `3d/CAD/`.
* **Material:** PETG or ABS is recommended for heat resistance and durability, though PLA is sufficient for prototyping.
* **Assembly:** The PCB mounts into the base using M3 screws. The display requires 3x 10.2mm M2 standoff.
* **Note:** Ensure non-conductive washers are used if mounting holes are near copper planes.

## Liability & Disclaimer

This hardware design and software are provided "as is" without warranty of any kind. The user assumes all liability for the assembly, testing, and operation of this device. This device is not certified by any regulatory body (FCC, CE, UL, FDA, etc.).

**By cloning this repository or building this device, you agree that you are solely responsible for your safety and the safety of others.**
