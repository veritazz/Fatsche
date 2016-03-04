# Arduino Make file. Refer to https://github.com/sudar/Arduino-Makefile
# if you have placed the alternate core in your sketchbook directory, then you can just mention the core name alone.
ARDMK_DIR=/usr/share/arduino/
#ARDUINO_CORE_PATH = /home/veritazz/sketchbook/hardware/core13/variants
#ARDUINO_VAR_PATH = /home/veritazz/sketchbook/hardware/core13/variants
#ALTERNATE_CORE = core13
# If not, you might have to include the full path.
#ALTERNATE_CORE_PATH = /home/veritazz/sketchbook/hardware/core13

BOARD_TAG    = micro
#ISP_PROG = avrispmkii
#ISP_PORT = usb
#AVRDUDE_ARD_BAUDRATE = 100 -B 100
ARDUINO_LIBS = Arduboy/src Arduboy/src/core Arduboy/src/audio ArduboyExtra SPI EEPROM
#EXTRA_CXXFLAGS = ...
include $(ARDMK_DIR)/Arduino.mk

# !!! Important. You have to use make ispload to upload when using ISP programmer
