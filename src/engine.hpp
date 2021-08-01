#pragma once

#include "types.hpp"
#include "common.hpp"

struct EngineState {
  m4 model;
  m4 view;
  m4 projection;
};

namespace engine {
  void update(EngineState *engine_state, CommonState *common_state);
}
