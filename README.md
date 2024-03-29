# 0x10pb

## What?

A simple MIDI CC controller in 200x75mm footprint, with 16 potentiometers x 4 banks. USB-C for flashing an USB MIDI device communication, TRS MIDI type A output, option for battery power. 

Per knob + per bank MIDI CCs, channels and scaling are programmable in the firmware. Sends identical messages via both USB and TRS.

## Why?

There are also many small DIY MIDI controllers more refined than this one, but

 1. I couldn't find anything that was battery powered and with both USB and hardware serial MIDI output to use with eg. Dirtywave M8 and Teenage Engineering OP-1 Field. It's kind of a letdown to have a portable instrument with an internal battery and then have to carry around a power bank just for an auxiliary controller.
 2. Most of the simple controllers are missing banks / pages. Quite often you want to control more than 16 parameters without remapping, but don't need to control them all at the same time.

Then a friend wanted one too, so had to design one.

## How?

There's not much to it. A custom carrier board with a power switch (because of the battery power), a bank switch button, two LEDs that indicate which bank is active (00, 01, 10, 11), TRS MIDI port and associated circuitry, 16 pots read via a single multiplexer. 

[Adafruit Feather RP2040](https://learn.adafruit.com/adafruit-feather-rp2040-pico) is used as the brain, mainly because it conveniently has both a USB-C port and battery power / charging circuit built in.

### Hardware

First you'll need to get a carrier PCB. KiCad project and Gerbers are included for ordering your own. Once you have that, here's a list of parts you'll need to populate the board:

 - Adafruit Feather RP2040 plus suitable headers (eg. Adafruit slim ones): PCB will likely work with almost any Feather but you probably need to modify pin mapping and use a different core lib
 - Optional: 3.7V LiPoly battery with Feather compatible (JST-PH) connector - I use a tiny 500mA one
 - U1: CD74HC4067M (SOIC)
 - U2: 74LVC1G07 (SOT-23-5 / SOT-25)
 - D1-D2: 3mm LED (Whichever you want, I used low power green ones)
 - J3: CUI SJ1-3523N TRS jack
 - C1-C2: 100nF 0603 capacitors
 - R1: 30R 0603 resistor
 - R2: 10R 0603 resistor
 - R3: 10k 0603 resistor
 - R4-R5: 0603 LED series resistors - calculate for your LEDs and 3.3V supply voltage, I used 330R
 - SW1: SPST Subminiature switch (5mm leg spacing)
 - SW2: Omron B3F-10xx series tactile switch + cap
 - RV1-RV16: 10k standard 9mm footprint potentiometers, eg. Alpha 9mm, plus relatively slim knobs such as Davies 1900h clones

If you can (hand) solder SOIC and 0603, populating the board is going to be fast and straightforward. Just mind the build order - SMT components first, and if you don't use removable headers for the Feather you might want to solder the pots before that so the Feather won't get in the way.

The repository includes a simple "sandwich" type panel design. If you use that, you'll also need some M3 spacers and screws. The exact height will depend on eg. how you mount the Feather. Try eg. 6x 10-12mm male-female ones for the front side (just attaching the panel using pots and power switch is OK too), 6x 17-18mm female-female ones for the back, plus 12x short screws to put it together. 

If you use removable header stack for the Feather, you may also want to secure the board in place with a short M2.5 spacer / screw and nut. Otherwise it may slip off.

### Firmware + bank mappings

Firmware can be edited and flashed with [Arduino IDE](https://www.arduino.cc/en/software). You'll also need to install these libraries (see instructions in respective repositories):

 - Earle F. Philhower, III's [Raspberry Pi Pico Arduino core](https://github.com/earlephilhower/arduino-pico)
 - Forty Seven Effects' [Arduino MIDI library](https://github.com/FortySevenEffects/arduino_midi_library)

After you have the IDE and the libraries installed, open the included Arduino sketch. In Tools menu, select "Adafruit Feather RP2040" as the board and "Adafruit TinyUSB" as the USB stack. Other options can be left as is. Then attach the USB-C cable to the Feather and turn power on. You should now be able to upload the sketch to the Feather via USB.

If the device isn't automatically recognized and you get an upload error, you may have to keep the "bootsel" button (in the back end of the board) pressed, quickly press "reset" towards the front of the board and finally release "bootsel" to get the board to bootloader mode. Then try uploading again.

The sketch also  includes the hardcoded MIDI CC mappings for all the four banks. For now, whenever you want to edit the mappings, edit the source file according to instructions in the comments, then upload a new version to the Feather.
