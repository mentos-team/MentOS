#include <stdio.h>
#include <fcntl.h>

void main(int argc, char ** argv)
{
    if (argc == 2)
    {
        // Try to open the file.
        int fd = open(argv[1], O_RDONLY, 42);
        if (fd > -1)
        {
            char c;
            // Put on the standard output the characters.
            while (read(fd, &c, 1)) putchar(c);
            putchar('\n');
            putchar('\n');
            close(fd);
        }
        else
        {
            printf("%s: cannot find the file.\n\n", argv[1]);
        }
    }
    else
    {
        printf("Usage: more file\n\n");
    }
}