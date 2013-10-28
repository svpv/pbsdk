#include <inkview.h>
#include "links.h"
#include "pthread.h"

#define PBSC_NOUPDATE   0
#define PBSC_PARTIALUPBW  1
#define PBSC_PARTIALUP    2
#define PBSC_FULLUPDATE   3

static irect man_updaterect = {0, 0, 0, 0, PBSC_NOUPDATE};
static ttime time_noupdate = 0;

void manUpdateReset()
{
	// reset update struct
	ClearTimer(manUpdateScreen);
	memset(&man_updaterect, 0, sizeof(man_updaterect));
}

void manUpdateScreen()
{
	time_noupdate = get_time();

	switch (man_updaterect.flags)
	{
		case PBSC_PARTIALUPBW:
			PartialUpdateBW(man_updaterect.x, man_updaterect.y, man_updaterect.w, man_updaterect.h);
			fprintf(stderr, "PartialUpdateBW %d, %d, %d, %d\n", man_updaterect.x, man_updaterect.y, man_updaterect.w, man_updaterect.h);
			break;
		case PBSC_PARTIALUP:
			PartialUpdate(man_updaterect.x, man_updaterect.y, man_updaterect.w, man_updaterect.h);
			fprintf(stderr, "PartialUpdate %d, %d, %d, %d\n", man_updaterect.x, man_updaterect.y, man_updaterect.w, man_updaterect.h);
			break;
		case PBSC_FULLUPDATE:
			FullUpdate();
			fprintf(stderr, "FullUpdate %d, %d, %d, %d\n", man_updaterect.x, man_updaterect.y, man_updaterect.w, man_updaterect.h);
			break;
		default:
			break;
	}
	manUpdateReset();
	return;
}

int getUpdateType(int color)
{
	int r, g, b;
	int result = PBSC_PARTIALUP;

	r = (color & 0xFF0000) >> 16;
	g = (color & 0xFF00) >> 8;
	b = (color & 0xFF);
	if ((!r || r == 0xFF) &&
	    (!g || g == 0xFF) &&
	    (!b || b == 0xFF))
		result = PBSC_PARTIALUPBW;

	return result;
}

void calcUpdateArea(int x, int y, int w, int h, int update_type)
{
	icanvas* current_canvas = GetCanvas();
	if (x < current_canvas->clipx1)
		x = current_canvas->clipx1;
	if (x + w >= current_canvas->clipx2)
		w = current_canvas->clipx2 - x + 1;
	if (y < current_canvas->clipy1)
		y = current_canvas->clipy1;
	if (y + h >= current_canvas->clipy2)
		h = current_canvas->clipy2 - y + 1;

	if (x >= 0 && y >= 0 && w > 0 && h > 0)
	{
		if (man_updaterect.flags == PBSC_NOUPDATE)
		{
			man_updaterect.x = x;
			man_updaterect.y = y;
			man_updaterect.w = w;
			man_updaterect.h = h;
			time_noupdate = get_time();
		}
		else
		{
			if (x < man_updaterect.x)
			{
				man_updaterect.w += man_updaterect.x - x;
				man_updaterect.x = x;
			}
			if (y < man_updaterect.y)
			{
				man_updaterect.h += man_updaterect.y - y;
				man_updaterect.y = y;
			}
			if ((x + w) > (man_updaterect.x + man_updaterect.w))
				man_updaterect.w = x + w - man_updaterect.x;
			if ((y + h) > (man_updaterect.y + man_updaterect.h))
				man_updaterect.h = y + h - man_updaterect.y;
		}
		if (update_type > man_updaterect.flags)
			man_updaterect.flags = update_type;

		if ((time_noupdate + 1000) < get_time())
		{
			ClearTimer(manUpdateScreen);
			manUpdateScreen();
		}
		else SetHardTimer("LinksScreenTimer", manUpdateScreen, 400);
	}
	return;
}

void manUpdateArea(int x, int y, int w, int h)
{
	calcUpdateArea(x, y, w, h, PBSC_PARTIALUPBW);
}

void manSetClip(int x, int y, int w, int h)
{
	SetClip(x, y, w, h);
	return;
}

void manFullUpdate()
{
	calcUpdateArea(0, 0, ScreenWidth(), ScreenHeight(), PBSC_FULLUPDATE);
	manUpdateScreen();
	return;
}

void manClearScreen()
{
	ClearScreen();
	calcUpdateArea(0, 0, ScreenWidth(), ScreenHeight(), PBSC_FULLUPDATE);
	return;
}

void manDrawPixel(int x, int y, int color)
{
	DrawPixel(x, y, color);
	calcUpdateArea(x, y, 1, 1, getUpdateType(color));
	return;
}

void manDrawLine(int x1, int y1, int x2, int y2, int color)
{
	int x, y, w, h;
	if (x1 < x2)
	{
		x = x1;
		w = x2 - x1;
	}
	else
	{
		x = x2;
		w = x1 - x2;
	}
	if (y1 < y2)
	{
		y = y1;
		h = y2 - y1;
	}
	else
	{
		y = y2;
		h = y1 - y2;
	}
	DrawLine(x1, y1, x2, y2, color);
	calcUpdateArea(x, y, w, h, getUpdateType(color));
	return;
}

void manFillArea(int x, int y, int w, int h, int color)
{
	FillArea(x, y, w , h, color);
	calcUpdateArea(x, y, w, h, getUpdateType(color));
	return;
}

void manInvertArea(int x, int y, int w, int h)
{
	InvertArea(x, y, w , h);
	calcUpdateArea(x, y, w, h, PBSC_PARTIALUP);
	return;
}

void manInvertAreaBW(int x, int y, int w, int h)
{
	InvertAreaBW(x, y, w, h);
	calcUpdateArea(x, y, w, h, PBSC_PARTIALUPBW);
	return;
}

void manDrawBitmapNew(int x, int y, void* b)
{
	ibitmap* pic = (ibitmap*) b;
	DrawBitmap(x, y, pic);
	calcUpdateArea(x, y, pic->width, pic->height, PBSC_PARTIALUP);
}

void manDrawRect(int x, int y, int w, int h, int color)
{
	DrawRect(x, y, w, h, color);
	calcUpdateArea(x, y, w, h, getUpdateType(color));
	return;
}

char* manDrawTextRect(ifont* tfont, int color, int x, int y, int w, int h, const char* s, int flags)
{
	if (tfont)
		SetFont(tfont, color);
	DrawTextRect(x, y, w, h, s, flags);
	calcUpdateArea(x, y, w, h, getUpdateType(color));
}

void manDrawBitmap(struct bitmap* bmp, int x, int y, int top_margin)
{
	int i = 0;
	unsigned char* canvas_addr = NULL;
	unsigned char* bmp_addr = NULL;
	int line_size = 0;
	int line_count = 0;
	int start_y = 0;
	int start_x = 0;
	int pixel_depth = 0;
	int copy_size = 0;

	icanvas* current_canvas = NULL;
	current_canvas = GetCanvas();

	if ((x <= current_canvas->clipx2) &&
		((y + top_margin + 1) <= current_canvas->clipy2) &&
		((x + bmp->x) >= current_canvas->clipx1) &&
		((y + top_margin + 1 + bmp->y) >= current_canvas->clipy1))
	{
		if (current_canvas->depth == 8)
		{

			pixel_depth = 1;

			line_count = (y + top_margin + 1 + bmp->y) > (current_canvas->clipy2 + 1) ? (current_canvas->clipy2 + 1 - y - top_margin - 1) : bmp->y;
			line_size = (x + bmp->x) > (current_canvas->clipx2 + 1) ? (current_canvas->clipx2 + 1 - x) : bmp->x;

			start_y = (y + top_margin + 1)  < current_canvas->clipy1 ? (current_canvas->clipy1 - (y + top_margin + 1)) : 0;
			start_x = x < current_canvas->clipx1 ? (current_canvas->clipx1 - x) : 0;

			line_count -= start_y;
			line_size -= start_x;

			canvas_addr = current_canvas->addr + (x + start_x) * pixel_depth + (start_y + y + top_margin + 1) * current_canvas->scanline;
			bmp_addr = bmp->data + start_y * bmp->skip + start_x * 1;

			copy_size = line_size * pixel_depth;

			for (i = 0; i < line_count; i++)
			{
				memcpy(canvas_addr, bmp_addr, copy_size);
				canvas_addr += current_canvas->scanline;
				bmp_addr += bmp->skip;
			}
			calcUpdateArea(x + start_x, y + start_y + top_margin + 1, line_size, line_count, /*bmp->bw ? PBSC_PARTIALUPBW : */PBSC_PARTIALUP);
		}
	}
}

