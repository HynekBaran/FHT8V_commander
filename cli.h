/*!
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

#ifndef CLI_H_
#define CLI_H_

#include <stdio.h>
#include "defs.h"

#ifdef __ARDUINO__
#define CLI_FPUTS	fputs_P
#define CLI_FPRINTF	fprintf_P
#define CLI_STRCMP	strcmp_PF
#define STR(a)		a
#elif __AVR__
#define CLI_FPUTS	fputs_P
#define CLI_FPRINTF	fprintf_P
#define CLI_STRCMP	strcmp_PF
#define STR(a)		PSTR(a)
#else
#define CLI_FPUTS	fputs
#define CLI_FPRINTF	fprintf
#define CLI_STRCMP	strcmp
#define STR(a)		(a)
#endif

/*! Size of buffer for line editing (including trailing null) */
#define CLI_MAX_LINE_LENGTH		128
/*! Maximum number of arguments to accept (including command itself) */
#define CLI_MAX_ARGS			16
/*! Maximum number of commands to support */
#define CLI_MAX_COMMANDS		20

struct cli;
typedef int (*cli_handler_t)(struct cli*, void*, int, char**);

typedef struct {
	const char *cmd;
	cli_handler_t handler;
	void *arg;
	const char *help;
} cli_cmd_t;

typedef struct cli {
	const char *name;				/*< CLI name */
	FILE *in;						/*< Input stream */
	FILE *out;						/*< Output stream */
	cli_cmd_t cmds[CLI_MAX_COMMANDS]; /*< Command table */
	int ncmds;						/*< Number of registered commands */

	char line[CLI_MAX_LINE_LENGTH];	/*< Edit buffer */
	int pos;						/*< Next position in edit buffer */
	int is_quoted;					/*< Set if current edit position is within quotes */

	int argc;						/*< Number of arguments + command */
	char *argv[CLI_MAX_ARGS];		/*< Array of pointers to arguments */
} cli_t;

void cli_init(FILE *in, FILE *out, const char *name);
int cli_register_command(const char *cmd, cli_handler_t handler, void *arg, const char *help);
void cli_task(void);

#endif /* CLI_H_ */
