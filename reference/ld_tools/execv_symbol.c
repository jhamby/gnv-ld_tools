#define _POSIX_EXIT 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <descrip.h>
#include <stsdef.h>
#include <ssdef.h>
#include <libclidef.h>

#pragma member_alignment save
#pragma nomember_alignment longword
struct item_list_3
{
  unsigned short len;
  unsigned short code;
  void * bufadr;
  unsigned short * retlen;
};

#pragma member_alignment

int
LIB$GET_SYMBOL (const struct dsc$descriptor_s * symbol,
                struct dsc$descriptor_s * value,
                unsigned short * value_len,
                const unsigned long * table);

#pragma member_alignment restore

#define MAX_DCL_SYMBOL_LEN (255)
#if __CRTL_VER >= 70302000 && !defined(__VAX)
# define MAX_DCL_SYMBOL_VALUE (8192)
#else
# define MAX_DCL_SYMBOL_VALUE (1024)
#endif

char symbol_value[MAX_DCL_SYMBOL_VALUE + 1];

char * get_symbol(const char * name) {
    unsigned long old_symtbl;
    struct dsc$descriptor_s value_desc;
    struct dsc$descriptor_s symbol_desc;
    unsigned short value_len;
    int status;

    symbol_desc.dsc$w_length = strlen(name);
    symbol_desc.dsc$a_pointer = (char *) name; /* cast ok */
    symbol_desc.dsc$b_dtype = DSC$K_DTYPE_T;
    symbol_desc.dsc$b_class = DSC$K_CLASS_S;

    value_desc.dsc$a_pointer = symbol_value;
    value_desc.dsc$w_length = MAX_DCL_SYMBOL_VALUE;
    value_desc.dsc$b_dtype = DSC$K_DTYPE_T;
    value_desc.dsc$b_class = DSC$K_CLASS_S;

    status = LIB$GET_SYMBOL(&symbol_desc, &value_desc,
                            &value_len, &old_symtbl);
    if (!$VMS_STATUS_SUCCESS (status)) {
        return NULL;
    } else {
        symbol_value[value_len] = 0;
    }

    return symbol_value;
}

#define MAX_ARGS 1024
#define MAX_SIZE 1024

int main(int argc, char ** argv) {
   int status;
   char *new_argv[MAX_ARGS + 1];  /* Big enough */
   char * symbol_str;

   if (argc != 3) {
       puts("usage:  execv VMS-EXE DCL-SYMBOL-NAME");
       exit(1);
   }

   symbol_str = get_symbol(argv[2]);
   if (symbol_str == NULL) {
       puts("Unable to get SYMBOL name value.");
       exit(2);
   }

   {
       int i = 0;
       int j = 0;
       int ac = 0;
       int ws = 1;
       int quote = 0;
       new_argv[ac] = malloc(MAX_SIZE + 1); /* Max EXE path */
       new_argv[ac+1] = NULL;
       while (symbol_str[i] != 0) {
           new_argv[ac][j] = 0;
           switch(symbol_str[i]) {
           case ' ':
           case '\t':
               /* Parameter delimeter unless quoted */
               if (quote != 0) {
                   new_argv[ac][j] = symbol_str[i];
                   j++;
                   break;
               }
               if (ws != 1) {
                   j = 0;
                   ac++;
                   if (ac == MAX_ARGS) {
                       break;
                   }
                   new_argv[ac] = malloc(MAX_SIZE);
                   new_argv[ac + 1] = NULL;
                   ws = 1;
               }
               break;
           case '\'':
           case '"':
               ws = 0;
               if (symbol_str[i] == quote) {
                   /* No nesting of quotes in test harness */
                   quote = 0;
               } else {
                   quote = symbol_str[i];
               }
               new_argv[ac][j] = symbol_str[i];
               j++;
               break;
           case '\\':
               /* Ignore escapes in single quotes */
               ws = 0;
               if (quote == '\'') {
                   new_argv[ac][j] = symbol_str[i];
                   j++;
                   break;
               }
               i++;
               switch(symbol_str[i] == 0) {
               case 0:
                   break;
               case 'n':
                   new_argv[ac][j] == '\n';
                   j++;
                   break;
               case 'r':
                   new_argv[ac][j] == '\r';
                   j++;
                   break;
               case 't':
                   new_argv[ac][j] == '\t';
                   j++;
                   break;
               default:
                   new_argv[ac][j] == symbol_str[i];
                   j++;
               }
               break;

           default:
               new_argv[ac][j] = symbol_str[i];
               ws = 0;
               j++;
           }
           if (j == MAX_SIZE) {
               /* error for this test harness */
               new_argv[ac][MAX_SIZE] = 0;
               break;
           }
           i++;
       }
   }

   status = execv(argv[1], new_argv);

   exit(status);
}
