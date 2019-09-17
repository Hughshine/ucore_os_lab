/* bugging.c*/
#include <stdio.h>
#include <stdlib.h>

static char buff [256];
static char* string;  // bug's here 
int main()
{
    printf("Please input a string");
    // gets(string);
    fgets(string, sizeof(string), stdin);
    printf("Your string is:%s\n", string);
}
