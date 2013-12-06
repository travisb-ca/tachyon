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
#include "util.h"
#include "options.h"

#include "predictor.h"

static char backspaces[PREDICTOR_PREDICTION_LENGTH];

/*
 * Initialize the given predictor object.
 * 
 * Returns:
 * 0 - On success
 */
int predictor_init(struct predictor *predictor) {
	predictor->num_chars = 0;
	predictor->num_echoed = 0;
	predictor->history_used = 0;
	memset(&predictor->history, 0, sizeof(predictor->history));

	if (backspaces[0] != '\b')
		for (int i = 0; i < sizeof(backspaces); i++)
			backspaces[i] = '\b';

	return 0;
}

void predictor_free(struct predictor *predictor) {
	return;
}

/*
 * Given the latest input from the user, output the best prediction of the
 * local echo to the windows of that buffer.
 */
static void predictor_output_guess(struct predictor *predictor, struct buffer *buffer, int size, char *input) {
	int old_history_used;
	bool predict;
	int result;

	old_history_used = predictor->history_used;

	/* Normalize to avoid integer overflow */
	if (predictor->num_chars > 1000000) {
		predictor->num_chars /= 2;
		predictor->num_echoed /= 2;
	}

	if (predictor->num_chars == 0)
		predict = true;
	else
		predict = (predictor->num_echoed * 100) / predictor->num_chars
			>= PREDICTOR_ECHO_PERCENTAGE;

	if (predict && sizeof(predictor->history) - predictor->history_used >= size) {
		/*
		 * We have room so we can try to predict. We only predict
		 * whole input pieces to attempt to avoid cutting escape
		 * code in half. It's not perfect, but it should be good
		 * enough until we get more smarts about escape codes.
		 */
		memcpy(predictor->history + predictor->history_used,
		       input, size);
		predictor->history_used += size;

		result = buffer_input(buffer, size, input);
		if (result != 0) {
			/* We've failed to output, pretend we didn't do anything */
			predictor->history_used = old_history_used;
		}
	}
}

/*
 * Given the latest input from the user, output the best prediction of the
 * local echo to the windows of that buffer if prediction is enabled. In all
 * cases forward that user input to the slave.
 *
 * Returns:
 * 0      - On success
 * EAGAIN - Buffer didn't have sufficient space
 */
int predictor_output(struct predictor *predictor, struct buffer *buffer, int size, char *input,
		     int (*outfunc)(struct buffer *buffer, int size, char *buf)) {
	if (cmd_options.predict)
		predictor_output_guess(predictor, buffer, size, input);
	return outfunc(buffer, size, input);
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
int predictor_learn(struct predictor *predictor, struct buffer *buffer, int size, char *output) {
	int n;
	int result;

	if (cmd_options.predict) {
		/*
		 * Find the first difference between what we predicted and what
		 * actually came back.
		 */
		for (n = 0; n < predictor->history_used && n < size; n++) {
			predictor->num_chars++;

			if (output[n] != predictor->history[n])
				break;

			predictor->num_echoed++;
		}

		if (output[n] != predictor->history[n]) {
			/*
			 * We weren't perfect up to the end. First figure out how
			 * many characters were not echoed immediately to get
			 * better.
			 */
			predictor->num_chars += min(predictor->history_used - n, size - n);
		}

		/* Undo all the prediction to replay the correct sequence */
		buffer_input(buffer, predictor->history_used, backspaces);
	}

	/* Output the actual output */
	result = buffer_input(buffer, size, output);

	if (cmd_options.predict) {
		predictor->history_used = max(0, predictor->history_used - size);
		memmove(predictor->history, predictor->history + size, predictor->history_used);

		if (result == 0 && predictor->history_used > 0) {
			/* Reoutput the prediction */
			buffer_input(buffer, predictor->history_used, predictor->history);
		}
	}

	return result;
}

