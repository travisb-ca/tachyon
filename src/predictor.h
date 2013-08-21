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
 * Header for the prediction engine, which runs various predictors against
 * the input and output and uses the best predictor for local echo
 * prediction.
 */
#ifndef PREDICTOR_H
#define PREDICTOR_H

struct buffer;

#define PREDICTOR_PREDICTION_LENGTH 128

struct predictor {
	int num_chars;

	int num_echoed;

	char history[PREDICTOR_PREDICTION_LENGTH];
};

int predictor_init(struct predictor *predictor);
int predictor_output(struct predictor *predictor, struct buffer *buffer, int size, char *input,
		     int (*outfunc)(struct buffer *buffer, int size, char *buf));
int predictor_learn(struct predictor *predictor, int bufid, int size, char *output);

#endif
