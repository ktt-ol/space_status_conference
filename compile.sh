#!/bin/sh

arduino-cli lib install PubSubClient
arduino-cli -b esp8266:esp8266:d1_mini compile
