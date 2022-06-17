#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "pzip.h"

/**
 * pzip() - zip an array of characters in parallel
 *
 * Inputs:
 * @n_threads:		   The number of threads to use in pzip
 * @input_chars:		   The input characters (a-z) to be zipped
 * @input_chars_size:	   The number of characaters in the input file
 *
 * Outputs:
 * @zipped_chars:       The array of zipped_char structs
 * @zipped_chars_count:   The total count of inserted elements into the zippedChars array.
 * @char_frequency[26]: Total number of occurences
 *
 * NOTE: All outputs are already allocated. DO NOT MALLOC or REASSIGN THEM !!!
 *
 */
struct args {
	int n_threads;
	char *input_chars;
	int input_chars_size;
	struct zipped_char *zipped_chars;
	int *zipped_chars_count;
	int *char_frequency;
	int location;
	pthread_mutex_t mutex;
};

int *localLengths;

pthread_barrier_t barrier;
pthread_barrier_t barrier2;
pthread_barrier_t barrier3;
void *zippy(void *data);

void *zippy(void *data)
{
	int loc = ((struct args *)data)->location;
	pthread_mutex_unlock(&((struct args *)data)->mutex);
	int size = ((struct args *)data)->input_chars_size;
	int threads = ((struct args *)data)->n_threads;
	struct zipped_char *localArray =
		malloc(sizeof(struct zipped_char) * (size / threads));
	int count = 1;

	int localFrequency[26] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	struct zipped_char *ptr = localArray;
	char next_char;
	int numChar = 0;

	for (int i = loc; i < loc + (size / threads); ptr++) {
		numChar++;
		ptr->character = ((struct args *)data)->input_chars[i];
		if (i + 1 == loc + (size / threads)) {
			goto GOtem;
		}
		next_char = ((struct args *)data)->input_chars[i + 1];
		while (ptr->character == next_char) {
			i++;
			if (i == loc + (size / threads)) {
				break;
			}
			count++;
			next_char = ((struct args *)data)->input_chars[i + 1];
		}
	GOtem:
		ptr->occurence = count;
		localFrequency[ptr->character - 97] += count;
		count = 1;
		i++;
	}

	localLengths[loc / (size / threads)] = numChar;

	pthread_barrier_wait(&barrier);

	pthread_mutex_lock(&((struct args *)data)->mutex);
	((struct args *)data)->zipped_chars_count[0] += numChar;
	for (int i = 0; i < 26; i++) {
		((struct args *)data)->char_frequency[i] += localFrequency[i];
	}
	pthread_mutex_unlock(&((struct args *)data)->mutex);

	int previousLengths = 0;
	for (int i = 0; i < loc / (size / threads); i++) {
		previousLengths += localLengths[i];
	}
	int j = 0;

	for (int i = previousLengths; i < numChar + previousLengths; i++) {
		((struct args *)data)->zipped_chars[i].character =
			localArray[j].character;
		((struct args *)data)->zipped_chars[i].occurence =
			localArray[j].occurence;
		j++;
	}

	pthread_barrier_wait(&barrier2);
	free(localArray);
	if (loc == 0) {
		free(localLengths);
		free(data);
	}
	pthread_barrier_wait(&barrier3);
	pthread_exit(NULL);
}

void pzip(int n_threads, char *input_chars, int input_chars_size,
	  struct zipped_char *zipped_chars, int *zipped_chars_count,
	  int *char_frequency)
{
	int retval;
	struct args *data = malloc(sizeof(struct args));
	data->input_chars = input_chars;
	data->input_chars_size = input_chars_size;
	data->zipped_chars = zipped_chars;
	data->zipped_chars_count = zipped_chars_count;
	data->char_frequency = char_frequency;
	data->n_threads = n_threads;
	localLengths = malloc(sizeof(int) * n_threads);

	pthread_t tid[n_threads];
	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);
	data->mutex = mutex;
	pthread_barrier_init(&barrier, NULL, n_threads);
	pthread_barrier_init(&barrier2, NULL, n_threads);
	pthread_barrier_init(&barrier3, NULL, n_threads + 1);

	for (int i = 0; i < n_threads; i++) {
		pthread_mutex_lock(&(data->mutex));
		data->location = i * (input_chars_size / n_threads);

		retval = pthread_create(&tid[i], NULL, zippy, (void *)data);
		if (retval != 0) {
			fprintf(stderr, "Error, could not create thread");
		}
	}
	pthread_barrier_wait(&barrier3);

	for (int i = 0; i < n_threads; i++) {
		retval = pthread_join(tid[i], NULL);
		if (retval != 0) {
			fprintf(stderr, "Error, could not create thread");
		}
	}

	return;
}
