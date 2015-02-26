#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include "ptools.h"
#include "disassembleString.h"

/*#define DEBUG 2*/
int disassembleString(char input[], char*** keyArray, char*** argumentArray, char*** formatArray,
                      char primaryToken[],
                      char secondaryToken[],
                      char thirdToken[],
                      char spaceToken[],
                      char tokenProtection)
{
   int     size_of_input       = 0;
   char   *local               = NULL;
   char  **argArray            = NULL;
   char  **valArray            = NULL;
   char  **pLocalKeyArray      = NULL;
   char  **pLocalArgumentArray = NULL;
   char  **pLocalFormatArray   = NULL;
   int     i                   = 0;
   int     found_elements      = 0;
   int     found_elements2     = 0;
   int     totallength         = 0;
   unsigned int length         = 0;

   size_of_input = strlen(input)+1;

   /*
    * check if string contains tokens at all, 
    * if not exit (-1) 
    */
   if(NULL == strstr(&input[0], primaryToken))
   {
      message(stderr,__FILE__,__LINE__,"ERROR","disassembleString",
              "\n\tstring `%s' \n\tdoesn't contain any key identifier (`%s') impossible to disassemble\n",
              &input[0],primaryToken);
      return -1;
   }

   /*
    * check if string contains = at all, 
    * if not exit 
    */
   if(NULL == strstr(&input[0], secondaryToken))
   {
      message(stderr,__FILE__,__LINE__,"ERROR","disassembleString",
              " \n\tstring `%s' \n\tdoesn't contain any `=' impossible to disassemble\n",
              &input[0]);
      return -1;
   }

   /*
    * local copy of input string 
    */
   local = (char *) malloc (sizeof (char) * size_of_input+1);
   if (!local)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","disassembleString",
              "couldn't allocate memory.\n");
      return -1;
   }
   /*clear*/
   memset(local, 0, size_of_input+1);
   strncpy(&local[0], &input[0], size_of_input+1);

   /*
    * clean input string of SPACE_TOKEN 
    */
   size_of_input = removeToken(local, spaceToken, tokenProtection);

   /*
    * check if string contains minimum number of characters: length of token + `=' + minimum 1 character
    * if not exit 
    */
   if (size_of_input < (strlen(primaryToken)+2))
   {
      message(stderr,__FILE__,__LINE__,"ERROR","disassembleString",
              "\n\tlenght of reduced string `%s' \n\tis less than minimum of %i characters\n",
              &local[0],
              strlen(primaryToken)+2);
      return -1;
   }

   /*
    * local argument array of char pointers 
    */
   argArray = NULL;

   /*
    * split up input string into parts separated by primary token 
    */
   found_elements=0;
   found_elements = divideUpStrings (local, &argArray, primaryToken, 0, tokenProtection);
   safeStringFree(&local, totallength+1);
   if (found_elements < 0)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","disassembleString",
              "divideUpStrings() failed\n");
      return -1;
   }

   /*
    * prepare local key, argument and format pointer arrays 
    */
   if ( 65536 < sizeof (char*) * found_elements)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","disassembleString",
              "could not allocate memory, %i exceeds size of 64kB.\n",
              sizeof (char*) * found_elements);
      return -1;
   }
   pLocalKeyArray      = (char **) calloc (found_elements,sizeof (char *));
   pLocalArgumentArray = (char **) calloc (found_elements,sizeof (char *));
   pLocalFormatArray   = (char **) calloc (found_elements,sizeof (char *));
   if (!pLocalKeyArray || !pLocalArgumentArray || !pLocalFormatArray)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","disassembleString",
              "couldn't allocate memory\n");
      return -1;
   }

   /*
    * fill arrays and subdivide argArray parts
    */
   for (i = 0; i < found_elements; i++)
   {
      /*
       * subdivide each argArray looking for secondaryToken 
       */

      /* init each element */
      pLocalArgumentArray[i] = NULL;
      pLocalKeyArray[i]      = NULL;
      pLocalFormatArray[i]   = NULL;

      found_elements2 = 0;
      length = strlen (argArray[i]);

      /* - local copy */
      local = (char *) malloc (sizeof (char) * length + 1);
      if (!local)
      {
         message(stderr,__FILE__,__LINE__,"ERROR","disassembleString",
                 "couldn't allocate memory.\n");
         return -1;
      }

      strncpy (&local[0], argArray[i], length+1);

      /* - clean argArray[i] */
      safeStringFree(&argArray[i], length);

      /* - local results pointer array */
      valArray = NULL;

      /* - split up input string into parts separated by secondary (assignment) token, i.e a=b */
      found_elements2 = divideUpStrings (&local[0], &valArray, secondaryToken, 2, tokenProtection);
      if (found_elements2 < 0)
      {
         /* error condition of divideUpStrings */
         message(stderr,__FILE__,__LINE__,"ERROR","disassembleString",
                 "divideUpStrings() failed!\n");
         return -1;
      }

      /* - clean local copy */
      safeStringFree(&local, length+1);

      /* - assign results to local key, argument and format arrays */
      pLocalKeyArray[i] = valArray[0];

      if (found_elements2 > 1)
      {
         pLocalArgumentArray[i] = valArray[1];
      }

      /* - clean local results array pointer not yet the values*/
      safePArrayFree((void**) valArray, 2);

      if (pLocalArgumentArray[i])
      {
         /*
          * subdivide each ArgumentArray looking for thirdToken, i.e. "" 
          */
         found_elements2 = 0;
         length = strlen (pLocalArgumentArray[i]);

         /* - local copy */
         local = (char *) malloc (sizeof (char) * length + 1);
         if (!local)
         {
            message(stderr,__FILE__,__LINE__,"ERROR","disassembleString",
                    "couldn't allocate memory\n");
            return -1;
         }

         strncpy (&local[0], pLocalArgumentArray[i], length+1);

         /* - local results pointer array */
         valArray = NULL;

         /* - split up input string into parts separated by third (pair) token, i.e b"a..., but   */
         found_elements2 = divideUpStrings (&local[0], &valArray, thirdToken, 2, tokenProtection);
         if (found_elements2 < 0)
         {
            /* error condition of divideUpStrings */
            message(stderr,__FILE__,__LINE__,"ERROR","disassembleString",
                    "divideUpStrings failed!\n");
            return -1;
         }

         /* - clean local copy */
         safeStringFree(&local, length+1);

         /* - if third token was found */
         /* - reassign results to argument and format arrays */
         if (found_elements2>1)
         {
            safeStringFree(&pLocalArgumentArray[i], strlen(pLocalArgumentArray[i])+1);

            pLocalArgumentArray[i] = (char*) malloc (sizeof(char)*(strlen(valArray[0])+1));
            if (!pLocalArgumentArray[i])
            {
               message(stderr,__FILE__,__LINE__,"ERROR","disassembleString", "couldn't allocate memory\n");
               return -1;
            }
            strncpy(&pLocalArgumentArray[i][0],&valArray[0][0],(strlen(valArray[0])+1));
            safeStringFree(&valArray[0], strlen(valArray[0])+1);

            pLocalFormatArray[i] = (char*) malloc (sizeof(char)*(strlen(valArray[1])+1));
            if (!pLocalFormatArray[i])
            {
               message(stderr,__FILE__,__LINE__,"ERROR","disassembleString", "couldn't allocate memory\n");
               return -1;
            }
            strncpy(&pLocalFormatArray[i][0],&valArray[1][0],(strlen(valArray[1])+1));
            safeStringFree(&valArray[1], strlen(valArray[1])+1);
         }
         else if(found_elements2==1)
         {
            safeStringFree(&valArray[0], strlen(valArray[0])+1);
         }

         safePArrayFree((void**) valArray, 2);
      }
   }
   safePArrayFree((void**)argArray, found_elements);

   /*
    * assigning pointer to locals to pointer to pointer of pointer array 
    */
   *keyArray      = pLocalKeyArray;
   *argumentArray = pLocalArgumentArray;
   *formatArray   = pLocalFormatArray;

   return found_elements;
}

int disassembleDIMFormatString(char format[], char ***typeArray, int **numberArray,
                               char fourthToken[],
                               char fifthToken[],
                               char tokenProtection)
{
   int     size_of_input       = 0;
   char   *local               = NULL;
   char  **argArray            = NULL;
   char  **valArray            = NULL;
   char  **pLocalTypeArray     = NULL;
   int    *pLocalNumberArray   = NULL;
   int     i                   = 0;
   int     found_elements      = 0;
   int     found_elements2     = 0;
   unsigned int length         = 0;

   size_of_input = strlen(format)+1;

   /* local copy of format string */
   local = (char *) malloc (sizeof (char) * size_of_input);
   if (!local)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","disassembleDIMFormatString", "couldn't allocate memory\n");
      return -1;
   }
   strncpy(&local[0], &format[0], size_of_input);

   /* local argument array of char pointers */
   argArray = NULL;
   /* split up format string into parts separated by fourth token */
   found_elements=0;
   found_elements = divideUpStrings (local, &argArray, fourthToken, size_of_input, tokenProtection );
   safeStringFree(&local, size_of_input);
   if (found_elements < 0)
   {
      /* error condition of divideUpStrings */
      message(stderr,__FILE__,__LINE__,"ERROR","disassembleDIMFormatString", "couldn't allocate memory\n");
      return -1;
   }

   /* prepare local key, argument and format pointer arrays */
   if ( 65536 < sizeof (char*) * found_elements)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","disassembleDIMFormatString",
              "could not allocate memory, %i exceeds size of 64kB.\n",
              sizeof (char*) * found_elements);
      return -1;
   }
   pLocalTypeArray   = (char **) calloc (found_elements, sizeof (char *));
   if ( 65536 < sizeof (int) * found_elements)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","disassembleDIMFormatString",
              "could not allocate memory, %i exceeds size of 64kB.\n",
              sizeof (char*) * found_elements);
      return -1;
   }
   pLocalNumberArray = (int *) calloc (found_elements, sizeof(int));
   if (!pLocalNumberArray || !pLocalTypeArray )
   {
      message(stderr,__FILE__,__LINE__,"ERROR","disassembleDIMFormatString", "couldn't allocate memory\n");
      return -1;
   }

   /* fill arrays and subdivide argArray parts*/
   for (i = 0; i < found_elements; i++)
   {
      /*
       * subdivide each argArray looking for fifth token 
       */

      /* init each element */
      pLocalTypeArray[i] = NULL;
      pLocalNumberArray[i] = -1;

      found_elements2 = 0;
      length = strlen (argArray[i]);
      /* - local copy */
      local = (char *) malloc (sizeof (char) * length + 1);
      if (!local)
      {
         message(stderr,__FILE__,__LINE__,"ERROR","disassembleDIMFormatString", "couldn't allocate memory\n");
         return -1;
      }
      strncpy (&local[0], argArray[i], length+1);
      /* - clean argArray[i] */
      safeStringFree(&argArray[i], length);
      /* - local results pointer array */
      valArray = NULL;
      /* - split up input string into parts separated by fifth (assignment) token, i.e a=b */
      found_elements2 = divideUpStrings(&local[0], &valArray, fifthToken, 2, tokenProtection);
      /* - clean local copy */
      safeStringFree(&local, length+1);
      if (found_elements2 < 0)
      {
         /* error condition of divideUpStrings */
         message(stderr,__FILE__,__LINE__,"ERROR","disassembleDIMFormatString",
                 "divideUpStrings() failed.\n");
         return -1;
      }
      /* - assign results to local key, argument and format arrays */
      if (found_elements2 == 1)
      {
         pLocalTypeArray[i] = valArray[0];
         pLocalNumberArray[i] = 0;
      }
      else if (found_elements2 == 2)
      {
         pLocalTypeArray[i]   = valArray[0];
         pLocalNumberArray[i] = atoi(valArray[1]);
         safeStringFree(&valArray[1], strlen(valArray[1])+1);
      }
      else
      {
         pLocalTypeArray[i]   = NULL;
         pLocalNumberArray[i] = -1;
      }
      /* - clean local results array pointer not yet the values*/
      safePArrayFree((void**) valArray, 2);
   }

   /* assigning pointer to locals to pointer to pointer of pointer array */
   *typeArray   = pLocalTypeArray;
   *numberArray = pLocalNumberArray;

   /* clean up*/
   safePArrayFree((void**) argArray, found_elements);
   return found_elements;
}

void  safeFreeOfDisassemblyStringArrays(int found_elements, char*** keyArray, char*** argumentArray, char*** formatArray)
{
   int i=0;
   for (i = 0; i < found_elements; i++)
   {
      if (NULL != (*keyArray)[i])
      {
         safeStringFree(&(    (*keyArray)[i]),strlen((*keyArray)[i]));
      }
      if (NULL != (*argumentArray)[i])
      {
         safeStringFree(&(    (*argumentArray)[i]),strlen((*argumentArray)[i]));
      }
      if (NULL != (*formatArray)[i])
      {
         safeStringFree(&((*formatArray)[i]),strlen((*formatArray)[i]));
      }
   }

   safePArrayFree((void**) *keyArray, found_elements);
   safePArrayFree((void**) *argumentArray, found_elements);
   safePArrayFree((void**) *formatArray, found_elements);
	*keyArray = NULL;
	*argumentArray = NULL;
	*formatArray = NULL;

}
