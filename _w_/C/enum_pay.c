#include <stdio.h>
#include <stdlib.h>

const char countries[][3] = {
        [0] = "A1",
        [1] = "A0",
        [2] = "O1",
        [3] = "AD",
        [4] = "AE",
        [5] = "B1",
};

const char* get_country(size_t code){
    size_t total_countries = (sizeof(countries) / sizeof(countries[0]));
    return code < total_countries ? countries[code] : "UNKNOWN";
}

int main (int argc, char **argv)
{
    size_t cc = strtol(argv[1], NULL, 10);
    // int a, b, c;
    // *(a ? &b : &c) = 42; // curiosidade
    // fprintf(stdout, "%s %d%d%d\n",get_country(cc),a,b,c);
    fprintf(stdout, "%s\n",get_country(cc));
    return 0;
}