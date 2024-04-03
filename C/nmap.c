#include <sys/stat.h>
#include <sys/mman.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

int main()
{
    int fd = open("file_name", O_RDONLY);
    if (fd == -1) { /* handle error */ }

    struct stat sb;
    if (fstat(fd, &sb) == -1) { /* handle error */ }

    char* map = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) { /* handle error */ }
    
    /* Since you mentioned you are doing a lot of random access,
     * use this hint to tell the kernel how you plan to use it.
     */
    madvise(map, sb.st_size, MADV_RANDOM);

    /* Once, you have the map, you actually don't need the file
     * descriptor open anymore...
     */
    close(fd);

    //You want byte 10000 ?
    char byte_10000 = map[10000];
    
    return 0/
}