/* select.c
 * Select Loop
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

/*
#define DEBUG_CALLS
*/

struct thread
{
	void (*read_func)(void*);
	void (*write_func)(void*);
	void (*error_func)(void*);
	void* data;
};

static struct thread threads[FD_SETSIZE];

static fd_set w_read;
static fd_set w_write;
static fd_set w_error;

static fd_set x_read;
static fd_set x_write;
static fd_set x_error;

static int w_max;

static int timer_id = 0;

struct timer
{
	struct timer* next;
	struct timer* prev;
	void (*func)(void*);
	void* data;
	ttime setup_time;
	ttime period;
	int id;
};

static struct list_head timers = {&timers, &timers};

ttime get_time(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int can_write(int fd)
{
	fd_set fds;
	struct timeval tv = {0, 0};
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	return select(fd + 1, NULL, &fds, NULL, &tv);
}

int can_read(int fd)
{
	fd_set fds;
	struct timeval tv = {0, 0};
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	return select(fd + 1, &fds, NULL, NULL, &tv);
}

long select_info(int type)
{
	int i = 0, j;
	struct cache_entry* ce;
	switch (type)
	{
		case CI_FILES:
			for (j = 0; j < (int)FD_SETSIZE; j++)
				if (threads[j].read_func || threads[j].write_func || threads[j].error_func) i++;
			return i;
		case CI_TIMERS:
			foreach(ce, timers) i++;
			return i;
		default:
			internal("cache_info: bad request");
	}
	return 0;
}

struct bottom_half
{
	struct bottom_half* next;
	struct bottom_half* prev;
	void (*fn)(void*);
	void* data;
};

struct list_head bottom_halves = { &bottom_halves, &bottom_halves };

int register_bottom_half(void (*fn)(void*), void* data)
{
	struct bottom_half* bh;
	foreach(bh, bottom_halves) if (bh->fn == fn && bh->data == data) return 0;
	bh = mem_alloc(sizeof(struct bottom_half));
	bh->fn = fn;
	bh->data = data;
	add_to_list(bottom_halves, bh);
	return 0;
}

void unregister_bottom_half(void (*fn)(void*), void* data)
{
	struct bottom_half* bh;
retry:
	foreach(bh, bottom_halves) if (bh->fn == fn && bh->data == data)
	{
		del_from_list(bh);
		mem_free(bh);
		goto retry;
	}
}

void check_bottom_halves(void)
{
	struct bottom_half* bh;
	void (*fn)(void*);
	void* data;
rep:
	if (list_empty(bottom_halves)) return;
	bh = bottom_halves.prev;
	fn = bh->fn;
	data = bh->data;
	del_from_list(bh);
	mem_free(bh);
#ifdef DEBUG_CALLS
	fprintf(stderr, "call: bh %p\n", fn);
#endif
	pr(fn(data))
	{
		free_list(bottom_halves);
		return;
	};
#ifdef DEBUG_CALLS
	fprintf(stderr, "bh done\n");
#endif
	goto rep;
}

#define CHK_BH if (!list_empty(bottom_halves)) check_bottom_halves()

void check_timers(void)
{
	struct timer* t;
	ttime current_time = get_time();
	foreach(t, timers) if (((t->setup_time > current_time) && (t->setup_time + t->period > t->setup_time)) || (t->setup_time + t->period <= current_time))
	{
		struct timer* tt;
#ifdef DEBUG_CALLS
		fprintf(stderr, "call: timer %p\n", t->func);
#endif
		pr(t->func(t->data))
		{
			del_from_list((struct timer*)timers.next);
			return;
		}

#ifdef DEBUG_CALLS
		fprintf(stderr, "timer done\n");
#endif
		CHK_BH;
		tt = t->prev;
		del_from_list(t);
		mem_free(t);
		t = tt;
	}
	else break;
	return;
}

int install_timer(ttime t, void (*func)(void*), void* data)
{
	struct timer* tm, *tt;
	tm = mem_alloc(sizeof(struct timer));
	tm->setup_time = get_time();
	tm->period = t;
	tm->data = data;
	tm->func = func;
	tm->id = timer_id++;
	foreach(tt, timers) if (tt->setup_time + tt->period >= t + tm->setup_time) break;
	add_at_pos(tt->prev, tm);

	return tm->id;
}

void kill_timer(int id)
{
	struct timer* tm;
	int k = 0;
	foreach(tm, timers) if (tm->id == id)
	{
		struct timer* tt = tm;
		del_from_list(tm);
		tm = tm->prev;
		mem_free(tt);
		k++;
	}
	if (!k) internal("trying to kill nonexisting timer");
	if (k >= 2) internal("more timers with same id");
}

void* get_handler(int fd, int tp)
{
	if (fd < 0 || fd >= (int)FD_SETSIZE)
	{
		internal("get_handler: handle %d >= FD_SETSIZE %d", fd, FD_SETSIZE);
		return NULL;
	}
	switch (tp)
	{
		case H_READ:
			return threads[fd].read_func;
		case H_WRITE:
			return threads[fd].write_func;
		case H_ERROR:
			return threads[fd].error_func;
		case H_DATA:
			return threads[fd].data;
	}
	internal("get_handler: bad type %d", tp);
	return NULL;
}

void set_handlers(int fd, void (*read_func)(void*), void (*write_func)(void*), void (*error_func)(void*), void* data)
{
	if (fd < 0 || fd >= (int)FD_SETSIZE)
	{
		internal("set_handlers: handle %d >= FD_SETSIZE %d", fd, FD_SETSIZE);
		return;
	}
	threads[fd].read_func = read_func;
	threads[fd].write_func = write_func;
	threads[fd].error_func = error_func;
	threads[fd].data = data;
	if (read_func) FD_SET(fd, &w_read);
	else
	{
		FD_CLR(fd, &w_read);
		FD_CLR(fd, &x_read);
	}
	if (write_func) FD_SET(fd, &w_write);
	else
	{
		FD_CLR(fd, &w_write);
		FD_CLR(fd, &x_write);
	}
	if (error_func) FD_SET(fd, &w_error);
	else
	{
		FD_CLR(fd, &w_error);
		FD_CLR(fd, &x_error);
	}
	if (read_func || write_func || error_func)
	{
		if (fd >= w_max) w_max = fd + 1;
	}
	else if (fd == w_max - 1)
	{
		int i;
		for (i = fd - 1; i >= 0; i--)
			if (FD_ISSET(i, &w_read) || FD_ISSET(i, &w_write) ||
			    FD_ISSET(i, &w_error)) break;
		w_max = i + 1;
	}
}

#define NUM_SIGNALS 32

struct signal_handler
{
	void (*fn)(void*);
	void* data;
	int critical;
};

static int signal_mask[NUM_SIGNALS];
static struct signal_handler signal_handlers[NUM_SIGNALS];

static int signal_pipe[2];

static void signal_break(void* data)
{
	char c;
	while (can_read(signal_pipe[0])) read(signal_pipe[0], &c, 1);
}

static void got_signal(int sig)
{
	int sv_errno = errno;
	/*fprintf(stderr, "ERROR: signal number: %d", sig);*/
	if (sig >= NUM_SIGNALS || sig < 0)
	{
		/*error("ERROR: bad signal number: %d", sig);*/
		goto ret;
	}
	if (!signal_handlers[sig].fn) goto ret;
	if (signal_handlers[sig].critical)
	{
		signal_handlers[sig].fn(signal_handlers[sig].data);
		goto ret;
	}
	signal_mask[sig] = 1;
ret:
	if (can_write(signal_pipe[1])) write(signal_pipe[1], "x", 1);
	errno = sv_errno;
}

static struct sigaction sa_zero;

void install_signal_handler(int sig, void (*fn)(void*), void* data, int critical)
{
	struct sigaction sa = sa_zero;
	if (sig >= NUM_SIGNALS || sig < 0)
	{
		internal("bad signal number: %d", sig);
		return;
	}
	if (!fn) sa.sa_handler = SIG_IGN;
	else sa.sa_handler = got_signal;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (!fn) sigaction(sig, &sa, NULL);
	signal_handlers[sig].fn = fn;
	signal_handlers[sig].data = data;
	signal_handlers[sig].critical = critical;
	if (fn) sigaction(sig, &sa, NULL);
}

void interruptible_signal(int sig, int in)
{
	struct sigaction sa = sa_zero;
	if (sig >= NUM_SIGNALS || sig < 0)
	{
		internal("bad signal number: %d", sig);
		return;
	}
	if (!signal_handlers[sig].fn) return;
	sa.sa_handler = got_signal;
	sigfillset(&sa.sa_mask);
	if (!in) sa.sa_flags = SA_RESTART;
	sigaction(sig, &sa, NULL);
}

static int check_signals(void)
{
	int i, r = 0;
	for (i = 0; i < NUM_SIGNALS; i++)
		if (signal_mask[i])
		{
			signal_mask[i] = 0;
			if (signal_handlers[i].fn)
			{
#ifdef DEBUG_CALLS
				fprintf(stderr, "call: signal %d -> %p\n", i, signal_handlers[i].fn);
#endif
				pr(signal_handlers[i].fn(signal_handlers[i].data)) return 1;
#ifdef DEBUG_CALLS
				fprintf(stderr, "signal done\n");
#endif
			}
			CHK_BH;
			r = 1;
		}
	return r;
}

static void sigchld(void* p)
{
#ifndef WNOHANG
	wait(NULL);
#else
	while (waitpid(-1, NULL, WNOHANG) > 0) ;
#endif
}

void set_sigcld(void)
{
	install_signal_handler(SIGCHLD, sigchld, NULL, 1);
}

int terminate_loop = 0;
int data_selected = 1;

struct timeval tm_select = {0, 50000};

void links_timer_loop()
{
	int n, i;

	if (!terminate_loop)
	{
		if (data_selected)
		{
			check_signals();
			check_timers();
			check_timers();
			check_timers();
			check_timers();
			if (!F)
			{
				redraw_all_terminals();
			}

			memcpy(&x_read, &w_read, sizeof(fd_set));
			memcpy(&x_write, &w_write, sizeof(fd_set));
			memcpy(&x_error, &w_error, sizeof(fd_set));
		}
		/*rep_sel:*/
		if (!data_selected || (!terminate_loop && (w_max != 0 || !list_empty(timers))))
		{
			if (!data_selected || !check_signals())
			{
				/*{
				  int i;
				  printf("\nR:");
				  for (i = 0; i < 256; i++) if (FD_ISSET(i, &x_read)) printf("%d,", i);
				  printf("\nW:");
				  for (i = 0; i < 256; i++) if (FD_ISSET(i, &x_write)) printf("%d,", i);
				  printf("\nE:");
				  for (i = 0; i < 256; i++) if (FD_ISSET(i, &x_error)) printf("%d,", i);
				  fflush(stdout);
				}*/
#ifdef DEBUG_CALLS
				fprintf(stderr, "select\n");
#endif
				if ((n = loop_select(w_max, &x_read, &x_write, &x_error, &tm_select)) >= 0)
				{
#ifdef DEBUG_CALLS
					fprintf(stderr, "select done\n");
#endif
					check_signals();
					/*printf("sel: %d\n", n);*/
					check_timers();
					i = -1;
					while (n > 0 && ++i < w_max)
					{
						int k = 0;
						/*printf("C %d : %d,%d,%d\n",i,FD_ISSET(i, &w_read),FD_ISSET(i, &w_write),FD_ISSET(i, &w_error));
						printf("A %d : %d,%d,%d\n",i,FD_ISSET(i, &x_read),FD_ISSET(i, &x_write),FD_ISSET(i, &x_error));*/
						if (FD_ISSET(i, &x_read))
						{
							if (threads[i].read_func)
							{
#ifdef DEBUG_CALLS
								fprintf(stderr, "call: read %d -> %p\n", i, threads[i].read_func);
#endif
								pr(threads[i].read_func(threads[i].data)) continue;
#ifdef DEBUG_CALLS
								fprintf(stderr, "read done\n");
#endif
								CHK_BH;
							}
							k = 1;
						}
						if (FD_ISSET(i, &x_write))
						{
							if (threads[i].write_func)
							{
#ifdef DEBUG_CALLS
								fprintf(stderr, "call: write %d -> %p\n", i, threads[i].write_func);
#endif
								pr(threads[i].write_func(threads[i].data)) continue;
#ifdef DEBUG_CALLS
								fprintf(stderr, "write done\n");
#endif
								CHK_BH;
							}
							k = 1;
						}
						if (FD_ISSET(i, &x_error))
						{
							if (threads[i].error_func)
							{
#ifdef DEBUG_CALLS
								fprintf(stderr, "call: error %d -> %p\n", i, threads[i].error_func);
#endif
								pr(threads[i].error_func(threads[i].data)) continue;
#ifdef DEBUG_CALLS
								fprintf(stderr, "error done\n");
#endif
								CHK_BH;
							}
							k = 1;
						}
						n -= k;
					}
					nopr();
					data_selected = 1;
				}
				else
				{
					data_selected = 0;
#ifdef DEBUG_CALLS
					fprintf(stderr, "select intr\n");
#endif
					if (errno != EINTR) error("ERROR: select failed: %d", errno);
				}
			}
			SetWeakTimer("LinksLoopTimer", links_timer_loop, 1);
		}
	}
	else 
	{
		// close application because browser already closed
		CloseApp();
	}
}

void select_loop(void (*init)(void))
{
	strcpy(download_dir, FLASHDIR);
	memset(&sa_zero, 0, sizeof sa_zero);
	memset(signal_mask, 0, sizeof signal_mask);
	memset(signal_handlers, 0, sizeof signal_handlers);
	FD_ZERO(&w_read);
	FD_ZERO(&w_write);
	FD_ZERO(&w_error);
	w_max = 0;

	ignore_signals();
	if (c_pipe(signal_pipe))
	{
		error("ERROR: can't create pipe for signal handling");
		retval = RET_FATAL;
		return;
	}
	fcntl(signal_pipe[0], F_SETFL, O_NONBLOCK);
	fcntl(signal_pipe[1], F_SETFL, O_NONBLOCK);
	set_handlers(signal_pipe[0], signal_break, NULL, NULL, NULL);
	init();
	CHK_BH;

	InkViewMain(ink_main);

#ifdef DEBUG_CALLS
	fprintf(stderr, "exit loop\n");
#endif
	nopr();
}
