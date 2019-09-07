# RED ROCKET

## REQUIRES
- Microchip AVR-IOT WG board (AC164160)
- MPLAB X 5.20 or later
- AVR GCC compiler 5.40

## DESCRIPTION
This project was produced by the MPLAB X Code Configurator v.3.85.1 (using
the AVR-IOT Example v1.2.0 and later hand modified extensively to fix a number
of known shortcomings of the current implementation such as:
- Removing a number of trivial warnings
- Reducing the number of files (removing or excluding unused ones) to speed up build
- Reducing the code size (<75%)
- Improving performance
- Improving programming time (increased default communication speed with PKOB Nano)

(see the (git) log for a detailed list of the modifications)

## NOTE
This repository is meant to be used as a *git-template* to create derivate
repositories/applications where MCC can still be used to further add sensors and
actuators as needed to further customize the application.
