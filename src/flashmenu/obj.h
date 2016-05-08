#ifndef OBJ_H
#define OBJ_H

struct _configuration {
    char type;
};

struct rect {
    short x;
    short y;
    short w;
    short h;
};

struct treenode {
    char            type;
    struct obj *    prev;
    struct obj *    next;
    struct obj *    parent;
    struct obj *    children;
};

struct obj {
    struct treenode node;
    struct rect     rect;

    int (*draw) (void *);
};

struct scrollable {
    struct obj  obj;
    char        bank;
};

#endif /* #ifndef OBJ_H */
