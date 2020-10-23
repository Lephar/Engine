CC = clang
SLC = glslc
CFLAGS = -std=gnu17 -march=native -mtune=native -O2 -Wall -Wextra
LDLIBS = -lm -lglfw -lvulkan
SOURCES = engine.c
VSHADES = shaders/shader.vert
FSHADES = shaders/shader.frag
OBJECTS = engine
VMODS = shaders/vert.spv
FMODS = shaders/frag.spv

all: $(OBJECTS) $(VMODS) $(FMODS)

$(OBJECTS): $(SOURCES)
	$(CC) $< -o $@ $(CFLAGS) $(LDLIBS)

$(VMODS): $(VSHADES)
	$(SLC) $< -o $@ -O

$(FMODS): $(FSHADES)
	$(SLC) $< -o $@ -O

clean:
	rm $(OBJECTS) $(VMODS) $(FMODS)
