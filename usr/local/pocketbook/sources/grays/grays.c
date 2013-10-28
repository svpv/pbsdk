#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <dlfcn.h>
#include "inkview.h"

int main_handler(int type, int par1, int par2) {

	int i;

	if (type == EVT_SHOW) {

		ClearScreen();
		FullUpdate();
		DrawRect(10, 18, 580, 104, 0);
		for(i=0; i<16; i++) {
			FillArea(12+i*36, 20, 36, 100, i*0x111111);
		}
		FullUpdate();
		FineUpdate();

	}

	if (type == EVT_KEYPRESS) {

		CloseApp();

	}

	return 0;

}

int main(int argc, char **argv) {

	InkViewMain(main_handler);
	return 0;
}

