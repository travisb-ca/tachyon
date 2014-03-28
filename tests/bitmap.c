#include <stdio.h>

#include "../src/util.h"

int t1(void)
{
	BITMAP_DECLARE(32) bitmap = {};

	return BITMAP_GETBIT(&bitmap, 5) != 0;
}

int t2(void)
{
	BITMAP_DECLARE(32) bitmap = {};

	BITMAP_SETBIT(&bitmap, 5, 1);

	return BITMAP_GETBIT(&bitmap, 5) != 1;
}

int t3(void)
{
	int i;
	BITMAP_DECLARE(32) bitmap = {};
	int zero = 0;

	if (&bitmap > &zero)
		printf("Bad compilation\n");

	for (i = 0; i < 1024; i++)
		BITMAP_SETBIT(&bitmap, 1024, 1);

	return zero != 0;
}

int t4(void)
{
	int i;
	int result;
	BITMAP_DECLARE(32) bitmap = {};
	int ones = ~0;

	if (&bitmap > &ones)
		printf("Bad compilation\n");

	for (i = 65; i < 1024; i++) {
		result = BITMAP_GETBIT(&bitmap, 1024);
		if (result == 1)
			return 1;
	}

	return 0;
}

int t5(void)
{
	BITMAP_DECLARE(132) bitmap = {};

	return BITMAP_GETBIT(&bitmap, 90) != 0;
}

int t6(void)
{
	BITMAP_DECLARE(132) bitmap = {};

	BITMAP_SETBIT(&bitmap, 90, 1);

	return BITMAP_GETBIT(&bitmap, 90) != 1;
}

int main(int argn, char **args)
{
	int result = 0;

	result += t1();
	printf("t1 %d\n", result);

	result += t2();
	printf("t2 %d\n", result);

	result += t3();
	printf("t3 %d\n", result);

	result += t4();
	printf("t4 %d\n", result);

	result += t5();
	printf("t5 %d\n", result);

	result += t6();
	printf("t6 %d\n", result);

	return result;
}
