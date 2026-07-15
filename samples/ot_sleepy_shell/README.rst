.. zephyr:code-sample:: openthread-sleepy-shell
   :name: OpenThread sleepy shell
   :relevant-api: net_stats

   Test Thread and IEEE 802.15.4 using an OpenThread shell configured as a
   button-controlled sleepy end device.

Overview
********

This sample is a dedicated sleepy-shell variant of the OpenThread shell sample.
It keeps the OpenThread CLI enabled, but configures the device as a sleepy end
device with CSL receiver support and platform deep-sleep coordination.

On boards with a ``sw0`` button alias, the sample starts in EM1-style idle
mode. Press Button 0 to toggle whether the OpenThread platform is allowed to
drop into EM2 between Thread wakeups, matching the SiSDK sleepy demo policy.

Building and Running
********************

Use this sample on boards that provide IEEE 802.15.4 support, a ``sw0`` button,
and EFR32 OpenThread counter support for microsecond alarm timing.

Build the sleepy shell application like this:

.. zephyr-app-commands::
   :zephyr-app: samples/ot_sleepy_shell
   :board: <board to use>
   :goals: build
   :compact:

Example building for Silicon Labs EFR32MG24 4187c radio board:

.. zephyr-app-commands::
   :zephyr-app: samples/ot_sleepy_shell
   :board: xg24_rb4187c
   :goals: build
   :compact:

Example building for Silicon Labs ``slwrb4180a``:

.. zephyr-app-commands::
   :zephyr-app: samples/ot_sleepy_shell
   :board: slwrb4180a
   :goals: build
   :compact:

Example building for Silicon Labs ``xg24_dk2601b``:

.. zephyr-app-commands::
   :zephyr-app: samples/ot_sleepy_shell
   :board: xg24_dk2601b
   :goals: build
   :compact:

Validation Focus
================

For EFR32 boards, verify the following:

* the device attaches as an SED and continues polling its parent across deep
  sleep residency
* Button 0 toggles the policy between EM1 idle and EM2 sleep
* idle current drops into EM2 instead of remaining in EM1/EM0 when sleep is
  allowed
* attach, reattach, and parent-loss recovery still work with ``CONFIG_PM=y``
