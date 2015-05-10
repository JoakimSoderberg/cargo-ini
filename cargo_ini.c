//
// The MIT License (MIT)
//
// Copyright (c) 2015 Joakim Soderberg <joakim.soderberg@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "cargo/cargo.h"
#include "alini/alini.h"
#include "uthash.h"

typedef enum debug_level_e
{
   NONE  = 0,
   ERROR = (1 << 0),
   WARN  = (1 << 1),
   INFO  = (1 << 2),
   DEBUG = (1 << 3)
} debug_level_t;

#define MAX_CONFIG_KEY 1024
#define MAX_CONFIG_VAL 1024

typedef struct conf_arg_s
{
	char key[MAX_CONFIG_KEY];
	char *vals[MAX_CONFIG_VAL];
	size_t val_count;
	UT_hash_handle hh;
} conf_arg_t;

typedef struct args_s
{
	alini_parser_t *parser;
	char **config_argv;
	int config_argc;
	conf_arg_t *config_args;
	size_t config_args_count;

	debug_level_t debug;
	char *config_path;
	int a;
	int b;
	int c;
	int d;
} args_t;

static void alini_cb(alini_parser_t *parser,
					char *section, char *key, char *value)
{
	conf_arg_t *it = NULL;
	args_t *args = (args_t *)alini_parser_get_context(parser);

	if (args->debug)
	{
		printf("Alini callback - Key: %s, Val: %s\n", key, value);
	}

	// Either find an existing config argument with this key.
	HASH_FIND_STR(args->config_args, key, it);

	// Or add a new config argument.
	if (!it)
	{
		if (!(it = calloc(1, sizeof(conf_arg_t))))
		{
			fprintf(stderr, "Out of memory\n");
			exit(-1);
		}

		strncpy(it->key, key, MAX_CONFIG_KEY - 1);
		HASH_ADD_STR(args->config_args, key, it);
	}

	if (!(it->vals[it->val_count++] = strdup(value)))
	{
		fprintf(stderr, "Out of memory\n");
		exit(-1);
	}
}

void args_init(args_t *args)
{
	// Init alini parser.
	memset(args, 0, sizeof(args_t));
}

void args_destroy(args_t *args)
{
	size_t i;
	conf_arg_t *it = NULL;
	conf_arg_t *tmp = NULL;

	// free the hash table contents.
	HASH_ITER(hh, args->config_args, it, tmp)
	{
		HASH_DEL(args->config_args, it);
		for (i = 0; i < it->val_count; i++)
		{
			free(it->vals[i]);
			it->vals[i] = NULL;
		}

		free(it);
	}

	cargo_free_commandline(&args->config_argv, args->config_argc);
}

void print_hash(conf_arg_t *config_args)
{
	size_t i = 0;
	conf_arg_t *it = NULL;
	conf_arg_t *tmp = NULL;

	// free the hash table contents.
	HASH_ITER(hh, config_args, it, tmp)
	{
		printf("(%lu) %s = ", it->val_count, it->key);

		for (i = 0; i < it->val_count; i++)
		{
			printf("%s", it->vals[i]);
			if (i != (it->val_count - 1))
			{
				printf(",");
			}
		}

		printf("\n");
	}
}

void print_commandline(args_t *args)
{
	int i;
	for (i = 0; i < args->config_argc; i++)
	{
		printf("%s ", args->config_argv[i]);
	}

	printf("\n");
}

void build_config_commandline(args_t *args)
{
	size_t j = 0;
	int i = 0;
	conf_arg_t *it = NULL;
	conf_arg_t *tmp = NULL;
	char tmpkey[1024];
	
	args->config_argc = 0;

	HASH_ITER(hh, args->config_args, it, tmp)
	{
		args->config_argc += 1 + it->val_count;
	}

	if (!(args->config_argv = calloc(args->config_argc, sizeof(char *))))
	{
		fprintf(stderr, "Out of memory!\n");
		exit(-1);
	}

	HASH_ITER(hh, args->config_args, it, tmp)
	{
		snprintf(tmpkey, sizeof(tmpkey) - 1, "--%s", it->key);
	
		if (!(args->config_argv[i++] = strdup(tmpkey)))
		{
			fprintf(stderr, "Out of memory!\n");
			exit(-1);
		}

		for (j = 0; j < it->val_count; j++)
		{
			if (!(args->config_argv[i++] = strdup(it->vals[j])))
			{
				fprintf(stderr, "Out of memory!\n");
				exit(-1);
			}
		}
	}
}

static int perform_config_parse(cargo_t cargo, args_t *args)
{
	int cfg_err = 0;

	if ((cfg_err = alini_parser_create(&args->parser, args->config_path)) < 0)
	{
		cargo_print_usage(cargo, CARGO_USAGE_SHORT_USAGE);
		fprintf(stderr, "\nFailed to load config: %s\n", args->config_path);
		return -1;
	}

	alini_parser_setcallback_foundkvpair(args->parser, alini_cb);
	alini_parser_set_context(args->parser, args);
	alini_parser_start(args->parser);

	return 0;
}

static int parse_config(cargo_t cargo, args_t *args)
{
	cargo_parse_result_t err = 0;

	// Parse the ini-file and store contents in a hash table.
	if (perform_config_parse(cargo, args))
	{
		return -1;
	}

	// Build an "argv" string from the hashtable contents:
	//   key1 = 123
	//   key1 = 456
	//   key2 = 789
	// Becomes:
	//   --key1 123 456 --key2 789
	build_config_commandline(args);

	if (args->debug)
	{
		print_hash(args->config_args);
		print_commandline(args);
	}

	// Parse the "fake" command line using cargo. We turn off the
	// internal error output, so the errors are more in the context
	// of a config file.
	if ((err = cargo_parse(cargo, CARGO_NOERR_OUTPUT,
							0, args->config_argc, args->config_argv)))
	{
		size_t k = 0;

		cargo_print_usage(cargo, CARGO_USAGE_SHORT_USAGE);

		fprintf(stderr, "Failed to parse config: %d\n", err);
		
		switch (err)
		{
			case CARGO_PARSE_UNKNOWN_OPTS:
			{
				const char *opt = NULL;
				size_t unknown_count = 0;
				const char **unknowns = cargo_get_unknown(cargo, &unknown_count);

				fprintf(stderr, "Unknown options:\n");

				for (k = 0; k < unknown_count; k++)
				{
					opt = unknowns[k] + strspn("--", unknowns[k]);
					fprintf(stderr, " %s\n", opt);
				}
				break;
			}
			default: break;
		}

		return -1;
	}

	return 0;
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
	ret |= cargo_add_option(cargo, 0, "<vals> --centauri -c", "Centauri", "i", &args.c);
	ret |= cargo_add_option(cargo, 0, "<vals> --delta -d", "Delta", "i", &args.d);

	assert(ret == 0);

	// Parse once to get --config value.
	if (cargo_parse(cargo, 0, 1, argc, argv))
	{
		goto fail;
	}

	// Read ini file and translate that into an argv that cargo can parse.
	printf("Config path: %s\n", args.config_path);
	if (args.config_path && parse_config(cargo, &args))
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

fail:
	cargo_destroy(&cargo);
	args_destroy(&args);

	return 0;
}


