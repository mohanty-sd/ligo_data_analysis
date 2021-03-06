/*
 * options.h
 *
 *  Created on: Mar 20, 2017
 *      Author: marcnormandin
 */

#ifndef SRC_C_OPTIONS_H_
#define SRC_C_OPTIONS_H_

#include "random.h"
#include "inspiral_signal.h"

int process_command_options (int argc, char **argv, source_t *s, gslseed_t *seed, int *last_index);

#endif /* SRC_C_OPTIONS_H_ */
