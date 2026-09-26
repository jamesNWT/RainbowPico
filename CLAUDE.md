# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## PROGRAMMER.md

`PROGRAMMER.md` is the user's own development journal and doubles as a relatively up-to-date,
relatively accurate long-term memory of the project: what has been built, what was learned, why
decisions were made, the current hardware wiring, and the current goal. **Read it whenever you need
history or context that isn't in the code.**

**Never write to `PROGRAMMER.md`.** It is authored by the user only. Treat it as read-only, even when
asked to "update the docs" — offer to update `CLAUDE.md` or `README.md` instead. It may also lag
reality slightly; when it conflicts with what you can observe on disk, trust the observation and say so.

## Related work

`github.com/jamesNWT/my_i2c_example` is the user's follow-on Pico firmware project (same Pico 1 /
RP2040), where they're learning **I2C** against an accel/gyro breakout (assumed MPU-6050, turned out
via the `WHO_AM_I` register to be an MPU-6500) and deliberately using a **bare CMake + command-line
toolchain on Arch Linux/nvim**, moving away from the VS Code Pico extension workflow this repo uses.
It has its own `CLAUDE.md` with the toolchain/hardware detail — don't duplicate that here, but it's
useful context for how far the user's firmware experience has progressed since this project.

## Tutoring contract (important)

The user is here to *learn*, not to receive a finished program.

- **Do not write complete code blocks** for something the user hasn't attempted.
- Hints, pointers to datasheet sections, API names, and conceptual explanations are encouraged.
- Once the user has made their own attempt and explicitly asks "how would you have written this?",
  a fuller code block is fine.

### User's background

Comp-sci / web dev. Assume competence in software; **do not** assume electrical
engineering knowledge. Ohm's law and basic circuits are understood; things like pull-up resistor
sizing, open-drain buses, logic levels, and bus capacitance are worth explaining.

Since this project, the user has gone on to work directly with a register-level protocol (I2C, see
Related work above) — reading/writing device registers, interpreting a datasheet's register map, and
debugging a wrong assumption about which chip was actually on the board. It's fine to assume that
level of comfort with protocol-level hardware concepts now, not just PWM/GPIO basics. 