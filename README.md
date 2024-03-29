# About

Real-time rendering engine written in C as Vulkan practice

# Dependencies

Dependencies are rather minimal;

* Vulkan compatible GPU with up-to-date drivers
* C17 (bug fix version of C11) compatible version of Clang
* GLFW library for window creation
* Vulkan headers for core renderer
* GLSL compiler for shader compilation

Currently only Arch based distros have shaderc on their official repos, so you
may need to get it yourself on other distros. Alternatively you can switch to
glslangValidator from glslc. It is also possible to use gcc instead of clang.

Assuming you are on an up-to-date system, you can get the dependencies as such:

## Arch / Manjaro:

`sudo pacman -Syu glfw vulkan-devel shaderc clang`

## Fedora / RHEL:

`sudo dnf install glfw3 vulkan-headers vulkan-validation-layers glslang clang`

## Debian / Ubuntu / Elementary / Mint:

`sudo apt-get install libglfw3-dev libvulkan-dev vulkan-validationlayers-dev glslang-tools clang`

## Solus:

`sudo eopkg install system.devel glfw vulkan-headers vulkan-validation-layers glslang clang`

# Compile & Run

And to compile and execute the engine, simply run:

```
make
./engine
```

Press ESC to switch between the cursor mode and the camera mode.

# Credits

Special thanks to our tester [Kapkic](https://gitlab.com/kapkic), and
[Vulkan Tutorial](https://vulkan-tutorial.com/) creators and contributors
