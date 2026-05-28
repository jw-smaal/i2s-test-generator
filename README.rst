I2S Digital Signal Generator for NXP MCXN947
###########################################

A digital signal generator for verifying DSP filters and audio signal chains. This project configures a FRDM-MCXN947 as an I2S Master stimulus provider controlled via the Zephyr Shell, calibrated for the PCM5102A DAC.

Hardware Setup
**************

This project is designed to run on one FRDM-MCXN947 board, acting as the **Source**, connected to a PCM5102A DAC (or compatible).

Phase 1: Verification (Generator -> PCM5102A DAC)
=================================================

Use this setup to verify the generator output via listening or an oscilloscope.

+-----------------------+---------------+-----------------------+
| FRDM-MCXN947 (SAI1)   | Signal        | PCM5102A DAC          |
+=======================+===============+=======================+
| J1 Pin 1 (PIO3_16)    | BCLK          | BCK                   |
+-----------------------+---------------+-----------------------+
| J1 Pin 11 (PIO3_17)   | LRCK          | LCK                   |
+-----------------------+---------------+-----------------------+
| J1 Pin 5 (PIO3_20)    | DATA          | DIN                   |
+-----------------------+---------------+-----------------------+
| J1 Pin 14             | GND           | GND                   |
+-----------------------+---------------+-----------------------+
| J1 Pin 16             | 3.3V          | VCC                   |
+-----------------------+---------------+-----------------------+

*Note: The PCM5102A generates its own MCLK from BCLK, so no MCLK connection is required for this phase.*

Phase 2: DSP Testing (Board A Generator -> Board B Filter)
==========================================================

In this configuration, Board A provides the digital stimulus to Board B. Board B processes the signal and outputs it to the DAC.

**Inter-Board Wiring (I2S Bus):**

+-----------------------+---------------+-----------------------+
| Board A (Generator)   | Signal        | Board B (DSP Filter)  |
| SAI1 - Master         |               | SAI1 - Slave          |
+=======================+===============+=======================+
| J1 Pin 1 (PIO3_16)    | BCLK          | J1 Pin 1 (PIO3_16)    |
+-----------------------+---------------+-----------------------+
| J1 Pin 11 (PIO3_17)   | LRCK          | J1 Pin 11 (PIO3_17)   |
+-----------------------+---------------+-----------------------+
| J1 Pin 5 (PIO3_20)    | DATA          | J1 Pin 5 (PIO3_20)    |
+-----------------------+---------------+-----------------------+
| J1 Pin 14             | GND           | J1 Pin 14             |
+-----------------------+---------------+-----------------------+

**Board B Output (DSP Filter -> DAC):**

Board B then uses its second SAI (`sai0`) to drive the DAC.

+-----------------------+---------------+-----------------------+
| Board B (SAI0)        | Signal        | PCM5102A DAC          |
+=======================+===============+=======================+
| PIO2_6                | BCLK          | BCK                   |
+-----------------------+---------------+-----------------------+
| PIO2_7                | LRCK          | LCK                   |
+-----------------------+---------------+-----------------------+
| PIO2_2                | DATA          | DIN                   |
+-----------------------+---------------+-----------------------+

Software Usage
**************

1. Flash the ``i2s-generator`` project to Board A.
2. Open a serial terminal (115200 baud).
3. Use the ``gen`` shell commands to control the signal:

.. code-block:: bash

   uart:~$ gen wave sine
   uart:~$ gen freq 440
   uart:~$ gen level pro
   uart:~$ gen start

Frequency Sweep
===============

To start a continuous logarithmic sweep:

.. code-block:: bash

   # Sweep from 20 Hz to 20 kHz over 5 seconds (5000 ms)
   uart:~$ gen sweep start 20 20000 5000

To stop the sweep and return to the fixed frequency:

.. code-block:: bash

   uart:~$ gen sweep stop

Phase & Burst Control (Sine Only)
================================

Diagnostic features for stereo imaging and transient response:

.. code-block:: bash

   # Set Right channel 180 degrees out of phase
   uart:~$ gen phase 180

   # Start an 8-cycle on, 16-cycle off tone burst
   uart:~$ gen burst start 8 16

   # Stop burst mode
   uart:~$ gen burst stop

Waveform Types
==============

*   ``sine``: Sine wave using CMSIS-DSP FastMath.
*   ``white``: White noise with 0V DC offset.
*   ``pink``: Pink noise (-3dB/octave) using the **Voss-McCartney** algorithm.
*   ``square`` / ``triangle`` / ``saw``: Geometric shapes.
*   ``dirac``: Periodic impulse train (Dirac Comb).
*   ``silence``: Digital silence (all zeros) for noise floor measurement.
*   ``lrswap``: Channel identification; alternates a sine tone between Left and Right every 1 second.
*   ``imd``: Intermodulation Distortion test signal (**SMPTE RP120-1994**).
*   ``jtest``: Jitter diagnostic signal (**Julian Dunn, 1992**).

Reference Levels (Calibrated for 2.165 Vrms FS)
=============================================

*   ``max``: 0 dBFS (2.165 Vrms).
*   ``pro``: +4 dBu (1.228 Vrms). Targets **SMPTE RP155** / **AES3** professional alignment.
*   ``consumer``: -10 dBV (0.316 Vrms). Targets **IEC 60268-10** consumer standard.

Standards & References
======================

*   **Pink Noise**: McCartney, J. (1999). *A New Pink Noise Algorithm*.
*   **IMD (SMPTE)**: SMPTE RP120-1994. *Measurement of Intermodulation Distortion in Audio Systems*.
*   **J-Test**: Dunn, J. (1992). *A Sampled Test Signal for Jitter*. Audio Engineering Society.
*   **Professional Levels**: SMPTE RP155. *Reference Level for Digital Audio Systems*.
*   **Consumer Levels**: IEC 60268-10. *Sound system equipment - Peak programme level meters*.

Architecture
************

This generator is implemented with the following design choices on Cortex-M33:

*   **Encapsulated State**: Generator parameters (phase, frequency, sweep, burst, noise state) are encapsulated in a ``struct gen_state``.
*   **Fixed-Point DSP**: Uses ``q15_t`` and ``q31_t`` arithmetic. Floating point operations are excluded from the audio hot-path.
*   **CMSIS-DSP**: Employs ``arm_sin_q15`` and ``arm_scale_q15`` for signal generation.
*   **Persistent Clocking**: Utilises the MCX SAI RX interface to provide stable, continuous bit-clocks for the transmitter.

Project notes
=============
To build for the FRDM-MCXN947:

.. code-block:: bash

    west build -b frdm_mcxn947/mcxn947/cpu0 -p always
