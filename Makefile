
BOARD_TAG			= nano328
#BOARD_TAG			= nano
BOARD_SUB			= atmega328

include /usr/share/arduino/Arduino.mk
CFLAGS				+= -I.
CFLAGS				+= -std=c99
