SHADER_SRCS = $(wildcard src_shaders/*.vert src_shaders/*.frag)
SHADER_OBJS = $(subst src_shaders,bin/shaders,$(addsuffix .spv,$(SHADER_SRCS)))

bin/shaders/%.spv: src_shaders/%
	glslc $< -o $@

shaders: $(SHADER_OBJS)
