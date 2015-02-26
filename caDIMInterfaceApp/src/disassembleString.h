#ifndef DISASSEMBLESTRING_H_
#define DISASSEMBLESTRING_H_

int     disassembleString(char input[], char*** keyArray, char*** argumentArray, char*** formatArray,
                          char primaryToken[],
                          char secondaryToken[],
                          char thirdToken[],
                          char spaceToken[],
                          char tokenProtection);
int     disassembleDIMFormatString(char format[], char ***typeArray, int **numberArray,
                                   char fourthToken[],
                                   char fifthToken[],
                                   char tokenProtection);
void    safeFreeOfDisassemblyStringArrays(int found_elements, char*** keyArray, char*** argumentArray, char*** formatArray);

#endif /*DISASSEMBLESTRING_H_ */
