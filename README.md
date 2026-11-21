# 16x16 LED Panel

Firmware for an ESP32-S3 driving a 16×16 WS2812B (NeoPixel) LED matrix. It has a
clock, scrolling text, pictures, dozens of animated effects, little animated
stories, and a set of calm ambient scenes (fireflies, koi pond, candle, campfire,
rainy window, jellyfish, ink in water, tide pool, sand garden, hourglass and more).
Everything is controlled from a web page served by the panel itself.

## Hardware

- ESP32-S3 board with **PSRAM** (4 MB flash or more)
- 16×16 WS2812B LED matrix (256 LEDs)
- 5 V power supply (the firmware limits brightness to about 2 A)
- Matrix **DIN → GPIO 4**, plus 5 V and GND shared between the board and panel

To use a different pin or current limit, change `LED_PIN` and `POWER_LIMIT_MA`
at the top of `MatrixPanel/MatrixPanel.ino`.

## Option 1: flash the ready-made firmware

`firmware/MatrixPanel-esp32s3-merged.bin` is a complete image. Write it at
address **0x0**, for example with esptool:

```
esptool.py --chip esp32s3 write_flash 0x0 firmware/MatrixPanel-esp32s3-merged.bin
```

or with a browser-based ESP flasher (Chrome/Edge), using offset 0x0.

## Option 2: build it with the Arduino IDE

1. In **Boards Manager**, install **esp32 by Espressif Systems**.
2. In **Library Manager**, install **Adafruit NeoPixel** and **Adafruit GFX Library**.
3. Open `MatrixPanel/MatrixPanel.ino` and set, under **Tools**:
   - Board: **ESP32S3 Dev Module**
   - USB CDC On Boot: **Enabled**
   - Flash Size: **4MB**
   - PSRAM: **Enabled** (required)
   - Partition Scheme: **Default 4MB with spiffs**
4. Pick the board's port and click **Upload**. If it won't connect, hold the
   **BOOT** button while plugging the board in, then try again.

## First-time setup

1. On first boot the panel opens a Wi-Fi hotspot called **Matrix-Panel**.
2. Join it from your phone; the setup page opens (or go to `http://192.168.4.1`).
3. Choose your Wi-Fi network and enter its password. The panel saves it and
   joins your network.
4. After that, open **http://matrix.local** (or the panel's IP address) to
   control it.

Wi-Fi details are stored on the board only; they are not in the code.

## License

MIT, see [LICENSE](LICENSE).
