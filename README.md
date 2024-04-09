## W: Uma Linguagem Moderna e Segura Inspirada em C

**W** é uma nova linguagem de programação projetada para ser um superconjunto seguro e expressivo de C, com foco em modularidade, gerenciamento de memória eficiente e programação concorrente. Inspirada em linguagens como TypeScript e Swift, W oferece uma sintaxe moderna e familiar, enquanto mantém a capacidade de interoperar com C de forma direta e eficiente.

### Principais Características

* **Sintaxe Simplificada:** W oferece recursos como destruturação de argumentos e variáveis, operadores de coalescência nula e opcionais para uma escrita de código mais concisa e expressiva.
* **Sistema de Tipos Avançado:** O sistema de tipos de W é estático com inferência de tipos, permitindo flexibilidade sem comprometer a segurança. Tipos com restrições como `String<maxLength: 22>` garantem segurança de memória em tempo de compilação.
* **Gerenciamento de Memória:** W utiliza um sistema onde o chamador é responsável pela alocação de memória, permitindo um controle preciso e evitando vazamentos de memória. A linguagem oferece tipos como `ref`, `storage`, `transfer` e `cow` para gerenciar a propriedade e o ciclo de vida dos dados.
* **Concorrência Estruturada:** W suporta programação concorrente através de corrotinas, inspiradas em Protothreads e libdill. Recursos como `async` e `await` permitem escrever código concorrente de forma clara e concisa.
* **Interoperabilidade com C:** W foi projetada para interoperar perfeitamente com C, permitindo que os desenvolvedores utilizem bibliotecas C existentes e escrevam código C quando necessário. 
* **Modularização:** W suporta módulos como unidades de organização e encapsulamento de código. Os módulos são singletons e possuem controle de memória e paralelismo independentes.
* **Ferramentas de Desenvolvimento:**  W visa oferecer um conjunto de ferramentas de desenvolvimento robustas, incluindo um Linter, IDE e um gerenciador de pacotes.

### Exemplos de Código

```typescript
// Função com destruturação de argumentos
fn({ a: string, b: number }) {
  return 0;
}

// Tipo com restrição de tamanho máximo
type CPF = String<maxLength: 12, mask: CPF, inputType: Number>;

// Função assíncrona com corrotina
async fetchUserData(userId: string) throws -> UserData {
  let response = await http.get(`https://api.example.com/users/${userId}`);
  return await response.json();
}

// Módulo com exportações
module MyModule {
  export function greet(name: string) {
    print(`Hello, ${name}!`);
  }
}

// Uso do módulo
import { greet } from "MyModule";
greet("World");

fn someFunc(string a, int b) {
  let c = a + b; // concatenação de string e int
  
  if let d = someOptionalValue {
    print(d);
  }
  
  for i in 0..<10 {
    print(i);
  }
  
  async let result = fetch("https://example.com");
  print(await result);
}
```

### Status do Projeto
W está em fase inicial de desenvolvimento. O foco atual é no design da linguagem, na implementação do compilador e na construção de ferramentas de desenvolvimento.

### Próximos Passos

* Implementar o compilador W para WC (Clang extension).
* Desenvolver um sistema de módulos robusto.
* Implementar corrotinas e paralelismo.
* Criar ferramentas de desenvolvimento como Linter e plugin do VSCode.
* Definir a biblioteca padrão.

### Contribuindo

Contribuições são bem-vindas! Se você está interessado em ajudar a desenvolver W, por favor, entre em contato.

### Licença

W é um software de código aberto licenciado sob a licença MIT.
