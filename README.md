# Datalogger

This project is a small, low-power flight logger for model rockets. The main idea is to build a compact device that can be powered by a small battery, installed in the rocket, switched on before launch, and then recovered after the flight for data analysis.

The logger is designed to record useful flight information such as acceleration and pressure/altitude, with the goal of reconstructing the rocket’s behavior during ascent and descent. In particular, the data can be used to estimate when parachutes deploy, based on changes in acceleration and barometric altitude.

## Current goals

- very small footprint
- low current consumption
- lightweight battery-powered design
- reliable operation during flight
- simple recovery and post-flight analysis
- support for detecting drogue and main parachute events
- two high-current outputs for deployment-related functions

## Hardware

The project currently includes:
- EasyEDA PCB design
- STM32-based main microcontroller
- accelerometer
- barometer
- internal memory for saved flight logs
- two high-current outputs

## Software

The project includes:
- STM32 firmware for sensor acquisition and data logging
- a Python helper program for reading saved logs from internal memory

## Status

This project is still under active development. The current repository contains the early foundations of the logger and basic data collection functionality. The design and software are expected to evolve as testing and flight validation progress.

## Purpose

The main purpose is to build a lightweight, practical flight logger that can be used to analyze rocket performance after recovery, especially for identifying critical events like chute deployment during flight.

## Future work

- improve reliability and power efficiency
- refine logging and data formats
- expand analysis tools
- validate behavior with real-world test flights
- improve deployment output control and timing
