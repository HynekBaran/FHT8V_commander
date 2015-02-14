/*
 * FHT heating valve comms example with RFM22/23 for AVR
 *
 * Copyright (C) 2013 Mike Stirling
 *
 * The OpenTRV project licenses this file to you
 * under the Apache Licence, Version 2.0 (the "Licence");
 * you may not use this file except in compliance
 * with the Licence. You may obtain a copy of the Licence at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the Licence is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the Licence for the
 * specific language governing permissions and limitations
 * under the Licence.
 *
 * \file cli.c
 * \brief Simple command line interface
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "cli.h"
#include "common.h"

#define CR						'\n'
#define LF						'\r'
#define BACKSPACE				0x08
#define DELETE					0x7f
#define MAP_BS_TO_DEL

static cli_t g_ctx;

static int cli_help(cli_t *ctx, void *arg, int argc, char **argv)
{
	int n;

	for (n = 0; n < ctx->ncmds; n++) {
		/* Filter on specified command */
		if (argc > 1 && CLI_STRCMP(argv[1], ctx->cmds[n].cmd))
			continue;
		CLI_FPRINTF(ctx->out,
#ifdef __AVR__
				STR("%16S - %S\n"),
#else
				STR("%16s - %s\n"),
#endif
				ctx->cmds[n].cmd, ctx->cmds[n].help);
	}

	return 0;
}

int cli_register_command(const char *cmd, cli_handler_t handler, void *arg, const char *help)
{
	cli_t *ctx = &g_ctx;

	if (ctx->ncmds == CLI_MAX_COMMANDS) {
		PRINTF("Command registry full\n");
		return -1;
	}

	ctx->cmds[ctx->ncmds].cmd = cmd;
	ctx->cmds[ctx->ncmds].help = help;
	ctx->cmds[ctx->ncmds].handler = handler;
	ctx->cmds[ctx->ncmds].arg = arg;
	ctx->ncmds++;

	return 0;
}

void cli_init(FILE *in, FILE *out, const char *name)
{
	cli_t *ctx = &g_ctx;

	ctx->in = in;
	ctx->out = out;
	ctx->name = name;

	/* Register help command */
	cli_register_command(STR("help"), cli_help, NULL, STR("List available commands"));
}

void cli_task(void)
{
	cli_t *ctx = &g_ctx;
	int rc;

        PRINTF("CLI Ready\n\n");
	while (1) {
		char c;

		/* Display prompt */
		CLI_FPUTS(ctx->name, ctx->out);
		CLI_FPUTS(STR("> "), ctx->out);

		/* Initialise parser */
		ctx->argv[0] = ctx->line;
		ctx->argc = 1;
		ctx->pos = 0;

		/* Read line from input stream (with echo) */
		for (;;) {
			c = getc(ctx->in);
			if (!ctx->is_quoted && (c == CR || c == LF)) {
				/* End of line */
				break;
			}

#ifdef MAP_BS_TO_DEL
			/* Convert backspace to delete */
			if (c == BACKSPACE) {
				c = DELETE;
			}
#endif

			/* Ignore non-printable characters */
			if (c < 32 && c != CR) {
				continue;
			}

			/* Handle line editing */
			if (c == DELETE) {
				if (ctx->pos) {
					/* Back up the cursor */
					putc(BACKSPACE, ctx->out);
					putc(' ', ctx->out);
					putc(BACKSPACE, ctx->out);
					ctx->pos--;

					/* Handle argument deletion */
					if (ctx->pos && ctx->line[ctx->pos] != '\0' && ctx->line[ctx->pos - 1] == '\0') {
						ctx->argc--;
					}

					/* Clear the deleted character from the buffer */
					ctx->line[ctx->pos] = '\0';
				}
				continue;
			}

			/* Ignore input when buffer is full */
			if (ctx->pos == CLI_MAX_LINE_LENGTH - 1 || ctx->argc == CLI_MAX_ARGS) {
				continue;
			}

			/* Echo character back to console */
			putc(c, ctx->out);

			/* Squash spaces to nulls unless quoted */
			if (!ctx->is_quoted && c == ' ') {
//				if (ctx->pos && ctx->line[ctx->pos - 1] != '\0') {
//					/* Replace first space with a null - indicates new argument */
//					c = '\0';
//				} else {
//					/* Subsequent spaces are ignored completely */
//					continue;
//				}
				c = '\0';
			}

			/* If this character is a quote then toggle quote state.  Trailing quotes
			 * cause a null to be pushed to the buffer, indicating the start of a new
			 * argument */
			if (c == '"') {
				ctx->is_quoted = !ctx->is_quoted;
				if (ctx->is_quoted) {
					/* Leading quote - no further action */
					continue;
				} else {
					/* Convert trailing quote to null */
					c = '\0';
				}
			}

			/* Add character to buffer */
			ctx->line[ctx->pos] = c;

			/* Check for start of new argument */
			if (ctx->pos && c != '\0' && ctx->line[ctx->pos - 1] == '\0') {
				ctx->argv[ctx->argc++] = &ctx->line[ctx->pos];
			}
			ctx->pos++;
		}

		/* Terminate final argument */
		ctx->line[ctx->pos] = '\0';
		putc(CR, ctx->out);

		if (ctx->pos) {
			int i;

			/* Pass command to handler, if available */
			for (i = 0; i < ctx->ncmds; i++) {
				if (CLI_STRCMP(ctx->argv[0], ctx->cmds[i].cmd) == 0) {
					rc = (ctx->cmds[i].handler)(ctx, ctx->cmds[i].arg, ctx->argc, ctx->argv);
					if (rc != 0) {
						CLI_FPRINTF(ctx->out, STR("Error %d\n"), rc);
					}
					break;
				}
			}
			if (i == ctx->ncmds) {
				CLI_FPUTS(STR("Unknown command\n"), ctx->out);
			}
		}
	}
}

