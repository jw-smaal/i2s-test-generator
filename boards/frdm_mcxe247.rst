.. _frdm_mcxe247_wiring:

FRDM-MCXE247 Hardware Wiring (User Truth)
#########################################

This document provides the definitive physical wiring for the MIDI Synthesizer 
on the FRDM-MCXE247 board J1 header.

J1 Header Pinout
****************

The following physical pin assignments must be used on the J1 header:

+------------------+-----------------------+------------------------+
| Signal           | Physical J1 Pin       | MCU Pin                |
+==================+=======================+========================+
| Bit Clock (BCLK) | **J1 Pin 9**          | PTD1 (SAI0_MCLK)       |
+------------------+-----------------------+------------------------+
| Frame Sync (LRCK)| **J1 Pin 7**          | PTA12 (SAI0_BCLK)      |
+------------------+-----------------------+------------------------+
| Data Out (TX_DAT)| **J1 Pin 5**          | PTA11 (SAI0_SYNC)      |
+------------------+-----------------------+------------------------+
| Data In (RX_DAT) | **J1 Pin 15**         | PTA14 (SAI0_D3)        |
+------------------+-----------------------+------------------------+

*Note: This mapping follows the physical locations specified by the user 
as the project "Truth".*

MIDI Connectivity
*****************

MIDI input utilizes the standard Arduino pins:

*   **MIDI RX**: Arduino D0 (J1 Pin 14 / PTD17)
*   **MIDI TX**: Arduino D1 (J1 Pin 16 / PTE12)

@author Jan-Willem Smaal <usenet@gispen.org>
