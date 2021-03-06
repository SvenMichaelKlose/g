/*
 * UltiFS
 *
 * Journaling file system for Flash memory.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define FALSE   0
#define TRUE    1

#ifndef IMAGE_SIZE
#define IMAGE_SIZE      (8 * 1024 * 1024)
#endif

/* NOTE: No secondary bookkeeping areas for GC till it's about to get implemented. */
#define ULTIFS_ID       "ULTIFS"
#define JOURNAL_START   (64 * 1024)
#define JOURNAL_SIZE    (64 * 1024)
#define BLOCKS_START    (JOURNAL_START + JOURNAL_SIZE)
#define BLOCKS_SIZE     (64 * 1024)
#define NUM_BLOCKS      (BLOCKS_SIZE / sizeof (struct block))
#define FILES_START     (BLOCKS_START + BLOCKS_SIZE)
#define FILES_SIZE      (64 * 1024)
#define DATA_START      (FILES_START + FILES_SIZE)

char image[IMAGE_SIZE];

typedef int16_t index_t;
typedef int32_t offset_t;

offset_t journal;
offset_t journal_free;
offset_t blocks;
offset_t files;
offset_t data_free;

#ifdef __GNUC__
#pragma pack(push, 1)   /* Disable struct padding. */
#endif

/*
 * File system/journal header
 */
struct ultifs_header {
    char     is_active;
    char     id[sizeof (ULTIFS_ID)];
    int16_t  version;
    offset_t blocks;
    offset_t files;
};

struct block {
    index_t  update;
    offset_t pos;
    int16_t  size;
    index_t  next;
};

struct file {
    index_t update;
    index_t first_block;
};

#define MAX_FILENAME_LENGTH     16

#define ULTIFS_TYPE_FILE        0
#define ULTIFS_TYPE_DIRECTORY   1

struct ultifs_dirent {
    index_t update;
    char    name[MAX_FILENAME_LENGTH];
    char    type;
    index_t file;
    int32_t size;
};

#ifdef __GNUC__
#pragma pack(pop)
#endif

void
clear_image ()
{
    memset (image, 0xff, IMAGE_SIZE);
}

void
img_write_mem (offset_t pos, void * str, size_t size)
{
    memcpy (&image[pos], str, size);
}

#define IMG_WRITE_STRUCT(pos, x)    img_write_mem (pos, &x, sizeof (x))

void
img_write_string (int pos, char * str)
{
    strcpy (&image[pos], str);
}

void
img_write_char (int pos, char x)
{
    *((char *) &image[pos]) = x;
}

void
img_read_mem (void * dest, offset_t pos, size_t size)
{
    memcpy (dest, &image[pos], size);
}

#define IMG_READ_STRUCT(x, pos)    img_read_mem (&x, pos, sizeof (x))

char
img_read_char (offset_t pos)
{
    return image[pos];
}

int16_t
img_read_word (offset_t pos)
{
    return *(int16_t *) &image[pos];
}

void
mk_bootloader ()
{
    FILE * in;

    in = fopen ("src/flashboot/flashboot.bin", "r");
    fread (image, 65536, 1, in);
    fclose (in);
}

void
mk_journal ()
{
    struct ultifs_header header;

    header.is_active = 0;
    strcpy (header.id, ULTIFS_ID);
    header.version = 1;
    header.blocks = BLOCKS_START;
    header.files = FILES_START;
    IMG_WRITE_STRUCT(JOURNAL_START, header);
}

#define INITIAL_ROOT_DIRECTORY_SIZE     1024

void
mk_root_directory_block ()
{
    struct block block;

    block.update = -1;
    block.pos = DATA_START;
    block.size = INITIAL_ROOT_DIRECTORY_SIZE;
    block.next = -1;
    IMG_WRITE_STRUCT(BLOCKS_START, block);
}

void
mk_root_directory_file ()
{
    struct file file;

    file.update = -1;
    file.first_block = 0;
    IMG_WRITE_STRUCT(FILES_START, file);
}

void
mkfs ()
{
    clear_image ();
    mk_bootloader ();
    mk_journal ();
    mk_root_directory_block ();
    mk_root_directory_file ();
}

void
emit_image ()
{
    FILE * out;

    out = fopen ("image", "w");
    fwrite (image, IMAGE_SIZE, 1, out);
    fclose (out);
}

/*
 * Find a free record in journal, block chains or file index.
 */
offset_t
find_free (offset_t start, size_t record_size, int num_records)
{
    offset_t p;
    char     i;
    int      j;
    char     is_free;

    p = start;
    for (j = 0; j < num_records; j++) {
        is_free = TRUE;
        for (i = 0; i < record_size; i++) {
            if (img_read_char (p + i) == -1)
                continue;
            is_free = FALSE;
            break;
        }
        if (is_free)
            break;
        p += record_size;
    }

    return p;
}

/*
 * Find update of a record.
 */
offset_t
get_update (offset_t pos)
{
    index_t update;

    while (1) {
        update = img_read_word (pos);
        if (update == -1)
            break;
        pos = ((offset_t) update << 3) + JOURNAL_START;
    }

    return pos;
}

void
get_block (struct block * b, index_t idx)
{
    offset_t pos = idx * sizeof (struct block) + BLOCKS_START;
    img_read_mem (b, get_update (pos), sizeof (struct block));
}

void
put_block (struct block * b, index_t idx)
{
    offset_t pos = idx * sizeof (struct block) + BLOCKS_START;
    img_write_mem (get_update (pos), b, sizeof (struct block));
}

void
get_file (struct file * b, index_t idx)
{
    offset_t pos = idx * sizeof (struct file) + FILES_START;
    img_read_mem (b, get_update (pos), sizeof (struct file));
}

/*
 * Find the first officially free byte in the data area.
 */
offset_t
find_highest_ending_block_address ()
{
    struct block b;
    int          i;
    offset_t     highest = 0;
    offset_t     tmp;

    for (i = 0; i < NUM_BLOCKS; i++) {
        get_block (&b, i);
        if (b.pos == -1)
            continue;
        tmp = b.pos + b.size;
        if (highest < tmp)
            highest = tmp;
    }

    return highest;
}

/*
 * Find the first free byte in the data area.
 *
 * Skips garbage resulting from incomplete writes.
 */
void
find_data_free ()
{
    offset_t highest = find_highest_ending_block_address ();
    int      i;

    for (i = IMAGE_SIZE - 1; i > highest; i--) {
        if (img_read_char (i) == -1)
            continue;
        data_free = i + 1;
        return;
    }

    data_free = highest;
}

void
mount_journal (struct ultifs_header * h)
{
    journal = JOURNAL_START;
    journal_free = find_free (JOURNAL_START + sizeof (struct ultifs_header), 8, (JOURNAL_SIZE - sizeof (struct ultifs_header)) / 8);
}

void
mount_blocks (struct ultifs_header * h)
{
    blocks = h->blocks;
}

void
mount_files (struct ultifs_header * h)
{
    files = h->files;
}

void
mount ()
{
    struct ultifs_header h;

    IMG_READ_STRUCT(h, JOURNAL_START);
    mount_journal (&h);
    mount_blocks (&h);
    mount_files (&h);
    find_data_free ();
}

index_t
alloc_journal (void * src, size_t size)
{
    index_t j = journal_free >> 3;

    if (journal_free & 7)
        j++;
    img_write_mem (journal_free, src, size);
    journal_free += size;

    return j;
}

index_t
alloc_block (int16_t size)
{
    offset_t     i = find_free (BLOCKS_START, sizeof (struct block), BLOCKS_SIZE / sizeof (struct block));
    struct block b;

    b.update = -1;
    b.pos = data_free;
    b.size = size;
    b.next = -1;
    data_free += size ? size : 0x10000;
    IMG_WRITE_STRUCT(i, b);

    return (i - BLOCKS_START) >> 3;
}

index_t
alloc_file (index_t first_block)
{
    offset_t    i = find_free (FILES_START, sizeof (struct file), FILES_SIZE / sizeof (struct file));
    struct file f;

    f.update = -1;
    f.first_block = first_block;
    IMG_WRITE_STRUCT(i, f);

    return (i - FILES_START) / sizeof (struct file);
}

void
link_block (index_t idx, index_t next)
{
    struct block b;

    get_block (&b, idx);
    b.next = next;
    put_block (&b, idx);
}

index_t
alloc_block_chain (size_t size)
{
    index_t first_block = -1;
    index_t last_block = -1;
    index_t b;
    size_t s;

    while (size) {
        s = size > 65536 ? 65536 : size;
        b = alloc_block (s);
        if (last_block != -1)
            link_block (last_block, b);
        else
            first_block = b;
        size -= s;
        last_block = b;
    }

    return first_block;
}

index_t
create_file (size_t size)
{
    index_t blocks = alloc_block_chain (size);
    return alloc_file (blocks);
}

typedef void (*fileop) (offset_t pos, void * data, size_t size);

/*
 * Transfer data from or to file.
 *
 * The data transferred must not cross the end of the block chain.
 */
void
data_xfer (fileop op, index_t fi, offset_t ofs, void * data, size_t size)
{
    struct file  f;
    struct block b;
    index_t      bi;
    offset_t     bend;
    size_t       s;
    size_t       bs;

    get_file (&f, fi);
    bi = f.first_block;
    while (2) {
        get_block (&b, bi);
        bs = b.size ? b.size : 0x10000;
        if (ofs < bs) {
            s = bs - ofs;
            if (s > size)
                s = size;
            op (b.pos + ofs, data, s);
            if (!(size -= s))
                return;
            data += s;
            ofs = 0;
        }
        bi = b.next;
        if (bi == -1) {
            printf ("Internal error: file %d: access beyond end of block chain.\n", fi);
            exit (1);
        }
    }
}

void
op_write (offset_t pos, void * data, size_t size)
{
    img_write_mem (pos, data, size);
}

void
op_read (offset_t pos, void * data, size_t size)
{
    img_read_mem (data, pos, size);
}

void
write_file (index_t fi, offset_t ofs, void * data, size_t size)
{
    data_xfer (op_write, fi, ofs, data, size);
}

void
read_file (index_t fi, offset_t ofs, void * data, size_t size)
{
    data_xfer (op_read, fi, ofs, data, size);
}

/*
 * Get size of file's block chain.
 *
 * The real size of a file is stored in its directory to
 * allow for preallocation of data areas which in turn
 * reduces the need for journaling.
 */
size_t
get_raw_filesize (index_t fi)
{
    struct file  f;
    struct block b;
    size_t      acc_size = 0;
    index_t     bi;

    get_file (&f, fi);
    bi = f.first_block;
    do {
        get_block (&b, bi);
        acc_size += b.size;
        bi = b.next;
    } while (bi != -1);

    return acc_size;
}

char
area_is_free (void * data, size_t size)
{
    int    i;
    char * p = (char *) data;

    for (i = 0; i < size; i++)
        if (p[i] != -1)
            return FALSE;

    return TRUE;
}

offset_t
find_free_dirent (index_t d)
{
    size_t   dsize = get_raw_filesize (d);
    offset_t p;
    char     buf[sizeof (struct ultifs_dirent)];

    for (p = 0; (p + sizeof (struct ultifs_dirent)) < dsize; p += sizeof (struct ultifs_dirent)) {
        read_file (d, p, buf, sizeof (struct ultifs_dirent));
        if (area_is_free (buf, sizeof (struct ultifs_dirent)))
            return p;
    }

    printf ("Error: directory is full.\n");
    exit (1);
}

void
create_dirent (index_t d, index_t f, char * name, char type, size_t size)
{
    offset_t             free_entry = find_free_dirent (d);
    struct ultifs_dirent e;

    e.update = -1;
    strncpy (e.name, name, MAX_FILENAME_LENGTH);
    e.type = type;
    e.file = f;
    e.size = size;
    write_file (d, free_entry, &e, sizeof (struct ultifs_dirent));
}

#define ROOT_DIRECTORY_INDEX    0

index_t
add_file (index_t di, char * name, char type, size_t size)
{
    index_t fi = create_file (size);

    create_dirent (di, fi, name, type, size);

    return fi;
}

void *
load_file (size_t * rs, char * pathname)
{
    FILE * f = fopen (pathname, "r");
    size_t s;
    char * loader;

    fseek (f, 0, SEEK_END);
    *rs = s = ftell (f);
    rewind (f);
    loader = malloc (s);
    fread (loader, s, 1, f);
    fclose (f);

    return loader;
}

void
add_secondary_bootloader ()
{
    size_t s;
    char * loader = load_file (&s, "src/flashmenu/flashmenu.bin");
    index_t fi = add_file (ROOT_DIRECTORY_INDEX, "boot", ULTIFS_TYPE_FILE, s);
    write_file (fi, 0, loader, s);
    free (loader);
}

void
make_asm_info ()
{
    FILE * f = fopen ("src/lib/ultifs/internals.asm", "w");
    fprintf (f, "journal_start = %d\n", JOURNAL_START);
    fprintf (f, "blocks_start = %d\n", BLOCKS_START);
    fprintf (f, "files_start = %d\n", FILES_START);
    fprintf (f, "\n");
    fprintf (f, "block_record_size = %lu\n", sizeof (struct block));
    fprintf (f, "block_pos = %lu\n", offsetof(struct block, pos));
    fprintf (f, "block_size = %lu\n", offsetof(struct block, size));
    fprintf (f, "block_next = %lu\n", offsetof(struct block, next));
    fprintf (f, "\n");
    fprintf (f, "dirent_name = %lu\n", offsetof(struct ultifs_dirent, name));
    fprintf (f, "dirent_type = %lu\n", offsetof(struct ultifs_dirent, type));
    fprintf (f, "dirent_file = %lu\n", offsetof(struct ultifs_dirent, file));
    fprintf (f, "dirent_size = %lu\n", offsetof(struct ultifs_dirent, size));
    fclose (f);
}

int
main (int argc, char ** argv)
{
    make_asm_info ();
    mkfs ();
    mount ();
    add_secondary_bootloader ();
    emit_image ();

    return 0;
}
