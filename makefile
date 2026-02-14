# makefile - SDL3 Top-Down RPG Engine
# Project layout:
#   src/**.c
#   inc/**.h
#   assets/...
# Output:
#   build/**.o
#   rpg_engine

CC      := gcc
TARGET  := rpg_engine
BUILD   := build
SRC_DIR := src
INC_DIR := inc

CSTD    := -std=c11
WARN    := -Wall -Wextra -Wpedantic
OPT     := -O2
DBG     := -g

# ------------------------------------------------------------
# pkg-config detection (SDL3 / SDL3_image / SDL3_ttf)
# ------------------------------------------------------------

PKGCONF := pkg-config

# Always require SDL3 via pkg-config (this should exist if SDL3 is installed)
SDL3_CFLAGS := $(shell $(PKGCONF) --cflags sdl3 2>/dev/null)
SDL3_LIBS   := $(shell $(PKGCONF) --libs   sdl3 2>/dev/null)

# SDL3_image / SDL3_ttf may not have .pc files in your install,
# so we TRY pkg-config first, otherwise fall back to -lSDL3_image/-lSDL3_ttf.
SDL3_IMAGE_CFLAGS := $(shell $(PKGCONF) --cflags SDL3_image 2>/dev/null)
SDL3_IMAGE_LIBS   := $(shell $(PKGCONF) --libs   SDL3_image 2>/dev/null)

SDL3_TTF_CFLAGS := $(shell $(PKGCONF) --cflags SDL3_ttf 2>/dev/null)
SDL3_TTF_LIBS   := $(shell $(PKGCONF) --libs   SDL3_ttf 2>/dev/null)

# Fallbacks if pkg-config didn't return anything
ifeq ($(strip $(SDL3_IMAGE_LIBS)),)
SDL3_IMAGE_LIBS := -lSDL3_image
endif

ifeq ($(strip $(SDL3_TTF_LIBS)),)
SDL3_TTF_LIBS := -lSDL3_ttf
endif

# If SDL3 itself isn't found, error early with a helpful message
ifeq ($(strip $(SDL3_LIBS)),)
$(error SDL3 not found via pkg-config. Install SDL3 or ensure pkg-config can find sdl3.pc)
endif

# ------------------------------------------------------------
# Paths / flags
# ------------------------------------------------------------

INCLUDES := -I$(INC_DIR) -I$(SRC_DIR)
CFLAGS   := $(CSTD) $(WARN) $(OPT) $(DBG) $(INCLUDES) \
            $(SDL3_CFLAGS) $(SDL3_IMAGE_CFLAGS) $(SDL3_TTF_CFLAGS)

# If you installed SDL into /usr/local, these help at link/runtime.
LDFLAGS  := -L/usr/local/lib -Wl,-rpath,/usr/local/lib -Wl,--enable-new-dtags

LIBS     := $(SDL3_LIBS) $(SDL3_IMAGE_LIBS) $(SDL3_TTF_LIBS)

# ------------------------------------------------------------
# Source discovery (recursive)
# ------------------------------------------------------------

SRCS := $(shell find $(SRC_DIR) -type f -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# ------------------------------------------------------------
# Rules
# ------------------------------------------------------------

.PHONY: all clean run print

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)

# Compile .c -> .o and generate .d dependency files
$(BUILD)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Include dependency files if they exist
-include $(DEPS)

clean:
	rm -rf $(BUILD) $(TARGET)

run: $(TARGET)
	./$(TARGET)

print:
	@echo "SRCS: $(SRCS)"
	@echo "OBJS: $(OBJS)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "LIBS: $(LIBS)"
