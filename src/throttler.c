/*
 * Throttler - a utility to perform an action once a NIC has used too much volume
 *
 * Copyright (c) 2015 Marius Gräfe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * File: throttler.c
 */


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>


#define VERSION_STR "0.1"
#define NETFILE "/proc/net/dev"

typedef unsigned long long int llu;

struct options_t
{
    llu max_up;
    llu max_down;
    llu max_total;
    const char *interface;
    const char *action;
};

static struct options_t options = {0, 0, 0, NULL, NULL};


void display_help()
{
    printf(
        "Throttler " VERSION_STR "\n"
        "Author: Marius Graefe\n"
        "Usage:\n"
        "\tthrottler [{OPTIONS}] interface action\n\n"
        "Options:\n"
        "\t-u | --max-upload limit\n"
        "\t\tSpecify upload limit\n"
        "\t-d | --max-down limit\n"
        "\t\tSpecify download limit\n"
        "\t-t | --max-total limit\n"
        "\t\tSpecify limit of up- and download combined\n"
        "\t-h | --help\n"
        "\t\tDisplay this help message\n"
        "\t-v | --version\n"
        "\t\tDisplay version information\n"
        "Limits are measured in bytes and may be specified with the following suffixes:\n"
        "\tk or K for Kilobytes, m or M for Megabytes, g or G for Gigabytes, t or T for Terabytes.\n"
        "\tIf no suffix is specified pure bytes are assumed.\n"
        "\tExample: throttler eth0 -u 10G -d 10G -t 15G 'echo Throttle'\n"
        "If called without any limits it simply outputs the number of bytes received and transmitted on the specified interface\n");
}


void display_version()
{
    printf("Throttler " VERSION_STR "\n");
}


llu get_unit_factor(char unit)
{
    switch(unit)
    {
        case 'k': case 'K': return (1ULL << 10);
        case 'm': case 'M': return (1ULL << 20);
        case 'g': case 'G': return (1ULL << 30);
        case 't': case 'T': return (1ULL << 40);
        default: break;
    }
    return 1;
}


int get_bytes_formatted(const char *arg, llu *bytes)
{
    int n;
    char unit;

    n = sscanf(arg, "%llu%c", bytes, &unit);
    if (n != 1 && n != 2)
        return 0;
    if (n == 2)
        *bytes *= get_unit_factor(unit);
    return 1;
}


void parse_cmd_line(int argc, char **argv, struct options_t *options)
{
    int opt, opt_index = 0;

    static struct option long_options[] = {
            {"max-up",    required_argument, 0, 'u' },
            {"max-down",  required_argument, 0, 'd' },
            {"max-total", required_argument, 0, 't' },
            {"help",      no_argument,       0, 'h' },
            {"version",   no_argument,       0, 'v' },
            {0,           0,                 0,  0  }
    };

    while (1)
    {
        opt = getopt_long(argc, argv, "u:d:t:hv", long_options, &opt_index);
        if (opt == -1)
            break;
        switch (opt)
        {
        case 'u':
            if (!get_bytes_formatted(optarg, &options->max_up))
                printf("Invalid argument for max upload\n");
            break;
        case 'd':
            if (!get_bytes_formatted(optarg, &options->max_down))
                printf("Invalid argument for max download\n");
            break;
        case 't':
            if (!get_bytes_formatted(optarg, &options->max_total))
                printf("Invalid argument for max download\n");
            break;
        case 'v':
            display_version();
            exit(0);
        case 'h':
            display_help();
            exit(0);
        default:
            printf("?? getopt returned character code 0%o ??\n", opt);
        }
    }

    if (optind < argc)
        options->interface = argv[optind++];
    else
    {
        printf("Missing interface specifier - call with --help to get information\n");
        exit(1);
    }

    if (optind < argc)
        options->action = argv[optind++];
}


void get_dev_rx_tx(const char *interface, llu *bytes_rx, llu *bytes_tx)
{
    FILE *file;
    char format[128];
    char buf[256];

    strcpy(format, " ");
    strcat(format, interface);
    strcat(format,
        ": %llu %*llu %*llu %*llu %*llu %*llu %*llu %*llu "
        "%llu %*llu %*llu %*llu %*llu %*llu %*llu %*llu");

    file = fopen(NETFILE, "r");
    if (!file)
    {
        printf("Error opening " NETFILE ", permissions?\n");
        exit(1);
    }

    while(fgets(buf, 256, file))
    {
        if(sscanf(buf, format, bytes_rx, bytes_tx) == 2)
            goto end;
    }

    printf("Could not find interface %s in " NETFILE "\n", interface);
    exit(1);

end:
    fclose(file);
}


void perform_action()
{
   system(options.action);
}


void evaluate_bytes(llu bytes_rx, llu bytes_tx)
{
    if (options.max_up == 0 && options.max_down == 0 && options.max_total == 0)
    {
        printf("Interface %s: Down: %llu, Up: %llu\n", options.interface, bytes_rx, bytes_tx);
        exit(0);
    }

    if ((options.max_up > 0 && bytes_tx > options.max_up) ||
        (options.max_down > 0 && bytes_rx > options.max_down) ||
        (options.max_total > 0 && (bytes_tx + bytes_rx) > options.max_total))
    {
        perform_action();
    }
}


int main(int argc, char **argv)
{
    llu bytes_rx, bytes_tx;

    parse_cmd_line(argc, argv, &options);

    get_dev_rx_tx(options.interface, &bytes_rx, &bytes_tx);
    evaluate_bytes(bytes_rx, bytes_tx);

    return 0;
}
