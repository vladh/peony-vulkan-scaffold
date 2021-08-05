# Peony Game Engine
# Copyright (C) 2020 Vlad-Stefan Harbuz <vlad@vladh.net>
# All rights reserved.

COMPILER_FLAGS = \
	-I/usr/local/opt/glm/include \
	-I/usr/local/opt/glfw/include \
	-I/usr/local/opt/assimp/include \
	-I/usr/local/opt/freetype/include/freetype2 \
	-I$(HOME)/.local/opt/VulkanSDK/1.2.176.1/macOS/include \
	-D_FORTIFY_SOURCE=2 -ggdb3 -Og -Wall -Werror -Wextra -pedantic \
	-std=c++2a \
	-Wno-deprecated-volatile -Wno-unused-function -Wno-unknown-pragmas -Wno-comment \
	-Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers \
	-Wno-unused-result -Wno-class-memaccess -Wno-unused-but-set-variable

LINKER_FLAGS = \
	-L/usr/local/opt/glfw/lib \
	-L/usr/local/opt/assimp/lib \
	-L/usr/local/opt/freetype/lib \
	-L$(HOME)/.local/opt/VulkanSDK/1.2.176.1/macOS/lib \
	-lvulkan -lfreetype -lglfw -lassimp -lm

unity-bundle: unity-bare
	mkdir -p bin/peony.app/Contents/MacOS
	cp bin/peony bin/peony.app/Contents/MacOS/
	cp extra/Info.plist bin/peony.app/Contents/

unity-bare: shaders
	@echo "################################################################################"
	@echo "### Building"
	@echo "################################################################################"
	time g++ $(COMPILER_FLAGS) $(LINKER_FLAGS) src/_unity.cpp -o bin/peony

unity: unity-bundle

run:
	@./bin/peony.app/Contents/MacOS/peony
