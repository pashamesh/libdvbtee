LIB_DIR := $(shell pwd)/libdvbtee
APP_DIR := $(shell pwd)/dvbtee

all:

install:
	$(MAKE) -C $(LIB_DIR) install
	$(MAKE) -C $(APP_DIR) install

%::
	$(MAKE) -C $(LIB_DIR) $(MAKECMDGOALS)
	$(MAKE) -C $(APP_DIR) $(MAKECMDGOALS)