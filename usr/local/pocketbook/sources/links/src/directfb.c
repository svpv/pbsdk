/* directfb.c
 * DirectFB graphics driver
 * (c) 2002 Sven Neumann <sven@directfb.org>
 *
 * This file is a part of the Links program, released under GPL.
 */

/* TODO:
 * - Store window size as driver params (?)
 * - Fix wrong colors on big-endian systems (fixed?)
 * - Make everything work correctly ;-)
 *
 * KNOWN PROBLEMS:
 * - If mouse drags don't work for you, update DirectFB
 *   (the upcoming 0.9.14 release fixes this).
 */


#include "cfg.h"

#ifdef GRDRV_DIRECTFB

#ifdef TEXT
#undef TEXT
#endif

#include <netinet/in.h>  /* for htons */
#include "links.h"

#define FOCUSED_OPACITY    0xFF
#define UNFOCUSED_OPACITY  0xC0

extern struct graphics_driver directfb_driver;

static int                    event_timer = -1;

static void directfb_flip_surface(void* pointer);
static void directfb_check_events(void* pointer);

static void directfb_add_to_table(struct graphics_device* gd);
static void directfb_remove_from_table(struct graphics_device* gd);

static struct graphics_device* sgd = NULL;

char text_url[1024] = {0};
char displayed_url[1024] = {0};
char entered_text[2048] = {0};

struct session* current_ses = NULL;
struct f_data_c* current_f = NULL;

static ifont* current_font = NULL;

static void handle_mouse_event(int x, int y, int flags);

extern const ibitmap btn_back;
extern const ibitmap btn_next;
extern const ibitmap btn_close;
extern const ibitmap btn_stop;
extern const ibitmap btn_reload;

// panel pictures
static int top_margin;

struct p_button
{
	int w;
	int h;
	int curpic;
	struct ibitmap** icon_array;
};

struct ibitmap* menu_bback_pic[] = {&btn_back, NULL};
struct ibitmap* menu_bnext_pic[] = {&btn_next, NULL};
struct ibitmap* menu_bnext_update[] = {&btn_reload, &btn_stop, NULL};
struct ibitmap* menu_bnext_close[] = {&btn_close, NULL};

struct p_button menu_button_bar[] = {
	{40, 40, 1, &menu_bback_pic},
	{40, 40, 1, &menu_bnext_pic},
	{40, 40, 1, &menu_bnext_update},
	{40, 40, 1, &menu_bnext_close}
};
int menu_button_count = sizeof(menu_button_bar) / sizeof(menu_button_bar[0]);

#define BUTTON_OFFSET 5

int calc_bar_width()
{
	int result = BUTTON_OFFSET;
	int iter = 0;
	while (iter < menu_button_count)
	{
		result += menu_button_bar[iter].w;
		iter++;
	}
	return result;
}

int draw_app_button(int button_num, int picnum, int inverted, int nofastupdate)
{
	int result = 0, iter = 0, posx = BUTTON_OFFSET, piciter;

	if (button_num < menu_button_count)
	{
		struct p_button* my_button = menu_button_bar;
		piciter = picnum;

		while (iter < button_num)
		{
			posx += my_button->w;
			my_button++;
			iter++;
		}

		ibitmap** pic_array = my_button->icon_array;
		// search picture in array. do not pos directly
		while (--piciter && *pic_array++);
		// check search result
		if (!piciter && *pic_array)
		{
			my_button->curpic = picnum;
			if (nofastupdate)
			{
				// use update manager
				manSetClip(posx, BUTTON_OFFSET, my_button->w, my_button->h);

				if (inverted)
					manInvertAreaBW(posx, BUTTON_OFFSET, my_button->w, my_button->h);
				else
					manDrawBitmapNew(posx + (abs(my_button->w - (*pic_array)->width) >> 1),
					BUTTON_OFFSET + (abs(my_button->h - (*pic_array)->height) >> 1),
					*pic_array);
			}
			else
			{
				// manual draw
				manSetClip(posx, BUTTON_OFFSET, my_button->w, my_button->h);
				if (inverted)
				{
					manInvertAreaBW(posx, BUTTON_OFFSET, my_button->w, my_button->h);
					manUpdateScreen();
					manInvertAreaBW(posx, BUTTON_OFFSET, my_button->w, my_button->h);
					manUpdateScreen();
				}
				else
				{
					manDrawBitmapNew(posx + (abs(my_button->w - (*pic_array)->width) >> 1),
					BUTTON_OFFSET + (abs(my_button->h - (*pic_array)->height) >> 1),
					*pic_array);
					manUpdateScreen();
					//PartialUpdateBW(posx, BUTTON_OFFSET, my_button->w, my_button->h);
				}
			}
			result = 1;
		}
		else
		{
			fprintf(stderr, "ERROR: draw_app_button() - no picture\n");
		}
	}
	return result;
}

int get_button_last_state(int button_num)
{
	return button_num < menu_button_count ? menu_button_bar[button_num].curpic : 0;
}

void set_button_state(int button_num, int picnum)
{
	if (button_num < menu_button_count)
		menu_button_bar[button_num].curpic = picnum;
	return;
}

int get_button_by_touch(int x, int y)
{
	int iter = 0, posx = BUTTON_OFFSET;
	struct p_button* my_button = menu_button_bar;
	while (iter < menu_button_count &&
	!(x >= posx && x <= (posx + my_button->w) && y >= BUTTON_OFFSET && y <= (BUTTON_OFFSET + my_button->h)))
	{
		posx += my_button->w;
		my_button++;
		iter++;
	}
	return (iter != menu_button_count) ? iter : -1;
}

int get_edit_rect(irect* edit_rect)
{
	int result = 0;
	if (edit_rect)
	{
		edit_rect->x = calc_bar_width() + BUTTON_OFFSET;
		edit_rect->y = BUTTON_OFFSET;
		edit_rect->w = ScreenWidth() - edit_rect->x - BUTTON_OFFSET;
		edit_rect->h = top_margin - edit_rect->y - BUTTON_OFFSET;
		result = 1;
	}
	return result;
}

void draw_app_url(char* url, int nofastupdate)
{
	irect edit_rect;
	if (get_edit_rect(&edit_rect))
	{
		manSetClip(edit_rect.x, edit_rect.y, edit_rect.w, edit_rect.h);
		manFillArea(edit_rect.x, edit_rect.y, edit_rect.w, edit_rect.h, WHITE);
		manDrawRect(edit_rect.x, edit_rect.y, edit_rect.w, edit_rect.h, BLACK);
		manDrawTextRect(current_font, BLACK, edit_rect.x + 5, edit_rect.y + 1, edit_rect.w - 10, edit_rect.h - 2, url, ALIGN_LEFT | VALIGN_MIDDLE);

		if (!nofastupdate)
			manUpdateScreen();
	}
	return;
}

void draw_app_menu(int ori_changed)
{
	int iter;
	if (!bookland_on)
	{
		if (ori_changed)
		{
			manSetClip(0, 0, ScreenWidth(), top_margin);
			manFillArea(0, 0, ScreenWidth(), top_margin, WHITE);
			manDrawLine(0, top_margin - 1, ScreenWidth(), top_margin - 1, BLACK);
			iter = 0;
			while (iter < menu_button_count)
			{
				draw_app_button(iter, 1, 0, 1);
				iter++;
			}
			if (displayed_url[0])
				draw_app_url(&displayed_url[0], 1);
		}
	}
}

void update_menu_status(const char* url, int onlyurl, int state)
{
	if (!bookland_on)
	{
		if (url)
		{
			memcpy(&displayed_url[0], url, strnlen(url, 1023));
			displayed_url[strnlen(url, 1023)] = 0;

			//if (displayed_url[0])
			//	draw_app_url(&displayed_url[0], 1);
		}

		if (!onlyurl)
		{
			int reload_but_state = get_button_last_state(2);
			if (reload_but_state)
			{
				if (reload_but_state == 1 && state != S_OK)
				{
					// set button to stop
					draw_app_button(2, 2, 0, 0);
					if (displayed_url[0])
						draw_app_url(&displayed_url[0], 0);
				}
				else if (reload_but_state == 2 && state == S_OK)
				{
					// set button to reload
					draw_app_button(2, 1, 0, 0);
					if (displayed_url[0])
						draw_app_url(&displayed_url[0], 0);

					// make full update
					manFullUpdate();
				}
			}
		}
	}
}

void keep_alive_timer(void)
{
	install_timer(1, keep_alive_timer, NULL);
}

void kbd_handler(char* text)
{
	if (strnlen(text_url, 1000) > 0)
	{
		memcpy(&displayed_url[0], text_url, strnlen(text_url, 1023));
		displayed_url[strnlen(text_url, 1023)] = 0;
		draw_app_menu(0);
		sgd->keyboard_handler(sgd, '%', 0);
	}
}

void kbd_text_handler(char* text)
{
	struct link* link;
	struct form_state* fs;

	if (current_f->f_data && current_f->vs->current_link != -1 && current_f->vs->current_link < current_f->f_data->nlinks)
	{
		link = &current_f->f_data->links[current_f->vs->current_link];
		if ((fs = find_form_state(current_f, link->form)) != NULL)
		{
			mem_free(fs->value);
			fs->value = stracpy((char*)(&entered_text[0]));
			current_ses->locked_link = 0;
			sgd->redraw_handler(sgd, &(sgd->size));
		}
	}
	current_ses = NULL;
	current_f = NULL;
}

extern void call_virtual_keyboard(struct session* ses, struct f_data_c* f)
{
	struct link* link;
	struct form_state* fs;
	
	if (f->f_data &&
	f->vs->current_link != -1 &&
	f->vs->current_link < f->f_data->nlinks)
	{
		link = &f->f_data->links[f->vs->current_link];
		if ((fs = find_form_state(f, link->form)) != NULL)
		{
			if (get_button_last_state(2) == 1)
			{
				memcpy(entered_text, fs->value, strnlen(fs->value, 2048));
				entered_text[strnlen(fs->value, 2048)] = '\0';
				manSetClip(0, 0, ScreenWidth(), ScreenHeight());
				manUpdateScreen();
				OpenKeyboard(" ", entered_text, link->form->maxlength > 2048 ? 2048 : link->form->maxlength, link->form->type == FC_PASSWORD ? KBD_PASSWORD : 0, kbd_text_handler);
				current_ses = ses;
				current_f = f;
			}
		}
	}
}

int longkey = 0;

int ink_main(int type, int par1, int par2)
{
	int key = 0;
	int flag = 0;

	switch (type)
	{
		case EVT_INIT:
			if (!current_font)
				current_font = OpenFont(DEFAULTFONTI, 16, 0);

			draw_app_menu(1);
			links_timer_loop();
			keep_alive_timer();
			break;

		case EVT_ORIENTATION:
			if (sgd)
			{
				SetOrientation(par1);
				directfb_driver.x = ScreenWidth();
				directfb_driver.y = ScreenHeight() - top_margin - 1;
				sgd->size.x2 = directfb_driver.x - 1;
				sgd->size.y2 = directfb_driver.y - 1;
				draw_app_menu(1);
				sgd->resize_handler(sgd);
				sgd->redraw_handler(sgd, &(sgd->size));
			}
			break;

		case EVT_POINTERDOWN:
			handle_mouse_event(par1, par2, B_DOWN | B_LEFT);
			break;

		case EVT_POINTERUP:
			handle_mouse_event(par1, par2, B_UP | B_LEFT);
			break;

		case EVT_KEYUP:
			if (longkey) longkey = 0;
			else
			{
				switch (par1)
				{
					case KEY_UP:
						key = KBD_UP;
						break;
					case KEY_DOWN:
						key = KBD_DOWN;
						break;
					case KEY_OK:
						key = KBD_ENTER;
						break;
					case KEY_BACK:
						key = KBD_CTRL_C;
						break;
					case KEY_PREV:
						key = KBD_PAGE_DOWN;
						break;
					case KEY_NEXT:
						key = KBD_PAGE_UP;
						break;
					case KEY_LEFT:
					case KEY_PREV2:
						key = '[';
						break;
					case KEY_RIGHT:
					case KEY_NEXT2:
						key = ']';
						break;
					default:
						//if ( 1 == keyboard_opened )
						//{
						//  key = par1;
						//}
						break;
				}
				if (0 != key)
				{
					sgd->keyboard_handler(sgd, key, flag);
					key = 0;
					flag = 0;
				}
			}
			break;
		case EVT_KEYREPEAT:
			if (par2 == 1)
			{
				switch (par1)
				{
					case KEY_UP:
						if (!bookland_on)
						{
							// Open keyboard for url entering
							manSetClip(0, 0, ScreenWidth(), ScreenHeight());
							manUpdateScreen();
							memcpy(&text_url[0], &displayed_url[0], strnlen(displayed_url, 1023));
							text_url[strnlen(displayed_url, 1023)] = 0;
							OpenKeyboard("Please enter URL", text_url, 1000, KBD_URL, kbd_handler);
						}
						break;
					case KEY_LEFT:
						key = KBD_LEFT;
						break;
					case KEY_RIGHT:
						key = KBD_RIGHT;
						break;
					case KEY_BACK:
					case KEY_PREV:
						key = KBD_CTRL_C;
						break;
					default:
						break;
				}
				if (0 != key)
				{
					sgd->keyboard_handler(sgd, key, flag);
					key = 0;
					flag = 0;
					longkey = 1;
				}
			}
			break;
		default:
			break;
	}
	return 0;
}

static void handle_mouse_event(int x, int y, int flags)
{
	int cell_width = 0;
	cell_width = top_margin;
	irect edit_rect;

	if (y >= top_margin)
	{
		sgd->mouse_handler(sgd, x, y - top_margin - 1, flags);
	}
	else
	{
		if (flags == (B_DOWN | B_LEFT))
		{
			int button_num = get_button_by_touch(x, y);
			if (button_num != -1)
			{
				int button_pic = get_button_last_state(button_num);
				if (button_pic)
				{
					draw_app_button(button_num, button_pic, 1, 0);
					switch (button_num)
					{
						case 0:
							// history back
							sgd->keyboard_handler(sgd, KBD_LEFT, 0);
							break;
						case 1:
							// history forward
							sgd->keyboard_handler(sgd, KBD_RIGHT, 0);
							break;
						case 2:
							switch (button_pic)
							{
								case 1:
									// reload page
									sgd->keyboard_handler(sgd, 'R', KBD_CTRL);
									break;
								case 2:
									// stop reload
									sgd->keyboard_handler(sgd, 'S', KBD_CTRL);
									break;
								default:
									break;
							}
							break;
						case 3:
							// exit app
							ink_main(EVT_KEYUP, KEY_BACK, 0);
							break;
						default:
							break;
					}
				}
			}
			else if (get_edit_rect(&edit_rect) &&
			x >= edit_rect.x &&
			x <= edit_rect.x + edit_rect.w &&
			y >= edit_rect.y &&
			y <= edit_rect.y + edit_rect.h)
			{
				if (get_button_last_state(2) == 1)
				{
					manSetClip(0, 0, ScreenWidth(), ScreenHeight());
					manUpdateScreen();
					memcpy(&text_url[0], &displayed_url[0], strnlen(displayed_url, 1023));
					text_url[strnlen(displayed_url, 1023)] = 0;
					OpenKeyboard("Please enter URL", text_url, 1000, KBD_URL, kbd_handler);
				}
				// check for adress bar
			}
		}
	}
}

static unsigned char* directfb_fb_init_driver(unsigned char* param, unsigned char* display)
{
	if (bookland_on) top_margin = 1;
	else top_margin = 50;
	//directfb_driver.depth = 708;
	directfb_driver.depth = 65;

	directfb_driver.x = ScreenWidth();
	directfb_driver.y = ScreenHeight() - top_margin - 1;
	return NULL;
}

static struct graphics_device* directfb_init_device(void)
{

	sgd = mem_alloc(sizeof(struct graphics_device));

	sgd->size.x2 = directfb_driver.x - 1;
	sgd->size.y2 = directfb_driver.y - 1;

	sgd->size.x1 = 0;
	sgd->size.y1 = 0;

	sgd->clip.x1 = sgd->size.x1;
	sgd->clip.x2 = sgd->size.x2;
	sgd->clip.y1 = sgd->size.y1;
	sgd->clip.y2 = sgd->size.y2;

	manSetClip(sgd->size.x1, sgd->size.y1 + top_margin + 1, sgd->size.x2 - sgd->size.x1, sgd->size.y2 - sgd->size.y1);
	//SetClip(sgd->size.x1, sgd->size.y1 + top_margin + 1, sgd->size.x2 - sgd->size.x1, sgd->size.y2 - sgd->size.y1);

	sgd->drv = &directfb_driver;
	sgd->driver_data = NULL;
	sgd->user_data   = NULL;

	return sgd;
}

static void directfb_shutdown_device(struct graphics_device* gd)
{
	mem_free(sgd);
	sgd = NULL;
	CloseApp();
}

static void directfb_shutdown_driver(void)
{
}

static unsigned char* directfb_get_driver_param(void)
{
	return NULL;
}

static int directfb_get_empty_bitmap(struct bitmap* bmp)
{
	//bmp->data=mem_alloc(bmp->x*bmp->y*4);
	//bmp->skip=bmp->x*4;
	bmp->data = mem_alloc(bmp->x * bmp->y * 1);
	bmp->skip = bmp->x * 1;
	bmp->flags = 0;
	//bmp->bw = 0;
	return 0;
}


static void directfb_register_bitmap(struct bitmap* bmp)
{
	//   IDirectFBSurface *surface = bmp->flags;
	//
	//   surface->Unlock (surface);
	return;
}

static void* directfb_prepare_strip(struct bitmap* bmp, int top, int lines)
{
	//   IDirectFBSurface *surface = bmp->flags;
	//
	//   surface->Lock (surface, DSLF_READ | DSLF_WRITE, &bmp->data, &bmp->skip);
	//
	return ((unsigned char*) bmp->data + top * bmp->skip);
	//      return NULL;
}

static void directfb_commit_strip(struct bitmap* bmp, int top, int lines)
{
	//   IDirectFBSurface *surface = bmp->flags;
	//
	//   surface->Unlock (surface);
	//   bmp->data = NULL;
}

static void directfb_unregister_bitmap(struct bitmap* bmp)
{
	//   IDirectFBSurface *surface = bmp->flags;
	//
	//   surface->Release (surface);
	mem_free(bmp->data);
	bmp->data = NULL;
}

static void directfb_draw_bitmap(struct graphics_device* gd, struct bitmap* bmp, int x, int y)
{
	manDrawBitmap(bmp, x, y, top_margin);
}

static void directfb_draw_bitmaps(struct graphics_device* gd, struct bitmap** bmps, int n, int x, int y)
{
	struct bitmap* bmp  = *bmps;

	if (n < 1)
		return;
	do
	{
		directfb_draw_bitmap(gd, bmp, x, y);
		bmp++;
	}
	while (--n);
}

static long directfb_get_color(int rgb)
{
	return rgb;
}

static void directfb_fill_area(struct graphics_device* gd,
int x1, int y1, int x2, int y2, long color)
{
	int w = x2 - x1;
	int h = y2 - y1;

	manFillArea(x1, y1 + top_margin + 1, w, h, color);
}

static void directfb_draw_hline(struct graphics_device* gd, int left, int y, int right, long color)
{
	manDrawLine(left, y + top_margin + 1, right, y + top_margin + 1, color);
}

static void directfb_draw_vline(struct graphics_device* gd, int x, int top, int bottom, long color)
{
	manDrawLine(x, top + top_margin + 1, x, bottom + top_margin + 1, color);
}

static void directfb_set_clip_area(struct graphics_device* gd, struct rect* r)
{
	gd->clip.x1 = r->x1;
	gd->clip.x2 = r->x2;
	gd->clip.y1 = r->y1;
	gd->clip.y2 = r->y2;

	manSetClip(r->x1, r->y1 + top_margin + 1, r->x2 - r->x1, r->y2 - r->y1);

}

static int directfb_hscroll(struct graphics_device* gd, struct rect_set** set, int sc)
{
	if (!sc)
		return 0;

	manFillArea(sgd->clip.x1, sgd->clip.y1, sgd->clip.x2 - sgd->clip.x1, gd->clip.y2 - sgd->clip.y1, 0);
	return 1;
}

static int directfb_vscroll(struct graphics_device* gd, struct rect_set** set, int sc)
{
	if (!sc)
		return 0;

	manFillArea(sgd->clip.x1, sgd->clip.y1, sgd->clip.x2 - sgd->clip.x1, gd->clip.y2 - sgd->clip.y1, 0);
	return 1;
}

struct graphics_driver directfb_driver =
{
	"directfb",
	directfb_fb_init_driver,
	directfb_init_device,
	directfb_shutdown_device,
	directfb_shutdown_driver,
	directfb_get_driver_param,
	directfb_get_empty_bitmap,
	/*directfb_get_filled_bitmap,*/
	directfb_register_bitmap,
	directfb_prepare_strip,
	directfb_commit_strip,
	directfb_unregister_bitmap,
	directfb_draw_bitmap,
	directfb_draw_bitmaps,
	directfb_get_color,
	directfb_fill_area,
	directfb_draw_hline,
	directfb_draw_vline,
	directfb_hscroll,
	directfb_vscroll,
	directfb_set_clip_area,
	dummy_block,
	dummy_unblock,
	NULL,  /*  set_title  */
	NULL,  /*  exec */
	0,   /*  depth      */
	0, 0,  /*  size       */
	0,     /*  flags      */
	0,     /*  codepage   */
	NULL,  /*  shell  */
};

static void directfb_flip_surface(void* pointer)
{
	//   DFBDeviceData *data = pointer;
	//
	//   if (!data->flip_pending)
	//     return;
	//
	//   data->surface->Flip (data->surface, &data->flip_region, 0);
	//
	//   data->flip_pending = 0;
}

static void directfb_check_events(void* pointer)
{
	//   struct graphics_device *gd   = NULL;
	//   DFBDeviceData          *data = NULL;
	//   DFBWindowEvent          event;
	//   DFBWindowEvent          next;
	//
	//   if (!events)
	//     return;
	//
	//   while (events->GetEvent (events, DFB_EVENT (&event)) == DFB_OK)
	//     {
	//       switch (event.type)
	//         {
	//         case DWET_GOTFOCUS:
	//         case DWET_LOSTFOCUS:
	//         case DWET_POSITION_SIZE:
	//         case DWET_SIZE:
	//         case DWET_KEYDOWN:
	//         case DWET_BUTTONDOWN:
	//         case DWET_BUTTONUP:
	//         case DWET_WHEEL:
	//         case DWET_MOTION:
	//           break;
	//         default:
	//           continue;
	//         }
	//
	//       if (!data || data->id != event.window_id)
	//         {
	//           gd = directfb_lookup_in_table (event.window_id);
	//           if (!gd)
	//             continue;
	//         }
	//
	//       data = gd->driver_data;
	//
	//       switch (event.type)
	//         {
	//         case DWET_GOTFOCUS:
	//           data->window->SetOpacity (data->window, FOCUSED_OPACITY);
	//           break;
	//
	//         case DWET_LOSTFOCUS:
	//           data->window->SetOpacity (data->window, UNFOCUSED_OPACITY);
	//           break;
	//
	//         case DWET_POSITION_SIZE:
	//           if (!data->mapped)
	//             {
	//               struct rect r = { 0, 0, event.w, event.h };
	//
	//               gd->redraw_handler (gd, &r);
	//               data->window->SetOpacity (data->window, FOCUSED_OPACITY);
	//               data->mapped = 1;
	//             }
	//           else
	//           /* fallthru */
	//         case DWET_SIZE:
	//           while ((events->PeekEvent (events, DFB_EVENT (&next)) == DFB_OK)   &&
	//                  (next.type == DWET_SIZE || next.type == DWET_POSITION_SIZE) &&
	//                  (next.window_id == data->id))
	//             events->GetEvent (events, DFB_EVENT (&event));
	//
	//           gd->size.x2 = event.w;
	//           gd->size.y2 = event.h;
	//           gd->resize_handler (gd);
	//           break;
	//
	//         case DWET_KEYDOWN:
	//           {
	//             int key, flag;
	//
	//             directfb_translate_key (&event, &key, &flag);
	//             if (key)
	//               gd->keyboard_handler (gd, key, flag);
	//           }
	//           break;
	//
	//         case DWET_BUTTONDOWN:
	//         case DWET_BUTTONUP:
	//           {
	//             int flags;
	//
	//             if (event.type == DWET_BUTTONUP)
	//               {
	//                 flags = B_UP;
	//                 data->window->UngrabPointer (data->window);
	//               }
	//             else
	//               {
	//                 flags = B_DOWN;
	//                 data->window->GrabPointer (data->window);
	//               }
	//
	//             switch (event.button)
	//               {
	//               case DIBI_LEFT:
	//                 flags |= B_LEFT;
	//                 break;
	//               case DIBI_RIGHT:
	//                 flags |= B_RIGHT;
	//                 break;
	//               case DIBI_MIDDLE:
	//                 flags |= B_MIDDLE;
	//                 break;
	//               default:
	//                 continue;
	//               }
	//
	//             gd->mouse_handler (gd, event.x, event.y, flags);
	//           }
	//           break;
	//
	//         case DWET_WHEEL:
	//           gd->mouse_handler (gd, event.x, event.y,
	//                              B_MOVE |
	//                              (event.step > 0 ? B_WHEELUP : B_WHEELDOWN));
	//    break;
	//
	//         case DWET_MOTION:
	//           {
	//             int flags;
	//
	//             while ((events->PeekEvent (events, DFB_EVENT (&next)) == DFB_OK) &&
	//                    (next.type      == DWET_MOTION)                           &&
	//                    (next.window_id == data->id))
	//               events->GetEvent (events, DFB_EVENT (&event));
	//
	//             switch (event.buttons)
	//               {
	//               case DIBM_LEFT:
	//                 flags = B_DRAG | B_LEFT;
	//                 break;
	//               case DIBM_RIGHT:
	//                 flags = B_DRAG | B_RIGHT;
	//                 break;
	//               case DIBM_MIDDLE:
	//                 flags = B_DRAG | B_MIDDLE;
	//                 break;
	//               default:
	//                 flags = B_MOVE;
	//                 break;
	//               }
	//
	//             gd->mouse_handler (gd, event.x, event.y, flags);
	//           }
	//           break;
	//
	//         case DWET_CLOSE:
	//           gd->keyboard_handler (gd, KBD_CLOSE, 0);
	//           break;
	//
	//         default:
	//           break;
	//         }
	//     }
	//
	//   event_timer = install_timer (20, directfb_check_events, events);
}

static void directfb_add_to_table(struct graphics_device* gd)
{
	//   DFBDeviceData           *data = gd->driver_data;
	//   struct graphics_device **devices;
	//   int i;
	//
	//   i = data->id % DIRECTFB_HASH_TABLE_SIZE;
	//
	//   devices = directfb_hash_table[i];
	//
	//   if (devices)
	//     {
	//       int c = 0;
	//
	//       while (devices[c++])
	//         if (c == MAXINT) overalloc();
	//
	//       if ((unsigned)c > MAXINT / sizeof(void *) - 1) overalloc();
	//       devices = mem_realloc (devices, (c + 1) * sizeof (void *));
	//       devices[c-1] = gd;
	//       devices[c]   = NULL;
	//     }
	//   else
	//     {
	//       devices = mem_alloc (2 * sizeof (void *));
	//       devices[0] = gd;
	//       devices[1] = NULL;
	//     }
	//
	//   directfb_hash_table[i] = devices;
}

static void directfb_remove_from_table(struct graphics_device* gd)
{
	//   DFBDeviceData           *data = gd->driver_data;
	//   struct graphics_device **devices;
	//   int i, j, c;
	//
	//   i = data->id % DIRECTFB_HASH_TABLE_SIZE;
	//
	//   devices = directfb_hash_table[i];
	//   if (!devices)
	//     return;
	//
	//   for (j = 0, c = -1; devices[j]; j++)
	//     if (devices[j] == gd)
	//       c = j;
	//
	//   if (c < 0)
	//     return;
	//
	//   memmove (devices + c, devices + c + 1, (j - c) * sizeof (void *));
	//   devices = mem_realloc (devices, j * sizeof (void *));
	//
	//   directfb_hash_table[i] = devices;
}

#endif /* GRDRV_DIRECTFB */
