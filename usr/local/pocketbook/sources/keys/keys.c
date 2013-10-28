#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "inkview.h"

ifont* pFont = NULL;

static icanvas* pCanvas;

int main_handler(int type, int par1, int par2)
{
	switch( type )
	{
	    case EVT_INIT:
	        {
	    	    pCanvas = GetCanvas();
	    	    printf("width: %d\n", pCanvas->width);
	    	    printf("height: %d\n", pCanvas->height);
	    	    printf("scanline: %d\n", pCanvas->scanline);
	    	    printf("depth: %d\n", pCanvas->depth);
	        	pFont = OpenFont(DEFAULTFONT, 32, 1);
	        }
	    	break;

	    case EVT_SHOW:
			ClearScreen();

        	int x,y;
        	for ( x=0; x<pCanvas->width; ++x )
        		for ( y=0; y<pCanvas->height; ++y )
        		{
        			*(pCanvas->addr + x + y*pCanvas->scanline ) = 128;
        		}

			FullUpdate();
			break;

	    case EVT_EXIT:
	    	CloseFont(pFont); pFont = NULL;
	    	break;

	    case EVT_KEYPRESS:
	        {
	        	char buf[64];
        	    snprintf( buf, sizeof(buf)/sizeof(buf[0]), "Key: %d(0x%x)\n", par1, par1 );
                SetFont(pFont,BLACK);
        	    ClearScreen();
        	    DrawTextRect(0, 0, ScreenWidth(), ScreenHeight(), buf, ALIGN_CENTER | VALIGN_MIDDLE);
        	    FullUpdate();
	        }
        	break;

	    default:
	    	break;
	}

	return 0;

}

int main(int argc, char **argv)
{
	InkViewMain(main_handler);
	return 0;
}

