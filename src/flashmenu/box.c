#include <stdlib.h>

#include "libgfx.h"
#include "obj.h"
#include "box.h"
#include "layout-ops.h"
#include "message.h"

void __fastcall__ draw_box (struct obj *);

struct obj_ops box_ops = {
    draw_box,
    layout_none
};

struct box * __fastcall__
make_box (char * pattern)
{
    struct box * box = alloc_obj (sizeof (struct box), &box_ops);
    box->pattern = pattern;
    return box;
}

void __fastcall__
draw_box (struct obj * o)
{
    struct box * b = (struct box *) o;
    struct rect * r = &o->rect;

    gfx_set_pencil_mode (1);
    gfx_set_pattern (b->pattern);
    gfx_draw_box (0, 0, r->w, r->h);
    if (!o->node.children)
        print_message ("no kids");
    draw_obj_children (o);
}