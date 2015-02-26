/*
 * ptools.h
 *  programming tools
 * Author: p.zumbruch, p.zumbruch@gsi.de
 * created: 15.02.2007
 * last change: 27.02.2007
 */

#ifndef PTOOLS_H_
#define PTOOLS_H_

#ifndef SAFE_FREE
#define SAFE_FREE(pointer) if(pointer){free(pointer);pointer=NULL;}
#endif
/* free */
void    safeStringFree(char **inp, size_t size_of_inp);
void    safePArrayFree(void **inp, size_t size_of_inp);

/* strings and tokens*/

int     stringLength (char *pstring);
int     divideUpStrings (char *stringinp, char **argumentArray[], char *token, int maxArgs, char tokenProtection);
int     removeToken(char* input, char* token, char tokenProtection);
int     replaceStringlets(char* input, char **output, char* token, char* replacement);
int     combineString(char **list, char** output, size_t elements);

/* messages */
void    message(FILE *stream, const char *file, int line, const char* type, const char* fcn, const char* format, ...);

/* debugging */
void    backTrace(size_t level);

/*stack*/
typedef struct stackNode
{
   void* data;
   struct stackNode* next;
   struct stackNode* prev;
   size_t size;
}
stackNode;

typedef struct stack
{
   struct stackNode *top;
   struct stackNode *bottom;
   size_t size;
}
stack;

struct stack* createStack(struct stack **pStack);
bool clearStack(struct stack* pStack);
bool clearStackOnly(struct stack* pStack);
bool initStack(struct stack* pStack);
bool deleteStack(struct stack** pStack);
bool deleteStackOnly(struct stack** pStack);
bool pushStack(struct stack* pStack, void* element, size_t size);
void* popStack(struct stack* pStack);
bool insertStack(struct stack *pStack, void* element, size_t size, int (*compare)(void*,void*));
bool dumpStack(FILE *stream, struct stack* pStack);
bool dumpStackString(FILE *stream, struct stack* pStack);
bool removeFromStack(struct stack* pStack, void* element, int (*compare)(void*,void*));
void* findInStack(struct stack* pStack, void* element, int (*compare)(void*,void*));


/*list*/
#ifdef INCLUDELIST

typedef struct listNode
{
   void* data;
   struct listNode* next;
   struct listNode* prev;
   size_t size;
}
listNode;

typedef struct list
{
   struct listNode *top;
   struct listNode *bottom;
   size_t size;
}
list;

struct list* createList(struct list **pList);
bool clearList(struct list* pList);
bool clearListOnly(struct list* pList);
bool initList(struct list* pList);
bool deleteList(struct list** pList);
bool deleteListOnly(struct list** pList);
bool appendList(struct list* pList, void* element, size_t size);
bool insertList(struct list *pList, void* element, size_t size, int (*compare)(void*,void*));
bool dumpList(FILE *stream, struct list* pList);
bool removeFromList(struct list* pList, void* element, int (*compare)(void*,void*));
void* findInList(struct list* pList, void* element, int (*compare)(void*,void*));
void* popList(struct list* pList);
bool dumpListString(FILE *stream, struct list* pList);
#endif
#endif /*PTOOLS_H_*/
