/// @file t_spwd.c
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief

#include <shadow.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <crypt/sha256.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("You can either:\n");
        printf("    -g, --generate <key> : prints the hashed key.\n");
        printf("    -l, --load <user>    : prints the hashed key stored for the given user.\n");
        return 1;
    }
    if (!strcmp(argv[1], "--generate") || !strcmp(argv[1], "-g")) {
        unsigned char buffer[SHA256_BLOCK_SIZE] = { 0 };
        char output[SHA256_BLOCK_SIZE * 2 + 1]  = { 0 };
        SHA256_ctx_t ctx;

        sha256_init(&ctx);
        for (unsigned i = 0; i < 100000; ++i)
            sha256_update(&ctx, (unsigned char *)argv[2], strlen(argv[2]));
        sha256_final(&ctx, buffer);

        sha256_bytes_to_hex(buffer, SHA256_BLOCK_SIZE, output, SHA256_BLOCK_SIZE * 2 + 1);
        printf("%s\n", output);

    } else if (!strcmp(argv[1], "--load") || !strcmp(argv[1], "-l")) {
        struct spwd *spbuf = getspnam(argv[2]);
        tm_t *tm;
        if (spbuf) {
            time_t lstchg = (time_t)spbuf->sp_lstchg;
            tm            = localtime(&lstchg);
            printf("name         : %s\n", spbuf->sp_namp);
            printf("password     :\n%s\n", spbuf->sp_pwdp);
            printf("lastchange   : %02d/%02d %02d:%02d\n", tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min);
            printf("days allowed : %d\n", spbuf->sp_min);
            printf("days req.    : %d\n", spbuf->sp_max);
            printf("days warning : %d\n", spbuf->sp_warn);
            printf("days inact   : %d\n", spbuf->sp_inact);
            printf("days expire  : %d\n", spbuf->sp_expire);
        }
    }
    return 0;
}
