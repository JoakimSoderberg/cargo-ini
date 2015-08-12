
#include <stdio.h>
#include <stdlib.h>
#include "cargo_ini.h"
#include <assert.h>


typedef enum debug_level_e
{
   NONE  = 0,
   ERROR = (1 << 0),
   WARN  = (1 << 1),
   INFO  = (1 << 2),
   DEBUG = (1 << 3)
} debug_level_t;


typedef struct args_s
{
	conf_ini_args_t ini_args;

	debug_level_t debug;
	char *config_path;
	int a;
	int b;
	int c;
	int d;
} args_t;

void args_init(args_t *args)
{
	// Init alini parser.
	memset(args, 0, sizeof(args_t));
}


int main(int argc, char **argv)
{
	int i;
	int ret = 0;
	cargo_t cargo;
	args_t args;

	args_init(&args);

	if (cargo_init(&cargo, CARGO_AUTOCLEAN, argv[0]))
	{
		fprintf(stderr, "Failed to init command line parsing\n");
		return -1;
	}

	cargo_set_description(cargo,
		"Specify a configuration file, and try overriding the values set "
		"in it using the command line options.");

	// Combine flags using OR.
	ret |= cargo_add_option(cargo, 0,
			"--verbose -v", "Verbosity level",
			"b|", &args.debug, 4, ERROR, WARN, INFO, DEBUG);

	ret |= cargo_add_option(cargo, 0,
			"--config -c", "Path to config file",
			"s", &args.config_path);

	ret |= cargo_add_group(cargo, 0, "vals", "Values", "Some options to test with.");
	ret |= cargo_add_option(cargo, 0, "<vals> --alpha -a", "Alpha", "i", &args.a);
	ret |= cargo_add_option(cargo, 0, "<vals> --beta -b", "Beta", "i", &args.b);
	ret |= cargo_add_option(cargo, 0, "<vals> --centauri", "Centauri", "i", &args.c);
	ret |= cargo_add_option(cargo, 0, "<vals> --delta -d", "Delta", "i", &args.d);

	assert(ret == 0);

	// Parse once to get --config value.
	if (cargo_parse(cargo, 0, 1, argc, argv))
	{
		goto fail;
	}

	// Read ini file and translate that into an argv that cargo can parse.
	printf("Config path: %s\n", args.config_path);
	if (args.config_path && parse_config(cargo, args.config_path, &args.ini_args))
	{
		goto fail;
	}

	// And finally parse the commandline to override config settings.
	if (cargo_parse(cargo, 0, 1, argc, argv))
	{
		goto fail;
	}

	printf("%10s: %d\n", "Alpha",		args.a);
	printf("%10s: %d\n", "Beta",		args.b);
	printf("%10s: %d\n", "Centauri",	args.c);
	printf("%10s: %d\n", "Delta",		args.d);
	printf("%10s: 0x%X\n", "Verbosity", args.debug);

fail:
	cargo_destroy(&cargo);
	ini_args_destroy(&args.ini_args);

	return 0;
}
