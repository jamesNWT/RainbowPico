#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "Controller.h"

const char *game_action_to_string(game_action_kind action);
// Returns a static buffer that the next call overwrites, so only call it from
// one task.
const char *controller_to_string(const struct controller *controller);

#endif