```typescript

fn({ a: string, b: number}){
  
  return 0
}

const x = "x"

fn(args:SomeType){
  { a, b } = args
  return false
}

let count = 0
fn(SomeType){
  ${ a, b}
  @{ count, x };
  
  return { count+b, `${x}-${a}` }
}

type FType = { a: string, b: number} 

fn(FType){
  ${ a, b}
  @{ count, x };
  
  export count+b
  return `${x}-${a}`
}

fName(FType){
  ${ a, b}
  @{ count, x };
  
  return $[]
}

type FType2 = { a: string?, b = 0 } 

fn(FType2){
  ${ a ?? 'default', b==0 ? 2, c > 1 ?: 0 }
  return `${a}-${b}-${c}`
}

// FuncReturnType
// FuncArgs{ FuncInner, FuncGlobal }
int func(*FuncReturnType,FuncArgs){

}

fName(FType){
  ${ a, b}
  @{ count, x };
  
  export { count, x } to Context
  return a + b
}

fName(FType){
  ${ a, b}
  import let { w, x } from('coisa')
  
  export { count, x } to Context
  return a + b
}

//
int fn(int* a){
    *a=33;
    return 2;
}

int main()
{
    int* a = alloca(sizeof *a);
    int b = fn(a);
    printf("Hello World %d %d",b,*a);

    return 0;
}
//

import { coisa } from 'outraCoisa'
import lalala from 'lololo'
import * as lili from 'lulu'
include 'bababa' // semelhante include do C, usando bababa default namespace
include 'bababa' as nanana // mudando namespace
include ninini from 'hahaha' // vai incluir só ninini e derivados
import * from 'bababa' // same as [ include 'bababa' ]

//

```