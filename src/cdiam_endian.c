#include <stdio.h>
#define BIG_ENDIAN 0
#define LITTLE_ENDIAN 1
int endian() {
	short int word = 0x0001;
	char *byte = (char *) &word;
	return (byte[0] ? LITTLE_ENDIAN : BIG_ENDIAN);
}

int main(int argc, char **argv)
{
	int value;
	value = endian();
	printf("#ifndef __CDIAM_ENDIAN__\n");
	printf("#define __CDIAM_ENDIAN__\n");
	if (value == 1) {
		printf("#undef WORDS_BIGENDIAN\n");
	} else {
		printf("#define WORDS_BIGENDIAN 1\n");
	}
	printf("#endif\n");
	return 0;
}

