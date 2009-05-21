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

/* Return a command after having prompted for it */
const cmd_t *readcmd(const char *prompt) {
   char *buf;
   int aargc;
   char *aargv[256]; /* With a command buffer of 512 bytes, there can't be
                      * more arguments than that */
   char *redir;
   int i,append = 0,is_pipe = 0;
   cmd_t *cmd;
   if (!isatty(fileno(stdin)))
      prompt = NULL;
   if (!(buf = readline(prompt))) {
      if (prompt)
         fprintf(stdout, "\n");
      return NULL;
   }
   for(i=strlen(buf)-1;(i>=0)&&(buf[i]==' ');buf[i--]=0);
   if (buf[0]==0)
      return &empty_cmd;

   if ((redir = index(buf, '|')))
      is_pipe = 1;
   else
      redir = index(buf, '>');
   if (redir) {
      char *s;
      for(s=redir-1;(s>=buf)&&(*s==' ');*(s--)=0);
      *(redir++) = 0;
      if (!is_pipe && *redir == '>') {
         append = 1;
         if (*(++redir) == '>') {
            fprintf(stderr,"Unexpected token '>'\n");
            return &empty_cmd;
         }
      }
      while (*redir == ' ')
         redir++;
   }
   aargc = 0;
   aargv[0] = buf;
   do {
      while (*(aargv[aargc]) == ' ')
         *(aargv[aargc]++) = 0;
   } while((aargv[++aargc] = strchr(aargv[aargc - 1], ' ')));

   cmd = calloc(sizeof(cmd_t),1);
   cmd->argc = aargc;
   cmd->argv = malloc(aargc * sizeof(char *));
   for(i=0; i<cmd->argc; i++)
      cmd->argv[i] = strdup(aargv[i]);
   cmd->piped = is_pipe;
   cmd->append = append;
   if (redir)
      cmd->redir = strdup(redir);

   return cmd;
}

/* Free a command */
void freecmd(const cmd_t *cmd) {
   int i;
   if (!cmd || cmd == &empty_cmd)
      return;

   for(i=0; i<cmd->argc; i++)
      free(cmd->argv[i]);
   free(cmd->argv);
   free(cmd->redir);
   free((cmd_t *)cmd);
}
