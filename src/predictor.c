/*
 * Copyright (C) 2013  Travis Brown (travisb@travisbrown.ca)
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
/*
 * The core of the prediction engine
 */
#include <string.h>

#include "buffer.h"
#include "controller.h"

#include "predictor.h"

/*
 * Initialize the given predictor object.
 * 
 * Returns:
 * 0 - On success
 */
int predictor_init(struct predictor *predictor) {
	predictor->num_chars = 0;
	predictor->num_echoed = 0;
	memset(&predictor->history, 0, sizeof(predictor->history));

	return 0;
}

/*
 * Given the latest input from the user, output the best prediction of the
 * local echo to the windows of that buffer.
 *
 * Returns:
 * 0      - On success
 * EAGAIN - Buffer didn't have sufficient space
 */
int predictor_output(struct predictor *predictor, struct buffer *buffer, int size, char *input) {
	return buffer_output(buffer, size, input);
}

/*
 * Given the latest output from the terminal, replace any correct prediction
 * with the confirmed output, undo any incorrect prediction and re-output the
 * new predicted characters.
 *
 * Returns:
 * 0      - On success
 * EAGAIN - Controller buffer didn't have sufficient space
 */
int predictor_learn(struct predictor *predictor, int bufid, int size, char *output) {
	return controller_output(bufid, size, output);
}

