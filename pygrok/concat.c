#include "concat.h"
#include <stdlib.h> 
#include <string.h> 

char* concat(char *S1, char *S2)
{
     int lengthOfStr1 = strlen(S1);
     int lengthOfStr2 = strlen(S2);
     char *result = (char*)malloc(lengthOfStr1 + lengthOfStr2 +1);
     strcpy(result, S1);
     strcat(result,S2);

     return result;
}
