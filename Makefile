FQBN := arduino:avr:nano:cpu=atmega328
PORT := /dev/ttyUSB0
SKETCH_NAME := smart_trap

SUFFIX := $(subst :,.,$(FQBN))
ELF_FILE := $(SKETCH_NAME)/$(SKETCH_NAME).$(SUFFIX).elf

compile: $(ELF_FILE)
	
$(ELF_FILE) : $(SKETCH_NAME)/$(SKETCH_NAME).ino
	arduino-cli  compile -p $(PORT)  --fqbn  $(FQBN)  $(SKETCH_NAME)

upload: compile
	arduino-cli  upload -p $(PORT)  --fqbn  $(FQBN)  $(SKETCH_NAME) -v
	
set_time: 


monitor: 
	python serial_monitor.py  --port $(PORT)


