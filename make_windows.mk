# Peony Game Engine
# Copyright (C) 2020 Vlad-Stefan Harbuz <vlad@vladh.net>
# All rights reserved.

COMPILER_FLAGS = \
	-I "C:/local/include/"  \
	-I "C:/local/opt/VulkanSDK/1.2.176.1/Include" \
	-FC -GS -MTd -MP18 -W4 -WL -WX -Z7 -Zo -diagnostics:column -fp:fast -nologo \
	-sdl -guard:cf -permissive- -std:c++latest \
	-GL -Oi -O2 \
	-wd4100 -wd4127 -wd4201 -wd4505 -wd4706 -wd4702 \
	-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX

LINKER_FLAGS = \
	-LIBPATH:"C:/local/lib/" \
	-LIBPATH:"C:/local/opt/VulkanSDK/1.2.176.1/Lib" \
	/SUBSYSTEM:console \
	-CGTHREADS:8 -DEBUG:FULL -INCREMENTAL:NO -opt:ref \
	vulkan-1.lib freetype.lib opengl32.lib glfw3.lib assimp-vc142-mtd.lib \
	user32.lib gdi32.lib shell32.lib

.PHONY: unity run

unity:
	@echo "################################################################################"
	@echo "### Building"
	@echo "################################################################################"
	ctime -begin bin/peony.ctm
	cl $(COMPILER_FLAGS) src/_unity.cpp -link $(LINKER_FLAGS) -out:bin/peony.exe
	ctime -end bin/peony.ctm %LastError%

run:
	@bin/peony.exe
