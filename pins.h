#pragma once
#include <Arduino.h>

/*
  CryoBelt Rev-A pin map extracted from supplied KiCad netlist.

  ESP32-S3-MINI-1-N8:
    GPIO8   SCL
    GPIO9   SDA
    GPIO11  BQ25895 INT_N
    GPIO12  TPS61089 12V_EN
    GPIO13  FAN_GATE PWM
    GPIO14  INA180 fan-current ADC
    GPIO16  MAX98357A BCLK
    GPIO17  MAX98357A LRCLK
    GPIO18  MAX98357A DIN (D3 DOUT trace cut on reworked Rev-A hardware)
    GPIO26  SK6805 DIN
    GPIO36  FUSB303 OUT1
    GPIO37  FUSB303 OUT2
    GPIO38  UP button
    GPIO39  USER button
    GPIO40  DOWN button
    GPIO41  FUSB303 ID
    GPIO42  external OTG PMOS gate-control NMOS
    GPIO19  native USB D-
    GPIO20  native USB D+
*/

constexpr uint8_t PIN_I2C_SCL       = 8;
constexpr uint8_t PIN_I2C_SDA       = 9;
constexpr uint8_t PIN_BQ_INT_N      = 11;
constexpr uint8_t PIN_12V_EN        = 12;
constexpr uint8_t PIN_FAN_GATE      = 13;
constexpr uint8_t PIN_FAN_CURRENT   = 14;

constexpr uint8_t PIN_AUDIO_BCLK    = 16;
constexpr uint8_t PIN_AUDIO_LRCLK   = 17;
constexpr uint8_t PIN_AUDIO_DIN     = 18;

constexpr uint8_t PIN_RGB_DATA      = 26;

constexpr uint8_t PIN_FUSB_OUT1     = 36;
constexpr uint8_t PIN_FUSB_OUT2     = 37;
constexpr uint8_t PIN_BUTTON_UP     = 38;
constexpr uint8_t PIN_BUTTON_USER   = 39;
constexpr uint8_t PIN_BUTTON_DOWN   = 40;
constexpr uint8_t PIN_FUSB_ID       = 41;
constexpr uint8_t PIN_OTG_GATE      = 42;

constexpr uint8_t PIN_USB_D_MINUS   = 19;
constexpr uint8_t PIN_USB_D_PLUS    = 20;
