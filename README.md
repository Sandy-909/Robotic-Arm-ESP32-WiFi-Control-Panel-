# Robotic-Arm-ESP32-WiFi-Control-Panel-
An open-source, 3D-printed robotic arm controlled wirelessly over WiFi from a browser-based control panel — no app install, no cables, no COM port required. Built from scratch: CAD → 3D printing → embedded firmware → custom web UI. Fully assembled, wired, and working.

<img width="630" height="1400" alt="ROBOTIC_ARM" src="https://github.com/user-attachments/assets/46fdf5dc-b0f0-4942-a93d-82c2852328a3" />

Overview

This project is a 5-axis robotic arm (Base, Shoulder, Elbow, Wrist Pitch, Wrist Roll, plus a Gripper) driven by an ESP32 microcontroller. The ESP32 hosts its own WiFi access point and serves a full control panel directly from the board — open the arm's IP address in any browser, no separate files, no app, no COM port. Every joint is controlled with a slider, with live angle readout, smooth per-joint speed-ramped movement, saved pose sequences, playback, and export/import.

All mechanical parts are 3D printed. All code — firmware and control panel — is included in this repo.

Features
6-channel servo control (5 arm joints + gripper)
Self-hosted browser control panel — served directly from the ESP32, just open its IP address
Live slider control per joint with real-time angle display
Smooth, per-joint speed-ramped movement — each joint moves in small steps rather than snapping instantly, with independently tunable speed per joint (heavier MG995 joints vs. lighter SG90 joints move differently)
Save Position — capture the current pose
Play Movements — play back saved poses in sequence with adjustable delay
Stop Movement — halt playback mid-sequence
Export / Import Positions — save and reload movement sequences as .json files
Reset Positions — return all joints to a safe default pose
No app installation — runs in any modern browser

## Hardware / Bill of Materials

| Component | Qty | Notes |
|---|---:|---|
| ESP32 Dev Board (NodeMCU-32S) | 1 | 5V input via onboard 5V/VIN pin |
| MG995 Servo (Positional, 180°) | 3 | Base, Shoulder, and Elbow joints |
| SG90 Servo | 3 | Wrist Pitch, Wrist Roll, and Gripper |
| 5V Input Power Supply (3A+) | 1 | Use a power adapter of 5V 3A  |
| DC Barrel Jack | 1 | Center-positive (tip = +, sleeve = −) |
| 3D Printed Parts | — | See `/cad` folder for STL and source files |
| M3 Screws & nuts | as required  | See CAD assembly  |
| Jumper Wires | — | Used for signal and power distribution |
⚠️ Important: Make sure any MG995 you buy is listed as 180° positional, not "continuous rotation" or "for robot wheels" — these look identical but behave completely differently. See Lessons Learned below.

### Wiring

<img width="3000" height="2208" alt="circuit_image" src="https://github.com/user-attachments/assets/ac3f821e-5821-49bc-9368-658c93cad482" />


## General Wiring Notes

- **Servo Signal:** Connect each servo's signal wire to a dedicated GPIO pin on the ESP32. See `SERVO_PINS[]` in the firmware and update the pin assignments to match your actual wiring.

- **Servo Power:** All servo V+ connections must come directly from the external buck converter's **5V rail**. Do **not** power the servos from the ESP32's onboard regulator.

- **ESP32 Power:** For untethered operation, power the ESP32 from the **same 5V/GND rail** as the servos through the board's **5V/VIN and GND pins**. Do not rely on USB power during normal operation.

- **Common Ground:** This is critical. Connect:
  - ESP32 GND
  - Power adapter GND
  - All servo GND wires
  
  These must share a common ground. Without a common reference, the PWM signals can become unstable and cause erratic or uncontrolled servo movement.

- **DC Barrel Jack:** Before soldering, use a multimeter in continuity mode to verify:
  - Tip = Positive (+)
  - Sleeve = Negative (−)
  
  Do not assume the pin arrangement. Some barrel jacks have a third **switch/detect pin** that can look like a power connection.

- **Perfboard / Prototyping Board:** Carefully inspect every solder joint along the power path. A cold or weak solder joint may show the correct voltage with a multimeter but still cause voltage drops during sudden current bursts, especially when the ESP32's Wi-Fi starts broadcasting.

## Software Setup

### Firmware (ESP32)

The firmware is located in the `/firmware` folder:

`robotic_arm_wifi_control_panel.ino`

This single file contains both the **Wi-Fi server** and the complete **web-based control panel** for the robotic arm.

### Requirements

- [Arduino IDE](https://www.arduino.cc/en/software) with **ESP32 board support** installed.
- **ESP32Servo** library by **Kevin Harrington**.
  - Install it through the Arduino IDE **Library Manager**.

## Setup & Upload

**Steps:**
1. Open `robotic_arm_wifi_control_panel.ino` in the **Arduino IDE**.
2. Update `SERVO_PINS[]` and `JOINT_NAMES[]` to match your actual wiring.
3. Tune the movement speed for each joint if required:
   - `STEP_SIZE[]` — degrees moved per step.
   - `STEP_INTERVAL_MS[]` — delay in milliseconds between steps.
   - Use one value for each joint.
   - Heavier **MG995** joints generally work better with smaller/slower steps, while lighter **SG90** joints can use faster movement.
4. *(Optional)* Change `WIFI_SSID` and `WIFI_PASSWORD` if required.
5. Connect the ESP32 and **upload the firmware**.
6. Open the **Serial Monitor** at `115200` baud.
7. Check the Serial Monitor for the arm's IP address. The default access point address is:
   ```text
   192.168.4.1

### Control Panel

No separate file or installation is required. The **ESP32 hosts and serves the control panel directly**.

**Steps:**

1. Power on the robotic arm.
2. On your phone or laptop, connect to the **ESP32 Wi-Fi network**. The SSID is shown in the Serial Monitor.
3. Open a web browser and enter the arm's IP address, for example:
   ```text
   http://192.168.4.1

### Contributing

Issues and pull requests are welcome. If you build this arm yourself, feel free to open an issue with photos — always good to see other builds.

### License

This project is licensed under the MIT License — free to use, modify, and distribute.

