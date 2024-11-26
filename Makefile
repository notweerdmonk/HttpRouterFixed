SHELL := /bin/bash
CC := g++

ifeq ($(CPP_VERSION),)
CPP_VERSION := c++23
endif

ifneq ($(DEBUG),)
CFLAGS += -ggdb3
endif

ifeq ($(INCLUDE_DIR),)
INCLUDE_DIR := .
endif

HEADER_FILES := HttpRouter.hpp
HEADER_FILES := $(foreach header, $(HEADER_FILES), $(INCLUDE_DIR)/$(header))

INCLUDE_FLAGS := $(foreach include_dir, $(INCLUDE_DIR), -I$(include_dir))

LIBS :=
LD_FLAGS := $(foreach lib, $(LIBS), -l$(lib))

SRCS := tests.cpp

OBJECTS := $(SRCS:.cpp=.o)

TARGETS := $(SRCS:.cpp=)

all: $(TARGETS)

$(OBJECTS): $(HEADER_FILES)

$(TARGETS): $(OBJECTS)

./%: ./%.o
	$(CC) $(INCLUDE_FLAGS) -o $@ $< $(LD_FLAGS)

./%.o: ./%.cpp $(HEADER_FILES)
	$(CC) -std=$(CPP_VERSION) $(CFLAGS) $(INCLUDE_FLAGS) -o $@ -c $<

clean:
	rm -f $(OBJECTS) $(TARGETS) 

.PHONY: all clean
