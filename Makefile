CC = gcc
GLSLC = glslangValidator
CFLAGS = -Ofast -Wall
LDLIBS = -lxcb -lvulkan
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
	$(GLSLC) -V $< -o $@

$(FMODS): $(FSHADES)
	$(GLSLC) -V $< -o $@

clean:
	rm $(OBJECTS) $(VMODS) $(FMODS)
