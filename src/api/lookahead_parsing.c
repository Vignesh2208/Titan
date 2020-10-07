#ifndef DISABLE_LOOKAHEAD
#include <stdarg.h>
#include "lookahead_parsing.h"

static int check (int test, const char * message, ...) {
    if (test) {
        va_list args;
        va_start (args, message);
        vfprintf (stderr, message, args);
        va_end (args);
        fprintf (stderr, "\n");
        return FAILURE;
    }
    return SUCCESS;
}

int LoadLookahead(const char * file_path, struct lookahead_info * linfo) {
    int fd;
    struct stat s;
    int status;
    size_t size;
    int i;

    if (!linfo)
        return FAILURE;

    
    linfo->mapped_memblock = NULL;
    linfo->mapped_memblock_length = 0;
    linfo->lmap.start_offset=0;
    linfo->lmap.finish_offset=0;
    linfo->lmap.number_of_values=0;
    linfo->lmap.lookahead_values=NULL;

    if (!file_path)
        return FAILURE;

    //printf ("Opening file: %s\n", file_path);
    /* Open the file for reading. */
    fd = open (file_path, O_RDONLY);
    if (!check (fd < 0, "open lookahead file %s failed: %s", file_path, strerror (errno)))
        return FAILURE;

    /* Get the size of the file. */
    status = fstat (fd, & s);
    if (!check (status < 0, "stat %s failed: %s", file_path, strerror (errno)))
        return FAILURE;
    size = s.st_size;

    //printf ("File size: %lu\n", size);

    /* Memory-map the file. */
    linfo->mapped_memblock = mmap (0, size, PROT_READ, MAP_SHARED, fd, 0);
    if (!check (linfo->mapped_memblock == MAP_FAILED, "mmap %s failed: %s",
           file_path, strerror (errno))) {
               linfo->mapped_memblock = NULL;
               return FAILURE;
    }
    linfo->mapped_memblock_length = size;
    close(fd);

    //printf ("mapped memblock size: %ld\n", linfo->mapped_memblock_length);

    memcpy(&linfo->lmap,(struct lookahead_map *)linfo->mapped_memblock,
            sizeof(struct lookahead_map));

    //printf ("Finish offset: %ld, Number of values: %ld\n", linfo->lmap.finish_offset, linfo->lmap.number_of_values);
    linfo->lmap.lookahead_values = (long *)(linfo->mapped_memblock + sizeof(struct lookahead_map));

    //printf ("Returning success !\n");
    return SUCCESS;

}

void CleanLookaheadInfo(struct lookahead_info * linfo) {
    if (!linfo) return;

    if (linfo->mapped_memblock_length && linfo->mapped_memblock)
        munmap((void *)linfo->mapped_memblock, linfo->mapped_memblock_length);
    
    linfo->mapped_memblock = NULL;
    linfo->mapped_memblock_length = 0;
    linfo->lmap.start_offset=0;
    linfo->lmap.finish_offset=0;
    linfo->lmap.number_of_values=0;
    linfo->lmap.lookahead_values=NULL;
    
}
#endif
