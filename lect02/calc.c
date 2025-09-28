#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    int a, b;
    char op;
    int result;

    printf("calc");    
    scanf("%d %c %d",&a,&op,&b);

    switch (op) {
            case '+': result = a + b; break;
            case '-': result = a - b; break;
            case 'x': result = a * b; break;
            case '/':  result = a / b; break;   
        }
        printf("%d\n", result);
	
    return 0;
}

