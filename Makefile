CC = clang
SLC = glslangValidator
CFLAGS = -std=gnu17 -march=native -Ofast -Wall -Wextra
LDLIBS = -lm -lglfw -lvulkan
SOURCES = engine.c
VSHADES = shaders/shader.vert
FSHADES = shaders/shader.frag
OBJECTS = engine.o
VMODS = shaders/vert.spv
FMODS = shaders/frag.spv

all: $(OBJECTS) $(VMODS) $(FMODS)

$(OBJECTS): $(SOURCES)
	$(CC) $< -o $@ $(CFLAGS) $(LDLIBS)

$(VMODS): $(VSHADES)
	$(SLC) -V $< -o $@

$(FMODS): $(FSHADES)
	$(SLC) -V $< -o $@

clean:
	rm $(OBJECTS) $(VMODS) $(FMODS)
