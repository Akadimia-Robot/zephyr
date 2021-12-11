/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Tiny, but not-as-primitive-as-it-looks implementation of something
 * like s/n/printf().  Handles %d, %x, %p, %c and %s only, allows a
 * "l" qualifier on %d and %x (and silently ignores one %s/%c/%p).
 * Accepts, but ignores, field width and precision values that match:
 * the regex: [0-9]*\.?[0-9]*
 */

struct _pfr {
	char *buf;
	size_t len;
	size_t idx;
};

/* Set this function pointer to something that generates output */
static void (*z_putchar)(int c);

static void pc(struct _pfr *r, char c)
{
	if (r->buf != NULL) {
		if (r->idx <= r->len) {
			r->buf[r->idx] = c;
		}
	} else {
		z_putchar((int)c);
	}
	r->idx++;
}

static void prdec(struct _pfr *r, long v)
{
	if (v < 0) {
		pc(r, '-');
		v = -v;
	}

	char digs[11U * sizeof(long) / 4u];
	size_t i = sizeof(digs) - 1U;

	digs[i--] = '\0';
	while ((v != 0) || (i == 9U)) {
		digs[i--] = '0' + (v % 10);
		v /= 10;
	}

	while (digs[++i] != '\0') {
		pc(r, digs[i]);
	}
}

static void endrec(struct _pfr *r)
{
	if ((r->buf != NULL) && (r->idx < r->len)) {
		r->buf[r->idx] = '\0';
	}
}

static size_t vpf(struct _pfr *r, const char *f, va_list ap)
{
	for (/**/; *f != '\0'; f++) {
		bool islong = false;

		if (*f != '%') {
			pc(r, *f);
			continue;
		}

		if (f[1] == 'l') {
			islong = sizeof(long) > 4U;
			f++;
		}

		/* Ignore (but accept) field width and precision values */
		while (f[1] >= '0' && f[1] <= '9') {
			f++;
		}
		if (f[1] == '.') {
			f++;
		}
		while (f[1] >= '0' && f[1] <= '9') {
			f++;
		}

		switch (*(++f)) {
		case '\0':
			return r->idx;
		case '%':
			pc(r, '%');
			break;
		case 'c':
			pc(r, (char)va_arg(ap, int));
			break;
		case 's': {
			char *s = va_arg(ap, char *);

			while (*s != '\0')
				pc(r, *s++);
			break;
		}
		case 'p':
			pc(r, '0');
			pc(r, 'x'); /* fall through... */
			islong = sizeof(long) > 4U;
		case 'x': {
			bool sig = false;
			unsigned long v = islong ? va_arg(ap, unsigned long)
				: va_arg(ap, unsigned int);
		    size_t i = 2U * sizeof(v);
			while (i > 0U) {
				--i;
				uint8_t d = (uint8_t)((v >> (i * 4U)) & 0x0fU);

				if (d != 0U) {
					sig = true;
				}
				if (sig || i == 0U)
					pc(r, "0123456789abcdef"[d]);
			}
			break;
		}
		case 'd':
			prdec(r, va_arg(ap, int));
			break;
		default:
			pc(r, '%');
			pc(r, *f);
		}
	}
	endrec(r);
	return r->idx;
}

#define CALL_VPF(rec)				\
	va_list ap;				\
	va_start(ap, f);			\
	ret = (int)vpf(&r, f, ap);			\
	va_end(ap);

static inline int snprintf(char *buf, size_t len, const char *f, ...)
{
	int ret = 0;
	struct _pfr r = { .buf = buf, .len = len };

	CALL_VPF(&r);
	return ret;
}

static inline int sprintf(char *buf, const char *f, ...)
{
	int ret = 0;
	struct _pfr r = { .buf = buf, .len = 0x7fffffffU };

	CALL_VPF(&r);
	return ret;
}

static inline int printf(const char *f, ...)
{
	int ret = 0;
	struct _pfr r = {0};

	CALL_VPF(&r);
	return ret;
}
