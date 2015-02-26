/*
 * ptools.c
 *  programming tools
 * Author: p.zumbruch, p.zumbruch@gsi.de
 * created: 15.02.2007
 * last change: 04.04.2007
 */
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <execinfo.h> /* for backtrace*/
#include <limits.h>

#include "ptools.h"

void message(FILE *stream, const char *file, int line, const char* type, const char* fcn, const char* format, ...)
{
   /* prints message to stream,
    * as fprintf does 
    * see settings format printf etc. 
    * 
    * in addition line message type and functionname are added in front of the message given in format and ...
    * if fcn is set to NULL / 0 its content is ommitted.
    */

   va_list argumentPointer;
   va_start(argumentPointer, format);

   if (NULL != type)
   {
      fprintf(stream,"%s",type);
   }
   if (0 < line)
   {
      fprintf(stream," in LINE %5i",line);
   }
   if (NULL != file)
   {
      fprintf(stream," of FILE %s",file);
   }
   if ( NULL != fcn)
   {
      fprintf(stream," in %s(): ",fcn);
   }
   else
   {
      if ( NULL != type || 0 < line || NULL != file || NULL != fcn)
      {
         fprintf(stream,": ");
      }
   }
   if ( 0 > vfprintf(stream, format, argumentPointer))
   {
      fprintf(stream,"LINE %5i - %s in %s()",__LINE__+1, "ERROR", "message");
      fprintf(stream,"call of vfprintf() failed\n");
   }
   fflush(stream);
   va_end(argumentPointer);
   return;
}

void backTrace(size_t level)
{
   /*
        // prints out level steps of function calls
        // before the function call (backtrace)
        // Useful for debugging
    */

   void** array = (void*) calloc(level, sizeof(void));
   int    size = backtrace (array, level);

   if(0 != size)
   {
      char **strings;
      strings = backtrace_symbols (array, size);

      fprintf(stderr,"Obtained %zd stack frames.\n", size);

      int i=0;
      for (i = 0; i < size; i++)
      {
         /*
         if(gSystem->Which("c++filt","/dev/null")==0)
      {
                  // do nice print out using c++filt from gnu binutils
                  TString symbol=strings[i];
         TString lib   =symbol;

         lib.Replace   (lib.First('(')   ,lib.Length()-lib.First('('),"");
         symbol.Replace(0                ,symbol.First('(')+1        ,"");
         symbol.Replace(symbol.First('+'),symbol.Length()            ,"");

         cout<<lib.Data()<<": "<<flush;
         gSystem->Exec(Form("c++filt %s",symbol.Data()));
      }
         else
      {
         // do normal print out

         cout<<strings[i]<<endl;
         */
         if (strings && strings[i])
         {
            fprintf(stderr, "%s\n", strings[i]);
         }
      }
      free(strings);
   }
   else
   {
      message(stderr,__FILE__,__LINE__, "ERROR", "backTrack", "Could not retrieve backtrace information");
   }
   safePArrayFree(array,level);
}
/* string & token */

int replaceStringlets(char* input, char **output, char* token, char* replacement)
{
   /* replaces in string input all occurences of string token by replacement and assings it to output
    * 
    * return values
    * 	-1 : case of errors
    *    strlen of output: else
    * 
    * example: 
    * 	char input[]="hello";
    * 	char* output = NULL
    * 	int size = replaceStringlets(input, &output, "l","m");
    * 	output-> 'hemmo'
    */

   /* object stack */
   stack *objects = NULL;
   if (NULL == createStack(&objects))
   {
      message(stdout,__FILE__,__LINE__,"ERROR","replaceStringlets","createStack failed\n");
      return -1;
   }
   stack *persistentObjects = NULL;
   if (NULL == createStack(&persistentObjects))
   {
      message(stdout,__FILE__,__LINE__,"ERROR","replaceStringlets","createStack failed\n");
      deleteStack(&objects);
      return -1;
   }

   /* check inputs */
   if (NULL == input)
   {
      message(stdout,__FILE__,__LINE__,"ERROR","replaceStringlets","input arg1 = NULL\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   int inputLength = stringLength(input);
   if (0 == inputLength)
   {
      message(stdout,__FILE__,__LINE__,"ERROR","replaceStringlets","input arg1 = empty string\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   if (-1 > inputLength)
   {
      message(stdout,__FILE__,__LINE__,"ERROR","replaceStringlets","input arg1 exceeds maximum size of %i characters\n"
              ,-inputLength);
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }

   if (NULL == output)
   {
      message(stdout,__FILE__,__LINE__,"ERROR","replaceStringlets","output arg2 = NULL\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   if (NULL == token)
   {
      message(stdout,__FILE__,__LINE__,"ERROR","replaceStringlets","token arg3 = NULL\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   if (NULL == replacement)
   {
      message(stdout,__FILE__,__LINE__,"ERROR","replaceStringlets","replacement arg4 = NULL\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   if (0 == strlen(token))
   {
      message(stdout,__FILE__,__LINE__,"ERROR","replaceStringlets","token is empty\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }

   /* scan replace */

   int increment = 0;
   increment = (int) (0.1 * inputLength);
   if (increment<5)
   {
      increment=5;
   }

   /* disassemble */
   inputLength = strlen(input);
   int tokenLength = strlen(token);
   int replacementLenght = strlen(replacement);
   int outputLength = inputLength+increment;
   int inputIndex = 0;
   int outputIndex = 0;
   int beginIndex = 0;

   char* outputString = NULL;
   outputString = (char*) malloc((outputLength+1)*sizeof(char));
   if (NULL == outputString)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","replaceStringlets","couldn't allocate memory\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   pushStack(persistentObjects, outputString, outputLength+1);
   memset(outputString,0,outputLength+1);

   int size = 0;
   while(inputIndex <= inputLength)
   {
      /* no space left for trailing token ? or match */
      if (inputIndex + tokenLength > inputLength || 0 != strncmp(&input[inputIndex],token,tokenLength))
      {
         /* readjustment of output string if necessary */
         if (outputIndex + replacementLenght > outputLength)
         {
            int oldLength = outputLength;
            outputLength+=(increment+replacementLenght);
            outputString = (char*) realloc(outputString,(outputLength+1)*sizeof(char));
            if (NULL == outputString)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","replaceStringlets","couldn't allocate memory\n");
               deleteStack(&objects);
               deleteStack(&persistentObjects);
               return -1;
            }
            pushStack(persistentObjects, outputString, outputLength);
            memset(&outputString[oldLength],0,outputLength-oldLength+1);
         }
         /* copy chars */
         outputString[outputIndex] = input[inputIndex];
         inputIndex++;
         outputIndex++;
         continue;
      }

      /* passed all comparisons - token found a match*/
      size = inputIndex - beginIndex;

      if (outputIndex + replacementLenght > outputLength)
      {
         int oldLength = outputLength;
         outputLength+=(increment+replacementLenght);
         outputString = (char*) realloc(outputString,(outputLength+1)*sizeof(char));
         if (NULL == outputString)
         {
            message(stderr,__FILE__,__LINE__,"ERROR","replaceStringlets","couldn't allocate memory\n");
            deleteStack(&objects);
            deleteStack(&persistentObjects);
            return -1;
         }
         pushStack(persistentObjects, outputString, outputLength);
         memset(&outputString[oldLength],0,outputLength-oldLength+1);
      }

      if (0 == outputIndex)
      {
         strncpy(outputString,replacement, replacementLenght+1);
      }
      else
      {
         strncat(outputString,replacement, replacementLenght+1);
      }
      outputIndex += replacementLenght;

      /* continue next search at the end of token */
      inputIndex+=tokenLength;
   }

   /* readjustment of output string  */
   outputString = (char*) realloc(outputString,(outputIndex+1)*sizeof(char));
   if (NULL == outputString)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","replaceStringlets","couldn't allocate memory\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   pushStack(persistentObjects, outputString, outputLength);

   /* copy pointer */
   *output = outputString;

   /* clean */
   deleteStack(&objects);
   deleteStackOnly(&persistentObjects);

   return strlen(*output);
}

int removeToken(char* input, char* token, char tokenProtection)
{
#ifdef DEBUG
   message(stdout,__FILE__,__LINE__,"DEBUG","removeToken","begin\n");
#endif

   int i=0;
   int size_of_inp = strlen(input)+1;
   int found_elements = 0;
   char *part = NULL;
   char *local = NULL;
   char  **spaceArray;
   int length=0;
   int totallength=0;

   local = (char *) malloc (size_of_inp*sizeof (char));
   if (!local)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","removeToken", "couldn't allocate memory.\n");
      return -1;
   }

   strncpy(local, input, size_of_inp);

   spaceArray = NULL;

   found_elements = divideUpStrings (local, &spaceArray, token, size_of_inp, tokenProtection);

   /* error condition of divideUpStrings */
   if (found_elements < 0)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","removeToken",
              "divideUpStrings() failed.\n");
      fflush(stderr);
      return -1;
   }

   safeStringFree(&local, size_of_inp);

   for (i = 0; i < found_elements; ++i)
   {
      if (NULL != spaceArray[i])
      {
         totallength+=strlen(spaceArray[i]);
      }
   }

   local = (char *) malloc ((totallength+1)*sizeof (char));
   if (!local)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","removeToken", "couldn't allocate memory.\n");
      return -1;
   }
   /* clear string */
   memset(local,0,totallength+1);
   strncpy(local,"",totallength+1); /* fill up with \0 */

   for (i = 0; i < found_elements; ++i)
   {
      length=strlen(spaceArray[i])+1;
      part = (char *) malloc(length * sizeof (char));
      if (!part)
      {
         message(stderr,__FILE__,__LINE__,"ERROR","removeToken", "couldn't allocate memory.\n");
         return -1;
      }
      if (i==0)
      {
         strncpy(local,spaceArray[i],length);
      }
      else
      {
         snprintf(part,length,"%s",&spaceArray[i][0]);
         strncat(local,part,length);
      }
      safeStringFree(&spaceArray[i], length);
      safeStringFree(&part, length);
   }
   safePArrayFree((void**)spaceArray,found_elements);

   strncpy(input, local, size_of_inp);

   strncpy(local,"", totallength+1);
   safeStringFree(&local, totallength+1);
#ifdef DEBUG

   message(stdout,__FILE__,__LINE__,"DEBUG","removeToken","end\n");
#endif

   return totallength;
}

int divideUpStrings (char *stringinp, char **argumentArray[], char *token, int maxArgs, char tokenProtection)
{
   /* divideUpStrings
   *
   * divide string stringinp containing separators token into stringlets stored in array argumentArray
   * if tokenProtection > 0: 
   * 	token can be protected, i.e. ignored, if tokenProtection is in front of it
   * 	combinations of 'tokenProtection+token' are replaced by 'token' in the stringlets  
   * 
   * returns
   * 	if error occur           : -1 (i.e. length of stringinp < 1, argArray or token == NULL)
   *    if maxArgs > 0           : number of found elements, but maximal maxArgs
   * 	number of found elements : else
   */


#ifdef DEBUG
   message(stdout,__FILE__,__LINE__,"DEBUG","divideUpStrings","begin \n");
#endif

   stack *objects = NULL;
   if (NULL == createStack(&objects))
   {
      message(stdout,__FILE__,__LINE__,"ERROR","divideUpStrings","createStack failed\n");
      return -1;
   }
   stack *persistentObjects = NULL;
   if (NULL == createStack(&persistentObjects))
   {
      message(stdout,__FILE__,__LINE__,"ERROR","divideUpStrings","createStack failed\n");
      deleteStack(&objects);
      return -1;
   }

   /* check and assert input values */
   if (NULL == stringinp)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","input string == NULL\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   assert(NULL != stringinp);
   int stringinpLength = stringLength(stringinp);
   if (0 == stringinpLength)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","input string empty\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   if (-1 > stringinpLength)
   {
      message(stdout,__FILE__,__LINE__,"ERROR","divideUpStrings",
              "input arg1 exceeds maximum size of %i characters\n",-stringinpLength);
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }

   if (NULL == token)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","token string == NULL\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   assert(NULL != token);
   int tokenLength = stringLength(token);
   if (0 == tokenLength)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","token string empty\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   if (-1 > tokenLength)
   {
      message(stdout,__FILE__,__LINE__,"ERROR","divideUpStrings",
              "token arg2 exceeds maximum size of %i characters\n",-tokenLength);
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }

   if (0 >= maxArgs)
   {
      maxArgs=INT_MAX;
   }

   /* local copies */
   char   *string =
      (char *) malloc (sizeof (char) * stringinpLength);
   if (NULL == string)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","couldn't allocate memory\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   memset(string,0, sizeof (char) * stringinpLength);
   strncpy (&string[0], &stringinp[0], stringinpLength);
   pushStack(objects, string, stringinpLength);

   int increment = 0;
   char ** argArray = NULL;
   int     found_elements = 0;

   increment = (int) (0.1 * stringinpLength);
   if (increment<5)
   {
      increment=5;
   }

   /* disassemble */
   int inputLength = stringinpLength;
/*   int tokenLength = tokenLength;*/
   int stringIndex = 0;
   int beginIndex = 0;
   char *replacement = malloc(((tokenLength+1) +1)*sizeof(char));
   /* failure ? */
   if (!replacement)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","couldn't allocate memory\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   memset(replacement,0,((tokenLength+1) +1));
   replacement[0] = tokenProtection;
   strncat(replacement, token, tokenLength);
   pushStack(objects, replacement, strlen(replacement));

   found_elements=0;
   while(stringIndex <= inputLength)
   {
      /* no space left for trailing token ? */
      if (stringIndex + tokenLength > inputLength)
      {
         break;
      }

      /* match ? */
      if (0 != strncmp(&string[stringIndex],token,tokenLength))
      {
         stringIndex++;
         continue;
      }

      /* tokenProtection */
      if ( 0 < tokenProtection)
      {
         /* token protected by tokenProtection ? */
         if (0 < stringIndex && tokenProtection == string[stringIndex-1])
         {
            stringIndex+=tokenLength;
            stringIndex++;
            continue;
         }
      }
      /* passed all comparisons - token found a match*/

      /* string between beginning or last token is larger than 0 */
      int size = stringIndex - beginIndex;
      if (0 < size)
      {
         /* (re)allocate pointer array for "increment" new pointers */
         if (argArray==NULL || found_elements%increment == 0)
         {
            argArray = (char**) realloc(argArray,sizeof(char*) * (found_elements+increment) );
            /* failure ? */
            if (!argArray)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","couldn't allocate memory\n");
               deleteStack(&objects);
               deleteStack(&persistentObjects);
               return -1;
            }
            pushStack(persistentObjects, argArray, 0);
            memset(&argArray[found_elements],0,sizeof(char**)*increment);
         }

         /* if tokenProtection active, replace 'tokenProtection+token' by 'token' */

         if (0 < tokenProtection)
         {
            char *input = NULL;
            argArray[found_elements] = NULL;

            /* allocate char array */
            input = (char*) malloc((size +1)*sizeof(char));
            /* failure ? */
            if (NULL == input)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","couldn't allocate memory\n");
               deleteStack(&objects);
               deleteStack(&persistentObjects);
               return -1;
            }
            /* clear array*/
            memset(input,0,size+1);
            /* copy value */
            strncpy(input, &string[beginIndex], size);

            /* replace stringlets */
            size = replaceStringlets(input, &(argArray[found_elements]), replacement, token);

            /* failure ? */
            if (0 > size)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","replaceStringlets() failed\n");
               deleteStack(&objects);
               deleteStack(&persistentObjects);
               return -1;
            }
            pushStack(persistentObjects, argArray[found_elements], (size +1)*sizeof(char));
            safeStringFree(&input,strlen(input));
         }
         else
         {
            /* allocate char array */
            argArray[found_elements] = (char*) malloc((size +1)*sizeof(char));
            /* failure ? */
            if (NULL == argArray[found_elements])
            {
               message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","couldn't allocate memory\n");
               deleteStack(&objects);
               deleteStack(&persistentObjects);
               return -1;
            }
            /* clear array*/
            memset(argArray[found_elements],0,size+1);
            /* copy value */
            strncpy (&argArray[found_elements][0], &string[beginIndex], size);
            pushStack(persistentObjects, argArray[found_elements], (size +1)*sizeof(char));
         }
         /* increase found_elements counter */
         found_elements++;
      }
      /* set the start of the next string to end of the last token */
      beginIndex=stringIndex+tokenLength;
      /* continue next search at the end of token */
      stringIndex+=tokenLength;

      if (found_elements == maxArgs)
      {
         break;
      }
   }
   /* remainder */
   int size = stringIndex - beginIndex;
   if (size > 0)
   {
      /* (re)allocate pointer array for "increment" new pointers */
      if (argArray==NULL || found_elements%increment == 0)
      {
         argArray = (char**) realloc(argArray,sizeof(char*) * (found_elements+1) );
         /* failure ? */
         if (!argArray)
         {
            message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","couldn't allocate memory\n");
            deleteStack(&objects);
            deleteStack(&persistentObjects);
            return -1;
         }
         pushStack(persistentObjects, argArray, 0);
      }
      /* allocate char array */
      argArray[found_elements] = (char*) malloc((size +1)*sizeof(char));
      /* failure ? */
      if (NULL == argArray[found_elements])
      {
         message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","couldn't allocate memory\n");
         deleteStack(&objects);
         deleteStack(&persistentObjects);
         return -1;
      }
      /* clear array*/
      memset(argArray[found_elements],0,size+1);
      /* copy value */
      strncpy (&argArray[found_elements][0], &string[beginIndex], size);
      /* increase found_elements counter */
      pushStack(persistentObjects, argArray[found_elements], (size +1)*sizeof(char));

      found_elements++;
   }

   /* re-adjust to final size*/
   argArray = (char**) realloc(argArray,sizeof(char*) * (found_elements) );
   if (!argArray)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","divideUpStrings","couldn't allocate memory\n");
      deleteStack(&objects);
      deleteStack(&persistentObjects);
      return -1;
   }
   pushStack(persistentObjects, argArray, 0);

#ifdef DEBUG

   {
      int i = 0;
      message(stdout,__FILE__,__LINE__,"DEBUG","divideUpStrings","token: `%s' - found %i elements\n", token, found_elements);
      if (DEBUG > 1)
         for (i = 0; i < found_elements; i++)
         {
            printf ("      `%s'\n", argArray[i]);
         }
      message(stdout,__FILE__,__LINE__,"DEBUG","divideUpStrings","end\n");
   }
#endif

   /* copy local array to calling array*/
   *argumentArray = argArray;

   deleteStack(&objects);
   deleteStackOnly(&persistentObjects);
   return found_elements;
}

int stringLength (char *pstring)
{
   /* returns the length of a string the pointer pstring is pointer to  */
   /* requires that it is a string, i.e. it is terminated by '\0'       */
   /* return values                                                     */
   /*                       -1: if pstring is NULL                      */
   /*   -MAXIMUM_STRING_LENGTH: if length exceeds MAXIMUM_STRING_LENGTH */
   /* else: the length of the string including '\0'                     */

#ifndef MAXIMUM_STRING_LENGTH
#define MAXIMUM_STRING_LENGTH INT_MAX
#endif

   int length = 0;
   if (pstring)
   {
      while (pstring[length] != '\0')
      {
         /*printf("%c-",pstring[length]); */
         length++;
         if (length == MAXIMUM_STRING_LENGTH)
         {
            return -MAXIMUM_STRING_LENGTH;
         }
      }
      return ++length;
   }
   else
   {
      return -1;
   }
}

void safeStringFree(char **inp, size_t size_of_inp)
{
   if (NULL != inp)
   {
      if ( NULL != *inp)
      {
         if (0 < size_of_inp)
         {
            memset((*inp),'\0',size_of_inp);
            SAFE_FREE (*inp);
         }
      }
   }
}

void safePArrayFree(void **inp, size_t size_of_inp)
{
   if (inp)
   {
      SAFE_FREE (inp);
   }
}

int combineString(char *list[], char** output, size_t elements)
{
   /* combines all strings given in list into output strings
    * return values:
    *               -1 : error
    * strlen of output : else
    */

   /* check input */
   if ( NULL == list )
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "combineString", "arg 1 = NULL\n");
      return -1;
   }
   if ( NULL == output )
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "combineString", "arg 1 = NULL\n");
      return -1;
   }

   char* allString = NULL;
   int length = 0;
   int i = 0;
   for (i = 0; i < elements; i++)
   {
      if( list[i] )
      {
         {
            length += strlen(list[i]);
         }
      }
   }
   /*  allocate combined string */
   allString = (char*) malloc ( (length+1) * sizeof(char) );
   if (NULL == allString)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "combineString", "couldn't allocate memory\n");
      return -1;
   }
   /* clear */
   memset(allString,0, length+1);
   for (i = 0; i < elements; i++)
   {
      if( NULL != list[i])
      {
         if (0 == allString[0])
         {
            strncpy(allString, list[i], strlen(list[i]));
         }
         else
         {
            strncat(allString, list[i], strlen(list[i]));
         }
      }
   }
   *output = allString;
   return length;
}

/* stack implementation */

struct stack* createStack(struct stack **pStack)
{
   if (NULL != *pStack)
   {
      clearStack(*pStack);
   }
   *pStack = (struct stack*) malloc(sizeof(stack));
   if (NULL == *pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "createStack", "couldn't allocate memory\n");
      return NULL;
   }
   if (false == initStack(*pStack))
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "initStack", "clearStack failed\n");
      return false;
   }
   return *pStack;
}

bool dumpStack(FILE *stream, struct stack* pStack)
{
   /* dumps the adresses of the contents and next/prev pointers*/
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "dumpStack", "argument is NULL\n");
      return false;
   }
   stackNode* pointer = (stackNode*) (pStack->top);
   int i=0;
   while(pointer)
   {
      char *data = (char*) pointer->data;
      message(stream, 0, 0, "DUMP", "dumpStack", "printing level %3i: data %p *data: `%s' size: %i next: %p prev: %p \n",
              i,
              data,
              (data)?"something":"(nil)",
              pointer->size, pointer->next, pointer->prev);
      pointer = (stackNode*) pointer->next;
      i--;
   }
   message(stream, 0, 0, "DUMP", "dumpStack","---------------------------------------------------\n");
   return true;
}

bool dumpStackString(FILE *stream, struct stack* pStack)
{
   /* dumps the adresses of the contents and next/prev pointers*/
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "dumpStack", "argument is NULL\n");
      return false;
   }
   stackNode* pointer = (stackNode*) (pStack->top);
   int i=0;
   while(pointer)
   {
      char *data = (char*) pointer->data;
      message(stream, 0, 0, "DUMP",  "dumpStackString", "level %3i: `%s'\n", i, data);
      pointer = (stackNode*) pointer->next;
      i--;
   }
   message(stream, 0, 0, "DUMP", "dumpStackString","---------------------------------------------------\n");
   return true;
}

bool clearStack(struct stack* pStack)
{
   /* clears the stack and node structures AND frees the content they are pointing at*/
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "clearStack", "argument is NULL\n");
      return false;
   }

   stackNode *node = NULL;
   while(0 < pStack->size)
   {
      node = pStack->top;
      if (NULL != node)
      {
         if (0 < node->size && NULL != node->data)
         {
            memset(node->data,0,node->size);
         }
         if (NULL != node->data)
         {
            free(node->data);
            node->data=NULL;
         }
      }
      popStack(pStack);
   }
   return true;
}

bool clearStackOnly(struct stack* pStack)
{
   /* only clears the stack and node structures but does not free the content they are pointing at*/
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "clearStack", "argument is NULL\n");
      return false;
   }
   while(0 < pStack->size)
   {
      popStack(pStack);
   }
   return true;
}

bool initStack(struct stack* pStack)
{
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "initStack", "argument is NULL\n");
      return false;
   }
   pStack->top = NULL;
   pStack->bottom = NULL;
   pStack->size = 0;
   return true;
}

bool deleteStack(struct stack** pStack)
{
   /* deletes the stack and node structures AND does free the content they are pointing at*/
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "deleteStack", "argument is NULL\n");
      return false;
   }

   if (NULL != *pStack)
   {
      if (false == clearStack(*pStack))
      {
         message(stderr, __FILE__, __LINE__, "ERROR", "deleteStack", "clearStack failed\n");
         return false;
      }
   }
   SAFE_FREE(*pStack);
   return true;
}

bool deleteStackOnly(struct stack** pStack)
{
   /* only deletes the stack and node structures but does not free the content they are pointing at*/
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "deleteStack", "argument is NULL\n");
      return false;
   }

   if (false == clearStackOnly(*pStack))
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "deleteStack", "clearStack failed\n");
      return false;
   }
   SAFE_FREE(*pStack);
   return true;
}

bool pushStack(struct stack* pStack, void* element, size_t size)
{
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "pushStack", "pointer to stack (arg1) is really NULL\n");
      return false;
   }
   if (NULL == element)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "pushStack", "element (arg2) is NULL\n");
      return false;
   }

   stackNode* newnode = (struct stackNode*) malloc(sizeof(stackNode));
   if (NULL == newnode)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "pushStack", "couldn't allocate memory\n");
      return false;
   }

   newnode->data = element;
   newnode->size = size;
   newnode->prev = NULL;

   /* stack empty */
   if (NULL == pStack->bottom)
   {
      pStack ->bottom = newnode;
      pStack ->top    = newnode;
      newnode->next   = NULL;
   }
   else
   {
      newnode->next = pStack->top;
      pStack ->top  = newnode;
      newnode->next->prev = newnode;
   }
   pStack->size++;

   return true;
}

bool insertStack(struct stack *pStack, void* element, size_t size, int (*compare)(void*,void*))
{
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "insertStack", "pointer to stack (arg1) is NULL\n");
      return false;
   }

   if (NULL == element)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "insertStack", "element (arg2) is NULL\n");
      return false;
   }

   if (NULL == compare)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "insertStack", "compare fcn not given, it is NULL\n");
      return false;
   }

   stackNode* newnode = (struct stackNode*) malloc(sizeof(stackNode));
   if (NULL == newnode)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "insertStack", "couldn't allocate memory\n");
      return false;
   }

   newnode->data = element;
   newnode->size = size;

   newnode->prev = NULL;

   /* stack empty */
   if (NULL == pStack->bottom)
   {
      free(newnode);
      return pushStack(pStack, element, size);
   }
   /* element is smaller than all the others (smallest on top) */
   else if (0 < compare(element, pStack->top->data) )
   {
      free(newnode);
      return pushStack(pStack, element, size);
   }
   /* element is larger than all the others (largest on bottom) */
   else if (0 > compare(element, pStack->bottom->data) )
   {
      pStack->bottom->next = newnode;
      newnode->prev = pStack->bottom;
      newnode->next = NULL;
      pStack->bottom = newnode;
   }
   /* top element is the same refering to the compare function, free and exit */
   else if (0 == compare(element, pStack->top->data))
   {
      free(newnode);
      message(stderr, __FILE__, __LINE__, "ERROR", "insertStack", "element already contained refering to compare\n");
      return false;
   }
   else
   {
      struct stackNode *current = pStack->top;
      while(0 > compare(element, current->next->data))
      {
         current = current->next;
      }
      if (0 == compare(element, current->next->data))
      {
         free(newnode);
         message(stderr, __FILE__, __LINE__, "ERROR", "insertStack", "element already contained refering to compare\n");
         return false;
      }

      current->next->prev = newnode;
      newnode->next=current->next;
      newnode->prev=current;
      current->next=newnode;
   }
   pStack->size++;

   return true;
}

void* popStack(struct stack* pStack)
{
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "popStack", "pointer to stack (arg1) is NULL\n");
      return NULL;
   }

   void* topNodeElement = NULL;
   stackNode *nextNode = NULL;
   stackNode *topNode = NULL;

   if (0 == pStack->size)
   {
      /*message(stderr, __FILE__, __LINE__, "WARNING", "popStack", "stack is empty\n");*/
      return NULL;
   }

   topNode = pStack->top;

   if (NULL == topNode)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "popStack", "Though size is not 0, top element is NULL\n");
      return NULL;
   }

   topNodeElement  = topNode->data;
   nextNode        = topNode->next;

   memset(topNode,0,sizeof(stackNode));
   SAFE_FREE(topNode);

   pStack->top = (stackNode*) nextNode;

   if (NULL == nextNode)
   {
      pStack->bottom = NULL;
   }
   else
   {
      nextNode->prev = NULL;
   }

   pStack->size--;

   return topNodeElement;
}

void* findInStack(struct stack* pStack, void* element, int (*compare)(void*,void*))
{
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInStack", "pointer to stack (arg1) is NULL\n");
      return false;
   }

   if (NULL == element)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInStack", "element (arg2) is NULL\n");
      return false;
   }

   if (NULL == compare)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInStack", "compare fcn not given, it is NULL\n");
      return false;
   }

   if (NULL == pStack->bottom)
   {
      return NULL;
   }

   stackNode *current = pStack->top;
   while( NULL != current)
   {
      if ( 0 == compare(element, current->data) )
      {
         return current->data;
      }
      current=current->next;
   }
   return NULL;
}

stackNode* findStackNode(struct stack* pStack, void* element, int (*compare)(void*,void*))
{
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInStack", "pointer to stack (arg1) is NULL\n");
      return false;
   }

   if (NULL == element)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInStack", "element (arg2) is NULL\n");
      return false;
   }

   if (NULL == compare)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInStack", "compare fcn not given, it is NULL\n");
      return false;
   }

   if (NULL == pStack->bottom)
   {
      return NULL;
   }

   stackNode *current = pStack->top;
   while( NULL != current)
   {
      if ( 0 == compare(element, current->data) )
      {
         return current;
      }
      current=current->next;
   }
   return NULL;
}

bool removeFromStack(struct stack* pStack, void* element, int (*compare)(void*,void*))
{
   if (NULL == pStack)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInStack", "pointer to stack (arg1) is NULL\n");
      return false;
   }

   if (NULL == element)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInStack", "element (arg2) is NULL\n");
      return false;
   }

   if (NULL == compare)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInStack", "compare fcn not given, it is NULL\n");
      return false;
   }

   /* stack empty */
   if (0 == pStack->size)
   {
      return false;
   }


   stackNode* current = NULL;

   if ( 0 == compare( element, pStack->top->data ))
   {
      current = pStack->top;
      if (NULL != current->next)
      {
         current->next->prev = current->prev;
      }
      if (1 == pStack->size)
      {
         pStack->bottom = current->next;
      }
      pStack->top = current->next;
      free(current);
      pStack->size--;
      return true;
   }
   else if ( 0 == compare ( element, pStack->bottom->data))
   {
      current = pStack->bottom;
      if (NULL != current->prev)
      {
         current->prev->next = current->next;
      }
      if (1 == pStack->size)
      {
         pStack->top = current->prev;
      }
      pStack->bottom = current->prev;
      free(current);
      pStack->size--;
      return true;
   }
   else
   {
      current = findStackNode(pStack, element, compare);
      if (NULL == current)
      {
         return false;
      }
      if (NULL != current)
      {
         current->prev->next = current->next;
         current->next->prev = current->prev;
         free(current);
         pStack->size--;
         return true;
      }
   }
   return false;
}

/* double linked list implementation */

#ifdef INCLUDELIST
struct list* createList(struct list **pList)
{
   if (NULL != *pList)
   {
      clearList(*pList);
   }
   *pList = (struct list*) malloc(sizeof(list));
   if (NULL == *pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "createList", "couldn't allocate memory\n");
      return NULL;
   }
   if (false == initList(*pList))
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "initList", "clearList failed\n");
      return false;
   }
   return *pList;
}

bool dumpList(FILE *stream, struct list* pList)
{
   /* dumps the adresses of the contents and next/prev pointers*/
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "dumpList", "argument is NULL\n");
      return false;
   }
   listNode* pointer = (listNode*) (pList->top);
   int i=0;
   while(pointer)
   {
      char *data = (char*) pointer->data;
      message(stream, 0, 0, "DUMP",  "dumpList", "printing level %3i: data %p *data: `%s' size: %i next: %p prev: %p \n",
              i,
              data,
              (data)?"something":"(nil)",
              pointer->size, pointer->next, pointer->prev);
      pointer = (listNode*) pointer->next;
      i++;
   }
   message(stream, 0, 0, "DUMP",  "dumpList","---------------------------------------------------\n");
   return true;
}

bool dumpListString(FILE *stream, struct list* pList)
{
   /* dumps the adresses of the contents and next/prev pointers*/
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "dumpList", "argument is NULL\n");
      return false;
   }
   listNode* pointer = (listNode*) (pList->top);
   int i=0;
   while(pointer)
   {
      char *data = (char*) pointer->data;
      message(stream, 0, 0, "DUMP", 0, "level %3i: `%s'\n", i, data);
      pointer = (listNode*) pointer->next;
      i++;
   }
   message(stream, 0, 0, "DUMP", 0,"---------------------------------------------------\n");
   return true;
}

bool clearList(struct list* pList)
{
   /* clears the list and node structures AND frees the content they are pointing at*/
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "clearList", "argument is NULL\n");
      return false;
   }

   listNode *node = NULL;
   while(0 < pList->size)
   {
      node = pList->top;
      if (NULL != node)
      {
         if (0 < node->size && NULL != node->data)
         {
            memset(node->data,0,node->size);
         }
         if (NULL != node->data)
         {
            free(node->data);
            node->data=NULL;
         }
      }
      popList(pList);
   }
   return true;
}

bool clearListOnly(struct list* pList)
{
   /* only clears the list and node structures but does not free the content they are pointing at*/
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "clearList", "argument is NULL\n");
      return false;
   }
   while(0 < pList->size)
   {
      popList(pList);
   }
   return true;
}

bool initList(struct list* pList)
{
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "initList", "argument is NULL\n");
      return false;
   }
   pList->top = NULL;
   pList->bottom = NULL;
   pList->size = 0;
   return true;
}

bool deleteList(struct list** pList)
{
   /* deletes the list and node structures AND does free the content they are pointing at*/
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "deleteList", "argument is NULL\n");
      return false;
   }

   if (false == clearList(*pList))
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "deleteList", "clearList failed\n");
      return false;
   }
   SAFE_FREE(*pList);
   return true;
}

bool deleteListOnly(struct list** pList)
{
   /* only deletes the list and node structures but does not free the content they are pointing at*/
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "deleteList", "argument is NULL\n");
      return false;
   }

   if (false == clearListOnly(*pList))
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "deleteList", "clearList failed\n");
      return false;
   }
   SAFE_FREE(*pList);
   return true;
}

bool appendList(struct list* pList, void* element, size_t size)
{
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "pushList", "pointer to list (arg1) is really NULL\n");
      return false;
   }
   if (NULL == element)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "pushList", "element (arg2) is NULL\n");
      return false;
   }

   listNode* newnode = (struct listNode*) malloc(sizeof(listNode));
   if (NULL == newnode)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "pushList", "couldn't allocate memory\n");
      return false;
   }

   newnode->data = element;
   newnode->size = size;
   newnode->next = NULL;

   /* list empty */
   if (NULL == pList->bottom)
   {
      pList ->bottom = newnode;
      pList ->top    = newnode;
      newnode->prev  = NULL;
   }
   else
   {
      newnode->prev       = pList->bottom;
      pList ->bottom      = newnode;
      newnode->prev->next = newnode;
   }
   pList->size++;

   return true;
}

void* popList(struct list* pList)
{
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "popList", "pointer to list (arg1) is NULL\n");
      return NULL;
   }

   void* bottomNodeElement = NULL;
   listNode *prevNode = NULL;
   listNode *bottomNode = NULL;

   if (0 == pList->size)
   {
      message(stderr, __FILE__, __LINE__, "WARNING", "popList", "list is empty\n");
      return NULL;
   }

   bottomNode = pList->bottom;

   if (NULL == bottomNode)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "popList", "Though size is not 0, bottom element is NULL\n");
      return NULL;
   }

   bottomNodeElement  = bottomNode->data;
   prevNode           = bottomNode->prev;

   memset(bottomNode,0,sizeof(listNode));
   SAFE_FREE(bottomNode);

   pList->bottom = (listNode*) prevNode;

   if (NULL == prevNode)
   {
      pList->bottom = NULL;
   }
   else
   {
      prevNode->next = NULL;
   }

   pList->size--;

   return bottomNodeElement;
}

bool insertList(struct list *pList, void* element, size_t size, int (*compare)(void*,void*))
{
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "insertList", "pointer to list (arg1) is NULL\n");
      return false;
   }

   if (NULL == element)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "insertList", "element (arg2) is NULL\n");
      return false;
   }

   if (NULL == compare)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "insertList", "compare fcn not given, it is NULL\n");
      return false;
   }

   listNode* newnode = (struct listNode*) malloc(sizeof(listNode));
   if (NULL == newnode)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "insertList", "couldn't allocate memory\n");
      return false;
   }

   newnode->data = element;
   newnode->size = size;

   newnode->prev = NULL;

   /* list empty */
   if (NULL == pList->top)
   {
      free(newnode);
      return appendList(pList, element, size);
   }
   /* element is larger than all the others (smallest on top) */
   else if (0 < compare(element, pList->bottom->data) )
   {
      free(newnode);
      return appendList(pList, element, size);
   }
   /* element is smaller than all the others (largest on bottom) */
   else if (0 > compare(element, pList->top->data) )
   {
      pList->top->prev = newnode;
      newnode->prev = NULL;
      newnode->next = pList->top;
      pList->top = newnode;
   }
   /* top element is the same refering to the compare function, free and exit */
   else if (0 == compare(element, pList->top->data))
   {
      free(newnode);
      message(stderr, __FILE__, __LINE__, "ERROR", "insertList", "element already contained refering to compare\n");
      return false;
   }
   else
   {
      struct listNode *current = pList->top;
      while(0 < compare(element, current->next->data))
      {
         current = current->next;
      }
      if (0 == compare(element, current->next->data))
      {
         free(newnode);
         message(stderr, __FILE__, __LINE__, "ERROR", "insertList", "element already contained refering to compare\n");
         return false;
      }

      current->next->prev = newnode;
      newnode->next=current->next;
      newnode->prev=current;
      current->next=newnode;
   }
   pList->size++;

   return true;
}

void* findInList(struct list* pList, void* element, int (*compare)(void*,void*))
{
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInList", "pointer to list (arg1) is NULL\n");
      return false;
   }

   if (NULL == element)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInList", "element (arg2) is NULL\n");
      return false;
   }

   if (NULL == compare)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInList", "compare fcn not given, it is NULL\n");
      return false;
   }

   if (NULL == pList->bottom)
   {
      return NULL;
   }

   listNode *current = pList->top;
   while( NULL != current)
   {
      if ( 0 == compare(element, current->data) )
      {
         return current->data;
      }
      current=current->next;
   }
   return NULL;
}

listNode* findListNode(struct list* pList, void* element, int (*compare)(void*,void*))
{
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInList", "pointer to list (arg1) is NULL\n");
      return false;
   }

   if (NULL == element)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInList", "element (arg2) is NULL\n");
      return false;
   }

   if (NULL == compare)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInList", "compare fcn not given, it is NULL\n");
      return false;
   }

   if (NULL == pList->bottom)
   {
      return NULL;
   }

   listNode *current = pList->top;
   while( NULL != current)
   {
      if ( 0 == compare(element, current->data) )
      {
         return current;
      }
      current=current->next;
   }
   return NULL;
}

bool removeFromList(struct list* pList, void* element, int (*compare)(void*,void*))
{
   if (NULL == pList)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInList", "pointer to list (arg1) is NULL\n");
      return false;
   }

   if (NULL == element)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInList", "element (arg2) is NULL\n");
      return false;
   }

   if (NULL == compare)
   {
      message(stderr, __FILE__, __LINE__, "ERROR", "findInList", "compare fcn not given, it is NULL\n");
      return false;
   }

   /* list empty */
   if (0 == pList->size)
   {
      return false;
   }

   listNode* current = NULL;

   if ( 0 == compare( element, pList->top->data ))
   {
      current = pList->top;
      if (NULL != current->next)
      {
         current->next->prev = current->prev;
      }
      if (1 == pList->size)
      {
         pList->bottom = current->next;
      }
      pList->top = current->next;
      free(current);
      pList->size--;
      return true;
   }
   else if ( 0 == compare ( element, pList->bottom->data))
   {
      current = pList->bottom;
      if (NULL != current->prev)
      {
         current->prev->next = current->next;
      }
      if (1 == pList->size)
      {
         pList->top = current->prev;
      }
      pList->bottom = current->prev;
      free(current);
      pList->size--;
      return true;
   }
   else
   {
      current = findListNode(pList, element, compare);
      if (NULL == current)
      {
         return false;
      }
      if (NULL != current)
      {
         current->prev->next = current->next;
         current->next->prev = current->prev;
         free(current);
         pList->size--;
         return true;
      }
   }
   return false;
}
#endif
