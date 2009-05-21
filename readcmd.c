/*
 * vmfs-tools - Tools to access VMFS filesystems
 * Copyright (C) 2009 Christophe Fillot <cf@utc.fr>
 * Copyright (C) 2009 Mike Hommey <mh@glandium.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <readline/readline.h>
#include "readcmd.h"

static const cmd_t empty_cmd = { 0, };

/* Argv allocation increment */
#define ARGV_INCR 16

/* Return a command after having prompted for it */
const cmd_t *readcmd(const char *prompt) {
   char *buf;
   int i;
   cmd_t *cmd = NULL;
   if (!isatty(fileno(stdin)))
      prompt = NULL;
   if (!(buf = readline(prompt))) {
      if (prompt)
         fprintf(stdout, "\n");
      return NULL;
   }
   for(i=strlen(buf)-1;(i>=0)&&(buf[i]==' ');buf[i--]=0);
   if (buf[0]==0) {
      free(buf);
      return &empty_cmd;
   }

   cmd = calloc(sizeof(cmd_t),1);
   cmd->buf = buf;
   if ((cmd->redir = index(buf, '|')))
      cmd->piped = 1;
   else
      cmd->redir = index(buf, '>');
   if (cmd->redir) {
      char *s;
      for(s=cmd->redir-1;(s>=buf)&&(*s==' ');*(s--)=0);
      *(cmd->redir++) = 0;
      if (!cmd->piped && *cmd->redir == '>') {
         cmd->append = 1;
         if (*(++cmd->redir) == '>') {
            fprintf(stderr,"Unexpected token '>'\n");
            free(cmd);
            free(buf);
            return &empty_cmd;
         }
      }
      while (*cmd->redir == ' ')
         cmd->redir++;
   }
   cmd->argv = malloc(ARGV_INCR * sizeof(char *));
   cmd->argv[0] = buf;
   do {
      while (*(cmd->argv[cmd->argc]) == ' ')
         *(cmd->argv[cmd->argc]++) = 0;
      if (!(++cmd->argc % ARGV_INCR)) {
         char **newargv = realloc(cmd->argv,
                                  (cmd->argc + ARGV_INCR) * sizeof(char *));
         if (newargv) {
            cmd->argv = newargv;
         } else {
            freecmd(cmd);
            fprintf(stderr, "Not enough memory\n");
            return NULL;
         }
      }
   } while((cmd->argv[cmd->argc] = strchr(cmd->argv[cmd->argc - 1], ' ')));

   return cmd;
}

/* Free a command */
void freecmd(const cmd_t *cmd) {
   if (!cmd || cmd == &empty_cmd)
      return;

   free(cmd->argv);
   free(cmd->buf);
   free((cmd_t *)cmd);
}
