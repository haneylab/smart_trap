FQBN := arduino:avr:uno
PORT := /dev/ttyACM0
SKETCH_NAME := smart_trap

SUFFIX := $(subst :,.,$(FQBN))
ELF_FILE := $(SKETCH_NAME)/$(SKETCH_NAME).$(SUFFIX).elf

compile: $(ELF_FILE)
	
$(ELF_FILE) : $(SKETCH_NAME)/$(SKETCH_NAME).ino
	arduino-cli  compile -p $(PORT)  --fqbn  $(FQBN)  $(SKETCH_NAME)

upload: compile
	arduino-cli  upload -p $(PORT)  --fqbn  $(FQBN)  $(SKETCH_NAME) -v

monitor: 
	python serial_monitor.py  --port $(PORT)


