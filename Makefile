# Peony Game Engine
# Copyright (C) 2020 Vlad-Stefan Harbuz <vlad@vladh.net>
# All rights reserved.

.PHONY: unity unity-bundle run shaders default vert frag clean

default: unity

include make_shaders.mk

ifeq ($(OS),Windows_NT)
	include make_windows.mk
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		include make_macos.mk
	else
		include make_linux.mk
	endif
endif

clean:
	rm -rf bin/peony* bin/shaders/*
