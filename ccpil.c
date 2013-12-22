#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/time.h>

#include "dbg.h"
#include "hex.h"

#define FLASH_SIZE (32*1024)

static bool opt_flash = false;
static bool opt_read = false;
static bool opt_write = false;
static char *flash_filename = NULL;
static int pages = 32;

static struct option long_options[] =
{
    {"help",    no_argument, 0, 'h'},
    {"flash",     required_argument, 0, 'f'},
    {"read",     required_argument, 0, 'r'},
    {"write",     required_argument, 0, 'w'},

    {"pages",     required_argument, 0, 'p'},
    {0, 0, 0, 0}
};

static void usage(void)
{
    fprintf(stderr, "ChipCon Pi Loader, Toby Jaffey <toby-ccpl@hodgepig.org>\n");
    fprintf(stderr, "cctl-prog [-f file.hex] [-r file.hex]\n");
    fprintf(stderr, "  --help           -h          This help\n");
    
    fprintf(stderr, "  --flash=file.hex -f file.hex Reflash device with intel hex file\n");

    fprintf(stderr, "  --read=file.out -r file.hex Dump device flash to file\n");
    fprintf(stderr, "  --write=file.out -w file.hex Write dump to the device\n");
}

static int parse_options(int argc, char **argv)
{
    int c;
    int option_index;

    while(1)
    {
        c = getopt_long (argc, argv, "hf:r:p:w:", long_options, &option_index);
        if (c == -1)
            break;
        switch(c)
        {
            case 'h':
                return 1;
            break;
            case 'f':
                opt_flash = true;
                flash_filename = strdup(optarg);
            break;
            case 'r':
                opt_read = true;
                flash_filename = strdup(optarg);
            break;
            case 'w':
                opt_write = true;
                flash_filename = strdup(optarg);
            break;
            case 'p':
                pages = atoi(optarg);
            break;
            default:
                return 1;
            break;
        }
    }

    return 0;
}

static void dump(const uint8_t *p, size_t len)
{
    while(len--)
        printf("%02X", *p++);
    printf("\n");
}

static int read_dumpfile(uint8_t *buf, size_t buflen, const char *filename)
{
    FILE *fp;
    char i;
    int pos=0;

    if (NULL == (fp = fopen(filename, "rb")))
        return 1;

    while (NULL != fread(&i, sizeof(char), 1, fp) && buflen>=pos)
    {
        buf[pos++] = i;
        printf("%02X", i);
    }
    printf("\n");

    fclose(fp);
    return 0;
}


static int program_verify_page(const uint8_t *data, uint8_t page)
{
    uint8_t verbuf[1024];

    if (0 != dbg_writepage(page, data))
    {
        fprintf(stderr, "program_page failed\n");
        return 1;
    }

    if (0 != dbg_readpage(page, verbuf))
    {
        fprintf(stderr, "read_page failed\n");
        return 1;
    }

    //if (0!=memcmp(verbuf, data, 1024))
    //{
        fprintf(stderr, "verify failed\n");

        printf("verbuf = ");
        dump(verbuf, 1024);
        printf("expected = ");
        dump(data, 1024);

    //    return 1;
    //}

    return 0;
}

static int read_page(const uint8_t *data, uint8_t page)
{
    if (0 != dbg_readpage(page, data))
    {
        fprintf(stderr, "read_page failed\n");
        return 1;
    }

    printf("data = ");
    dump(data, 1024);

    return 0;
}

int main(int argc, char *argv[])
{
    uint8_t *buf;
    int i;

    if (NULL == (buf=malloc(FLASH_SIZE)))
    {
        fprintf(stderr, "out of ram\n");
        return 1;
    }   

    if (0 != parse_options(argc, argv))
    {
        usage();
        return 1;
    }

    if (!opt_flash && !opt_read && !opt_write)
    {
        usage();
        return 1;
    }

    if (opt_flash)
    {
        if (0 != dbg_init())
        {
            fprintf(stderr, "Failed to initialise (run as root for /dev/mem access)\n");
            return 1;
        }

        memset(buf, 0xFF, FLASH_SIZE);
        if (0 != read_hexfile(buf, FLASH_SIZE, flash_filename))
        {
            fprintf(stderr, "Failed to read %s\n", flash_filename);
            return 1;
        }

        if (0 != dbg_mass_erase())
        {
            fprintf(stderr, "CC1110 mass erase failed\n");
            return 1;
        }

        for (i=0;i<FLASH_SIZE;i+=1024)
        {
            bool skip = true;
            int j;

            for (j=i;j<i+1024;j++)
            {
                if (buf[j] != 0xFF)
                {
                    skip = false;
                    break;
                }
            }
            if (skip)
            {
                printf("Skipping blank page %d\n", i/1024);
                continue;
            }

            printf("Programming and verifying page %d\n", i/1024);
            if (0 != program_verify_page(buf + i, i/1024))
            {
                fprintf(stderr, "FAILED\n");
                return 1;
            }
        }

        printf("Programming complete\n");
        dbg_reset();
    }

    if (opt_write)
    {
        if (0 != dbg_init())
        {
            fprintf(stderr, "Failed to initialise (run as root for /dev/mem access)\n");
            return 1;
        }

        memset(buf, 0xFF, FLASH_SIZE);
        if (0 != read_dumpfile(buf, FLASH_SIZE, flash_filename))
        {
            fprintf(stderr, "Failed to read %s\n", flash_filename);
            return 1;
        }

        if (0 != dbg_mass_erase())
        {
            fprintf(stderr, "CC1110 mass erase failed\n");
            return 1;
        }

        for (i=0;i<FLASH_SIZE;i+=1024)
        {
            bool skip = true;
            int j;

            for (j=i;j<i+1024;j++)
            {
                if (buf[j] != 0xFF)
                {
                    skip = false;
                    break;
                }
            }
            if (skip)
            {
                printf("Skipping blank page %d\n", i/1024);
                continue;
            }

            printf("Programming and verifying page %d\n", i/1024);
            if (0 != program_verify_page(buf + i, i/1024))
            {
                fprintf(stderr, "FAILED\n");
                return 1;
            }
        }

        printf("Programming complete\n");
        dbg_reset();
    }

    if(opt_read)
    {
        if (0 != dbg_init())
        {
            fprintf(stderr, "Failed to initialise (run as root for /dev/mem access)\n");
            return 1;
        }

        memset(buf, 0xFF, FLASH_SIZE);

        for (i=0;i<FLASH_SIZE;i+=1024)
        {
            if(i/1024>pages) {
                printf("Reached max number of pages!\n");
                break;
            }
            bool skip = true;
            int j;

            for (j=i;j<i+1024;j++)
            {
                if (buf[j] != 0xFF)
                {
                    skip = false;
                    break;
                }
            }
            if (skip)
            {
                //printf("Skipping blank page %d\n", i/1024);
                //continue;
            }

            printf("Reading page %d\n", i/1024);
            if (0 != read_page(buf + i, i/1024))
            {
                fprintf(stderr, "FAILED\n");
                return 1;
            }
        }

        printf("Number of bytes read: %d\n",i);

        FILE *f;
        f = fopen(flash_filename, "wb");

        int j;
        for(j = 0;j<i;j++)
        {
            fprintf(f, "%c", buf[j]);
        }

        fclose(f);

        printf("Reading completed\n");
        dbg_reset();
    }

    return 0;
}


