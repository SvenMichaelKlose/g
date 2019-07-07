#pragma code-name ("FILEWINDOW")

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <cbm.h>

#include <ingle.h>
#include <ultimem.h>

#include "cc65-charmap.h"
#include "ultifs.h"
#include "wrap-ultifs.h"
#include "obj.h"
#include "button.h"
#include "layout-ops.h"
#include "list.h"
#include "table.h"
#include "window.h"
#include "message.h"
#include "file-window.h"
#include "main.h"

#define KEY_UP      145
#define KEY_DOWN    17
#define KEY_LEFT    157
#define KEY_RETURN  13

struct cbm_dirent dirent;

unsigned __fastcall__
gen_launch (struct drive_ops * drive_ops, unsigned start)
{
    unsigned oldblk5 = *ULTIMEM_BLK5;
    unsigned read_bytes;
    unsigned size = 0;

    print_message ("Loading...");
    *ULTIMEM_BLK5 = 12;

    while (1) {
        read_bytes = drive_ops->read ((void *) 0xa000, 0x2000);
        if (!read_bytes)
            break;
        if (read_bytes == -1) {
            print_message ("Read error!");
            return -1;
        }
        size += read_bytes;
        *ULTIMEM_BLK5RAM = *ULTIMEM_BLK5RAM + 1;

        sprintf (message_buffer, "%U read...", (unsigned) size);
        print_message (message_buffer);
    }
    *ULTIMEM_BLK5 = oldblk5;

    return size;
}

char
gcbm_opendir ()
{
    return cbm_opendir (2, 8, "$");
}

char __fastcall__
gcbm_readdir (struct cbm_dirent * dirent)
{
    return cbm_readdir (2, dirent);
}

void
gcbm_closedir ()
{
    cbm_closedir (2);
}

char __fastcall__
gcbm_enterdir (char * name)
{
    sprintf (message_buffer, "CD/%S/", name);
    cbm_open (15, 8, 15, message_buffer);
    cbm_read (15, message_buffer, 63);
    cbm_close (15);

    return 0;
}

void
gcbm_leavedir ()
{
}

char __fastcall__
gcbm_open (char * name, char mode)
{
    return cbm_open (2, 8, mode, name);
}

int __fastcall__
gcbm_read (void * data, unsigned size)
{
    if (cbm_k_readst ())    /* cbm_read() returns 1 on end-of-file. */
        return 0;
    return cbm_read (2, data, size);
}

void
gcbm_close ()
{
    cbm_close (2);
}

struct drive_ops cbm_drive_ops = {
    gcbm_opendir,
    gcbm_readdir,
    gcbm_closedir,
    gcbm_enterdir,
    gcbm_leavedir,
    gcbm_open,
    gcbm_read,
    gcbm_close,
    gen_launch
};

bfile * current_file;

char __fastcall__
u_open (char * name, char mode)
{
    current_file = w_ultifs_open (ultifs_pwd, name, mode);
    if (!current_file)
        return -1;
    return 0;
}

int __fastcall__
u_read (void * data, unsigned size)
{
    return w_bfile_readm (current_file, data, size);
}

void
u_close ()
{
    w_bfile_close (current_file);
}


void __fastcall__
gen_exec (unsigned long ptr, unsigned start, unsigned size)
{
    memcpy ((void *) 0x120, (void *) 0x9ff0, 16);
    *(unsigned int *) 0x128 = DESKTOP_BANK;
    save_state ((unsigned) restart, 0);
    ingle_exec (ptr, start, size);
}

unsigned __fastcall__
ultifs_launch (struct drive_ops * drive_ops, unsigned start)
{
    gen_exec (current_file->ptr, start, current_file->size);

    return current_file-> size;
}

struct drive_ops ultifs_drive_ops = {
    w_ultifs_opendir,
    w_ultifs_readdir,
    w_ultifs_closedir,
    w_ultifs_enterdir,
    w_ultifs_leavedir,
    u_open,
    u_read,
    u_close,
    ultifs_launch
};

void __fastcall__
file_window_free_files (struct file_window_content * content)
{
    struct dirent * d = content->files;
    struct dirent * n;

    while (d) {
        n = d->next;
        free (d);
        d = n;
    }
}

void __fastcall__
file_window_read_directory (struct file_window_content * content)
{
    struct dirent * first_dirent = NULL;
    struct dirent * last_dirent = NULL;
    struct dirent * d;
    unsigned len = 0;

    file_window_free_files (content);

    content->drive_ops->opendir ();
    while (1) {
        if (content->drive_ops->readdir (&dirent))
            break;
        if (dirent.type == CBM_T_HEADER || dirent.name[0] == '.' || (dirent.type == CBM_T_DIR && (!strcmp (".", dirent.name) || !strcmp ("..", dirent.name))))
            continue;
        len++;
        d = malloc (sizeof (struct dirent));
        if (last_dirent)
            last_dirent->next = d;
        else
            first_dirent = d;
        last_dirent = d;

        d->size = dirent.size;
        d->type = dirent.type;
        memcpy (&d->name, &dirent.name, 17);
    }
    content->drive_ops->closedir ();

    content->files = first_dirent;
    content->len = len;
}

void __fastcall__
file_window_invert_position (struct file_window_content * content)
{
    unsigned visible_lines = content->obj.rect.h / 8 - 1;
    unsigned wpos = content->pos - (content->pos % visible_lines);
    unsigned y = (content->pos - wpos) * 8;

    if (y > content->obj.rect.h || !content->len)
        return;

    gfx_push_context ();
    gfx_reset_region ();
    set_obj_region ((struct obj *) content);
    gfx_set_pattern (pattern_solid);
    gfx_set_pencil_mode (PENCIL_MODE_XOR);
    gfx_draw_box (0, y, content->obj.rect.w, 8);
    gfx_pop_context ();
}

struct dirent * __fastcall__
file_window_get_file_by_index (struct file_window_content * content, unsigned x)
{
    struct dirent * d = content->files;

    while (d && x--)
        d = d->next;

    return d;
}

void __fastcall__
file_window_draw_list (struct obj * w)
{
    struct window * win = WINDOW(w);
    struct file_window_content * content = (struct file_window_content *) w;
    unsigned visible_lines = content->obj.rect.h / 8 - 1;
    char xofs = 1;
    struct dirent * d = content->files;
    unsigned y = 0;
    unsigned wpos = content->pos - (content->pos % visible_lines);
    unsigned rpos = wpos;
    uchar i;
    uchar t;
    char c;
    char size[8];

    d = file_window_get_file_by_index (content, wpos);

    gfx_push_context ();
    gfx_reset_region ();
    set_obj_region (w);

    /* Clear window. */
    gfx_set_pattern (pattern_empty);
    gfx_draw_box (0, 0, w->rect.w, w->rect.h);

    while (1) {
        if (y > w->rect.h)
            break;

        if (rpos >= content->len)
            goto next;
        if (!d || !d->name[0])
            goto next;

        /* Print file type. */
        gfx_set_font (charset_4x8, 0, FONT_BANK);
        gfx_set_font_compression (1);
        gfx_set_position (xofs, y);
        switch (d->type) {
            case CBM_T_SEQ:
                t = 's'; break;
            case CBM_T_PRG:
                t = 'p'; break;
            case CBM_T_USR:
                t = 'u'; break;
            case CBM_T_REL:
                t = 'r'; break;
            case CBM_T_VRP:
                t = 'v'; break;
            case CBM_T_DEL:
                t = 'x'; break;
            case CBM_T_CBM:
                t = 'c'; break;
            case CBM_T_DIR:
                t = 'd'; break;
            case CBM_T_LNK:
                t = 'l'; break;
            case CBM_T_HEADER:
                t = 'h'; break;
            default:
                t = 'o';
        }
        gfx_putchar (t);
        gfx_putchar (32);

        /* Print file size. */
        if (d->type != CBM_T_DIR) {
            gfx_set_position (xofs + 6, y);
            memset (size, 32, sizeof (size));
            sprintf (size, "%U", (unsigned int) d->size);
            size[strlen (size)] = 32;
            for (i = 0; i < 5; i++)
                gfx_putchar (size[i]);
        }

        /* Print file name. */
//        gfx_set_font ((void *) 0x8000, 0);
//        gfx_set_font_compression (0);
        gfx_set_position (xofs + 28, y);
        for (i = 0; i < 16; i++)
            if (c = d->name[i]) {
                gfx_putchar (c); //(c - 64) & 127);
            } else
                break;

        d = d->next;
next:
        y += 8;
        rpos++;
    }

    if (!content->len) {
        gfx_set_font (charset_4x8, 2, FONT_BANK);
        gfx_draw_text (1, 1, "No files.");
    }

    gfx_pop_context ();
}


void __fastcall__
file_window_draw_content (struct obj * w)
{
    struct file_window_content * content = (struct file_window_content *) w;

    if (!content->files)
        file_window_read_directory (content);

    file_window_draw_list (w);
    file_window_invert_position (content);
}

typedef void __fastcall__ (*launch_t) (unsigned start, unsigned size);

void __fastcall__
file_window_launch_program (struct file_window_content * content, struct dirent * d)
{
    struct drive_ops * drive_ops = content->drive_ops;
    unsigned start;
    unsigned size;

    if (drive_ops->open (d->name, 0)) {
        print_message ("Can't open file.");
        return;
    }
    drive_ops->read (&start, 2);
    size = drive_ops->launch (drive_ops, start);
    drive_ops->close ();

    gen_exec (12 * 0x2000u, start, size);
}

void __fastcall__
file_window_enter_directory (struct file_window_content * content, struct dirent * d)
{
    content->drive_ops->enterdir (d->name);
    content->pos = 0;
    file_window_read_directory (content);
    file_window_draw_content ((struct obj *) content);
    file_window_invert_position (content);
}

void __fastcall__
file_window_launch (struct file_window_content * content, struct dirent * d)
{
    switch (d->type) {
        case CBM_T_DIR:
            file_window_enter_directory (content, d);
            break;

        default:
            file_window_launch_program (content, d);
            break;
    }
}

char __fastcall__
file_window_event_handler (struct obj * o, struct event * e)
{
    struct file_window_content * content = (struct file_window_content *) o->node.children;
    int visible_lines = content->obj.rect.h / 8 - 1;
    unsigned wpos = content->pos - (content->pos % visible_lines);

    file_window_invert_position (content);

    switch (e->data_char) {
        case KEY_RETURN:
            file_window_invert_position (content);
            file_window_launch (content, file_window_get_file_by_index (content, content->pos));
            break;

        case KEY_UP:
            if (!content->pos)
                goto done;
            content->pos--;
            if (content->pos < wpos)
                goto new_page;
            break;

        case KEY_DOWN:
            if (content->pos > content->len) {
                content->pos = content->len - 1;
                goto done;
            }
            if (content->pos == content->len - 1)
                goto done;
            content->pos++;
            if (content->pos > wpos + visible_lines - 1)
                goto new_page;
            break;

        case KEY_LEFT:
            content->drive_ops->leavedir ();
            goto new_directory;
    }

done:
    file_window_invert_position (content);
    return FALSE;

new_directory:
    content->pos = 0;
    file_window_read_directory (content);

new_page:
    file_window_draw_content ((struct obj *) content);
    return FALSE;
}

struct obj_ops obj_ops_file_window_content = {
    file_window_draw_content,
    obj_noop,
    obj_noop,
    event_handler_passthrough,
    FILE_WINDOW_BANK,
    FILE_WINDOW_BANK,
    FILE_WINDOW_BANK,
    FILE_WINDOW_BANK
};

struct obj * __fastcall__
make_file_window_content (struct drive_ops * drive_ops)
{
	struct obj * obj =  alloc_obj (sizeof (struct obj), &obj_ops_file_window_content);
    struct file_window_content * content = calloc (1, sizeof (struct file_window_content));

    memcpy (content, obj, sizeof (struct obj));
    free (obj);
    content->files = NULL;
    content->drive_ops = drive_ops;

    return OBJ(content);
}

struct obj * __fastcall__
make_file_window (struct drive_ops * drive_ops, char * title, gpos x, gpos y, gpos w, gpos h)
{
    struct file_window_content * content = (struct file_window_content *) make_file_window_content (drive_ops);
	struct window * win = make_window (title, (struct obj *) content, file_window_event_handler);

	window_set_position_and_size (win, x, y, w, h);

    return OBJ(win);
}
