# MAKEFILE C++
# --------------------------------------------
# Compiler
CC          := g++

# Target binary
TARGET      := app

# Directories
SRCDIR      := src
INCDIR      := inc
BUILDDIR    := obj
TARGETDIR   := bin
RESDIR      := res
SRCEXT      := cpp
OBJEXT      := o

# Debug / Release toggle
ifeq ($(DEBUG),1)
  CFLAGS    := -std=c++20 -Wall -Wextra -g -O0
else
  CFLAGS    := -std=c++20 -Wall -Wextra -O2
endif

# Shown on the title screen: the git tag/commit, or "dev" outside a checkout.
VERSION     := $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)
CFLAGS      += -DVOXEL_VERSION='"$(VERSION)"'

# Auto-generated header dependency files (-MMD produces .d alongside .o)
DEPFLAGS    := -MMD -MP

# Libraries
LIB         := -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -ldl

# Include paths
INC := -I$(CURDIR)/$(INCDIR) \
       -I$(CURDIR)/external \
       -I$(CURDIR)/external/imgui \
       -I$(CURDIR)/external/imgui/backends \
       -I$(CURDIR)/external/stb \
       -I$(CURDIR)/external/glad/include \
       -I$(CURDIR)/external/KHR \
       -I$(CURDIR)/external/robin_hood \
       -I/usr/local/include

# --- Source files ---
APP_SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
GLAD_SRC    := external/glad/src/glad.c
IMGUI_SRC   := $(wildcard external/imgui/*.cpp) \
               $(wildcard external/imgui/backends/*.cpp)

# --- Object files ---
APP_OBJECTS := $(patsubst $(SRCDIR)/%.$(SRCEXT),$(BUILDDIR)/%.$(OBJEXT),$(APP_SOURCES))
GLAD_OBJ    := $(patsubst %.c,$(BUILDDIR)/%.o,$(GLAD_SRC))
IMGUI_OBJ   := $(patsubst %.cpp,$(BUILDDIR)/%.o,$(IMGUI_SRC))

OBJECTS     := $(APP_OBJECTS) $(GLAD_OBJ) $(IMGUI_OBJ)

# --- Tests (doctest 2.4.11, vendored at external/doctest/doctest.h) ---
# The test binary links every app object except main.o, so tests exercise the
# real code. Libraries are the same as the app's, but tests must never open a
# window or touch GL.
TESTDIR      := tests
TEST_SOURCES := $(wildcard $(TESTDIR)/*.$(SRCEXT))
TEST_OBJECTS := $(patsubst $(TESTDIR)/%.$(SRCEXT),$(BUILDDIR)/$(TESTDIR)/%.$(OBJEXT),$(TEST_SOURCES))
TEST_TARGET  := tests

# Must come after OBJECTS/TEST_OBJECTS: := expands immediately.
DEPS        := $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)

# Default target
all: directories resources $(TARGET) compile_commands.json

# Build binary
$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGETDIR)/$(TARGET) $^ $(LIB)

# Compile C++ files
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INC) -c -o $@ $<

# Build and run the unit tests
test: directories $(TEST_OBJECTS) $(filter-out $(BUILDDIR)/main.o,$(OBJECTS))
	$(CC) -o $(TARGETDIR)/$(TEST_TARGET) $(TEST_OBJECTS) $(filter-out $(BUILDDIR)/main.o,$(OBJECTS)) $(LIB)
	./$(TARGETDIR)/$(TEST_TARGET)

$(BUILDDIR)/$(TESTDIR)/%.$(OBJEXT): $(TESTDIR)/%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INC) -c -o $@ $<

# Compile ImGui files
$(BUILDDIR)/external/%.o: external/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INC) -c -o $@ $<

# Compile Glad (C file)
$(BUILDDIR)/external/glad/src/glad.o: external/glad/src/glad.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

# --- Utility targets ---
resources:
	@cp -r $(RESDIR)/* $(TARGETDIR)/ 2>/dev/null || true

directories:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(BUILDDIR)

clean:
	@$(RM) -rf $(BUILDDIR)

cleaner: clean
	@$(RM) -rf $(TARGETDIR)

# --- compile_commands.json (for clangd) ---
compile_commands.json.tmp:
	@echo "[" > $@
	@for src in $(APP_SOURCES) $(IMGUI_SRC) $(GLAD_SRC); do \
	  obj=$(BUILDDIR)/$${src%.*}.o; \
	  echo "  {" >> $@; \
	  echo "    \"directory\": \"$(CURDIR)\"," >> $@; \
	  echo "    \"command\": \"$(CC) $(CFLAGS) $(INC) -c $$src -o $$obj\"," >> $@; \
	  echo "    \"file\": \"$$src\"" >> $@; \
	  echo "  }," >> $@; \
	done
	@sed -i '$$ s/},/}/' $@
	@echo "]" >> $@

compile_commands.json: compile_commands.json.tmp
	@if ! cmp -s $@ $<; then mv $< $@; else rm $<; fi

.PHONY: all clean cleaner resources directories test

# Include auto-generated header dependency rules (silently ignored on first build)
-include $(DEPS)
