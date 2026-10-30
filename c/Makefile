# Aurora ERP - Makefile
# Compilacao: gcc -std=c11 -Wall -Wextra -O2 -Iinclude ... -lm
CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2 -Iinclude
LDLIBS  := -lm

# make SQL=1  -> habilita o modulo opcional do SQL Server (ODBC)
ifeq ($(SQL),1)
CFLAGS  += -DUSE_SQLSERVER
LDLIBS  += -lodbc
endif
TARGET  := aurora_erp
SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:src/%.c=build/%.o)

.PHONY: all clean run release test gui rungui

# ---- Interface grafica GTK3 (make gui) ----
GUI_TARGET := aurora_gui
GUI_SRC    := $(wildcard gui/*.c)
GUI_OBJ    := $(GUI_SRC:gui/%.c=build/gui_%.o)
CORE_OBJ   := $(filter-out build/main.o,$(OBJ))
GTK_CFLAGS  = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS    = $(shell pkg-config --libs gtk+-3.0 2>/dev/null)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

build/%.o: src/%.c include/erp.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

run: all
	./$(TARGET)

test: all
	./$(TARGET) --testes

release: clean
	$(MAKE) CFLAGS="-std=c11 -Wall -Wextra -O2 -DNDEBUG -Iinclude" all

gui: $(GUI_TARGET)

$(GUI_TARGET): $(CORE_OBJ) $(GUI_OBJ)
	$(CC) $^ -o $@ $(GTK_LIBS) $(LDLIBS)

build/gui_%.o: gui/%.c gui/gui.h include/erp.h | build
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $< -o $@

rungui: gui
	./$(GUI_TARGET)

clean:
	rm -rf build $(TARGET) $(GUI_TARGET) *.dat *.dat.tmp
