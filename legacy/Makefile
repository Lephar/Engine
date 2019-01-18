CC = gcc
GLSLC = glslc
CFLAGS = -std=gnu17 -march=native -Ofast -Wall
GLSLFLAGS = -O
LDLIBS = -lm -lblis -lxcb -lvulkan
SOURCES = vulkan.c
VSHADES = shaders/shader.vert
FSHADES = shaders/shader.frag
OBJECTS = vulkan.o
VMODS = shaders/vert.spv
FMODS = shaders/frag.spv

all: $(OBJECTS) $(VMODS) $(FMODS)

$(OBJECTS): $(SOURCES)
	$(CC) $< -o $@ $(CFLAGS) $(LDLIBS)

$(VMODS): $(VSHADES)
	$(GLSLC) $< -o $@ $(GLSLFLAGS)

$(FMODS): $(FSHADES)
	$(GLSLC) $< -o $@ $(GLSLFLAGS)

clean:
	rm $(OBJECTS) $(VMODS) $(FMODS)
