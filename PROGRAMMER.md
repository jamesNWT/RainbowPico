# PROGRAMMER.md

A journal of development in this repo, and long-term memory accessible to agents, but LLMs do not modify this file.

## What we're building and why

A toy C program to learn embedded development using LEDs, resistors, and buttons on a breadboard, driven by an RP2040 Raspberry Pi Pico.

### Current hardware list

- RaspberryPi PICO-H 1

### Current circuit diagram

```ASCII
┌────────────────────────────────────────────────────────────────────┐
│ PICO                                                               │
│                                                                    │
│phys:  04   05   06   09   10   11   19   20   25   26   27   36  38│
│GPIO: GP02 GP03 GP04 GP06 GP07 GP08 GP14 GP15 GP19 GP20 GP21 3V3 GND│
└───────│────│────│────│────│────│────│────│────│────│────│────│───│─┘
        │    │    │    │    │    │    │    │    │    │    │    │   │  
        │    │    │ ┌──┴─┐┌─┴──┐┌┴───┐│    │ ┌──┴─┐┌─┴──┐┌┴───┐│   │  
        │    │    │ │220Ω││220Ω││220Ω││    │ │220Ω││220Ω││220Ω││   │  
        │    │    │ └──┬─┘└─┬──┘└┬───┘│    │ └──┬─┘└─┬──┘└┬───┘│   │  
      ┌─┼─┐┌─┼─┐┌─┼─┐ ┌┴────┴────┴┐   │    │   ┌┴────┴────┴┐   │   │  
      │RED││GRE││BLU│ │B    G    R│   │    │   │B    G    R│   │   │  
      │LED││LED││LED│ │Target LED │   │    │   │ Play LED  │   │   │  
      └───┘└───┘└───┘ └─────┬─────┘   │    │   └─────┬─────┘   │   │  
        │    │    │         │      ┌──┼─┐ ┌┼───┐     │         │   │  
        └────┼────┘         │      │Up  │ │Down│     │         │   │  
             │              │      │But.│ │But.│     │         │   │  
          ┌──┼─┐            │      └──┬─┘ └┬───┘     │         │   │  
          │220Ω│            │         │    │         │         │   │  
          └──┬─┘            └─────────│────│─────────┴─────────┘   │  
             │                        │    │                       │  
             │                        │    │                       │  
             └────────────────────────┴────┴───────────────────────┘  
```

## Structure / Architecture

  
## What we have done so far and why:

### Breadboard construction and rainbow program working

I got the program basically working before starting this dev journal to a state where I had two buttons: one for on-off, one for changing color, and an LED that would respond to said buttons. Learned about PWM and discovered some basic principles of working with hardware and firmware while building.

### FreeRTOS setup:

- brought in the FreeRTOS kernel as a git submodule pinned to V11.3.1 in a `lib` directory, following the example of https://github.com/LearnEmbeddedSystems/rp2040-freertos-template.git .
- Got Claude to re-do the directories to move my application code into a `source` directory and adjust the cmakelists.txt files to accommodate the new structure, and properly bring in the FreeRTOS Kernel as a dependency.

### Breadboard re-work:

Added more components to the breadboard, lifted the PICO off the breadboard to make room. Improved the wiring.

### Break up code 
into domains of: 
- hardware configuration and initialization definitions (`Hardware.h`)
- hardware component control (`ComponentControl.h`)

### Verify re-worked breadboard
- Rewrote main program to verify buttons and basic LEDs are configured and initialized correctly.

### Write fancy pwm-initialization function
- Used bitmasks to ensure that each pwm slice is only initialized once, and channel output polarity can be set per-pin. Wasn't strictly necessary for this project but to me it just feels like the right way to do it. Now this function can be called on any pin without worrying about overwriting settings, with the caveat that the slice-level configuration is hardcoded into the function (wrap, clock divide).
- Once this was done I rewrote the main program to verify that the RGB-LED pins were working as expected.

### Better button polling
- We can now detect when a button state changes, what state it is in, and how long a press is held.

### Yet better button polling
- We now have more precise button polling by implementing the poll as an ISR.

## Current goal

Change the device from simply displaying a light into a hue-matching color game.

## Later TODOs:

- None