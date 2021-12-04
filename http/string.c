#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    // strpbrk
    char* target1 = "12345\t 12345";
    char* s1 = "13";
    char* res1 = strpbrk(target1,s1);
    printf("res1: %c\n",*res1);
    // strspn
    char* target2 = "baidu.aaaaaaaaacom";
    char* s2 = "hbaidu.";
    size_t res2 = strspn(target2,s2);
    printf("res2: %d\n",(int)res2);
    // strchr
    char* target3 = "abcdefg";
    char* res3 = strchr(target3,'b');
    printf("res3: %s\n",res3);
    return 0;
}
