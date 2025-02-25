#include <stdio.h>
#include <Block.h>

typedef int (^IntBlock)();

IntBlock MakeCounter(int start, int increment) {
	__block int i = start;
	
	return Block_copy( ^(void) {
		int ret = i;
		i += increment;
		return ret;
	});
	
}

int main(void) {
	IntBlock mycounter = MakeCounter(5, 2);
	printf("First call: %d\n", mycounter());
	printf("Second call: %d\n", mycounter());
	printf("Third call: %d\n", mycounter());
	
	/* because it was copied, it must also be released */
	Block_release(mycounter);
	
	return 0;
}

// Apesar de funcionar os blocks, compilei apenas usando  clang -O3 -std=gnu2x  W/C/block.c , provavelmente as nested functions do gcc e algum outro lance pra fazer o callback type é melhor. Não gosto do fato dos blocks capturem escopo.