# Boiler Control

A simple Arduino Nano-based system to manage a shared boiler queue between 4 residents using physical switches and LEDs.

## Project Overview

This project is designed for a shared living space where multiple residents use a common boiler. It provides a fair and automated way to manage access, using an Arduino Nano, physical switches, and LEDs.

## Features

- **3 Normal Switches**  
  Representing 3 different residents.  
  - Short press: Join the queue  
  - Long press: Leave the queue  
  - Corresponding **control LED** indicates queue participation

- **1 Master Switch**  
  - Overrides the queue and gains immediate control of the boiler  
  - The currently active resident is pushed to the front of the queue  
  - Useful for emergency or priority use

- **4 Control LEDs**  
  - Show which residents are currently in the queue

- **4 Operation LEDs**  
  - Simulate actual power delivery to the boiler  
  - Only **one** operation LED is active at a time  
  - Can be connected to relays for individual electricity tracking

## Queue Logic

1. **Join the Queue**  
   A short press on any of the 3 normal switches adds the resident to the queue.

2. **Leave the Queue**  
   A long press removes the resident from the queue.

3. **Master Override**  
   Pressing the master switch bypasses the queue immediately.  
   The current active resident is pushed to the front of the queue and paused.

> Only one operation LED (and thus one boiler power channel) is active at any given time, ensuring exclusive use and proper electricity tracking.

## Remote control

Includes remote control with Serial, which can be used with [RoboRemo](https://roboremo.app/) for example via Bluetooth (RFCOMM).

Key features:
- View the current operating LED.
- Turn on/off the master switch.
- Control the operation duration.

## Use Case

Perfect for apartments or shared spaces where:
- A single boiler is shared between residents.
- Fair usage and electricity tracking are needed.
- Physical interface is preferred over digital/app-based systems.

## License

This project is open-source and available under the [MIT License](LICENSE).
