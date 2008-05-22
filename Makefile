
CFLAGS += -g -O0
ARCH=$(shell uname -m)
OBJDIR ?= obj/$(ARCH)
LIBRARIES += -lboost_regex
LIBRARIES += -lboost_program_options
LIBRARIES += -lncurses
PREFIX ?= /usr/local
BIN=schedtop
INSTPATH=$(PREFIX)/bin

SRCS += $(shell find *.cc)
HEADERS += $(shell find *.hh)

OBJS = $(patsubst %.cc,$(OBJDIR)/%.o,$(SRCS))

OUTPUT = $(OBJDIR)/$(BIN)

all: $(OUTPUT)

$(OBJS): $(HEADERS) Makefile

$(OUTPUT): $(OBJS)

$(OBJDIR)/%.o: %.cc	Makefile
	@echo "Compiling (C++) $< to $@"
	@mkdir -p $(OBJDIR)
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJDIR)/%:
	@echo "Linking $@"
	@mkdir -p $(OBJDIR)
	$(CXX) $(CFLAGS) $(INCLUDES) -o $@ $(filter %.o %.a,$+) $(LIBDIR) $(LIBRARIES)

clean: 
	@rm -f $(OBJDIR)/*

install: $(OUTPUT)
	@mkdir -p $(INSTPATH)
	cp -d $(OUTPUT) $(INSTPATH)
