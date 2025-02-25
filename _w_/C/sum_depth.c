#include <stdio.h>

long sum(long depth, long x){
    if (depth == 0) {
        return x;
    } else {
        long fst = sum(depth-1, x*2+0); // adds the fst half
        long snd = sum(depth-1, x*2+1); // adds the snd half
        return fst + snd;
    }
}

int main(){
    printf("%ld",sum(32, 0));
    return 0;
}
