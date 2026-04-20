# Meeting Room Aux Panel 3.5

Meeting Room Aux Panel 3.5 is firmware for the auxiliary `3.5-inch` room-status display used together with the main `Meeting Room` panel.

It runs on an ESP32-S3 based touch display and works as a smaller door-side panel in the same local meeting-room setup.
The UI supports both English and Chinese.

## Project Status

This firmware is operational and supports the full auxiliary panel workflow:
- first-time Wi-Fi onboarding
- main panel discovery and pairing
- live room-state updates over UDP
- runtime settings and language switch on device

## Features

- Compact room-status screen for the door-side panel
- Current room occupancy display
- Remaining meeting time display
- Room name display
- Local Wi-Fi onboarding through a setup portal
- Runtime settings screen on the device
- English / Chinese interface
- Pairing with the main panel over the local network
- Live room-state updates received from the main panel over UDP

## Hardware / Software

- Display class: `3.5-inch` touch panel
- Resolution: `480x320`
- MCU: `ESP32-S3`
- Framework: [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- UI stack: [LVGL](https://lvgl.io/)

## Project Layout

- `main/main.c` - startup, hardware init, Wi-Fi, setup portal, pairing, UDP receive loop
- `main/ui_aux_panel.c` - auxiliary panel LVGL UI and text updates
- `main/ui_aux_panel.h` - UI interface
- `main/aux_udp_protocol.h` - shared UDP packet format used with the main panel
- `components/fonts/` - Chinese font assets used by the UI

## Quick Start

1. Install ESP-IDF and export the environment.
2. Clone this repository.
3. Open the project directory.
4. Set chip target:

```bash
idf.py set-target esp32s3
```

5. Build firmware:

```bash
idf.py build
```

6. Flash firmware:

```bash
idf.py -p <PORT> flash
```

7. Open monitor:

```bash
idf.py -p <PORT> monitor
```

You can also run the project without local compilation: use firmware/flash_tool.exe, which flashes prebuilt binaries from firmware/binaries/.


## Wi-Fi Setup

If the panel does not yet have working Wi-Fi credentials, it can expose its own setup network and setup page.
That page is used to save the Wi-Fi SSID and password.

Typical flow:
1. Power on the panel.
2. Connect to the panel's temporary setup network.
3. Open the setup page in a browser.
4. Enter Wi-Fi credentials.
5. Save settings.
6. Wait for the panel to join the normal local network.

Once connected to Wi-Fi, the panel works as a normal auxiliary client in the same meeting-room environment as the main panel.

## Runtime Settings

The auxiliary panel includes an on-device settings overlay used for:
- Wi-Fi status
- device setup access
- language switching
- discovering and selecting the target main panel
- pairing / link flow

## Main Panel Integration

This repository is the auxiliary side of a two-panel setup.
The main panel is responsible for room-state generation and distribution.

How integration works:
- auxiliary panel joins the same local Wi-Fi network
- auxiliary panel discovers available main panels through UDP beacons
- auxiliary panel sends pair requests to the selected main panel
- pairing state is stored in NVS
- auxiliary panel receives live room-state packets and displays status

Shared protocol:
- file: `main/aux_udp_protocol.h`
- transport: UDP
- port: `33334`
- packet types: beacon, state, pair request, pair ack, pair nack

Typical deployment flow:
1. Flash main `Meeting Room` panel firmware.
2. Flash this auxiliary `3.5-inch` panel firmware.
3. Connect both devices to the same local Wi-Fi network.
4. Open auxiliary panel settings.
5. Select the target main panel.
6. Pair the auxiliary panel with the selected main panel.
7. Auxiliary display starts showing live room status.

## Runtime Configuration Stored In NVS

The auxiliary panel stores runtime configuration such as:
- Wi-Fi credentials
- panel name
- selected language
- selected target main panel
- pairing state and token

## Third-Party Components

This repository includes third-party components under `components/`, including LVGL and other ESP-IDF dependencies.
Please review their original licenses before redistribution.
