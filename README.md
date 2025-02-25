# Documentação Completa da Linguagem W e Ferramentas Relacionadas

## 1. Descrição Geral da Linguagem W

**W** é uma linguagem de programação moderna, segura e expressiva, projetada como um superconjunto de C. Inspirada por linguagens como TypeScript, Swift, Zig e Rust, W combina a performance e o controle de baixo nível do C com uma sintaxe mais amigável ao desenvolvedor e recursos de segurança integrados. W enfatiza modularidade, gerenciamento de memória eficiente e concorrência estruturada, tornando-a ideal para programação de sistemas, aplicações concorrentes e projetos que requerem acesso direto ao hardware.

### Objetivos Principais

- **Segurança**: Prevenir bugs comuns como estouros de buffer, dereferências de ponteiros nulos e erros de concorrência, utilizando verificações em tempo de compilação e execução.
- **Expressividade**: Sintaxe moderna e legível, com recursos como destruturação, operadores de coalescência nula, `async`/`await` e inferência de tipos.
- **Desempenho**: Manter a eficiência de C, com compilação para código de máquina otimizado via backend C.
- **Interoperabilidade**: Integração direta com código e bibliotecas C existentes, sem overhead significativo.
- **Concorrência**: Suporte integrado para programação concorrente estruturada, usando corrotinas e primitivas de paralelismo.
- **Modularidade**: Organização de código em módulos singleton com controle independente de memória e paralelismo.

## 2. Características Detalhadas da Linguagem W

### 2.1 Sintaxe Moderna

W possui uma sintaxe limpa e moderna, semelhante a C, mas com extensões para facilitar a escrita de código. Exemplos:

- **Destruturação**: Desestruturação de structs e arrays para accessar campos diretamente.

```w
func process({ a: String, b: Int }) {
  print(`Received a: ${a}, b: ${b}`)
}

func main() {
  let data = { a: "Hello", b: 42 }
  process(data)
}
```

- **Operadores de Coalescência Nula e Opcionais**: Usa `??` para valores padrão e `?` para tipos opcionais.

```w
let name: String? = nil
let displayName = name ?? "Anonymous" // Usa "Anonymous" se name for nil
```

- **Async/Await**: Suporte para operações assíncronas estruturadas.

```w
async func fetchData(url: String) throws -> String {
  let response = await http.get(url)
  return await response.text()
}

func main() {
  try await print(fetchData("https://example.com"))
}
```

### 2.2 Sistema de Tipos Avançado

W possui um sistema de tipos estático com inferência de tipos e restrições. Tipos podem ser decorados com restrições para validação em tempo de compilação.

- **Tipos Restritos**: Exemplo com `String<maxLength: 22>` ou `Number<range: 0...100>`.

```w
type Username = String<maxLength: 22, pattern: /^[a-zA-Z0-9]+$/>

func setUsername(name: Username) {
  print(`Username set to: ${name}`)
}

func main() {
  let validName: Username = "John123" // Válido
  try setUsername(validName)

  let invalidName: Username = "John@123" // Lança erro em tempo de compilação
}
```

### 2.3 Gerenciamento de Memória

W utiliza um modelo híbrido de gerenciamento de memória:

- **Contagem de Referências Automática (ARC)**: Para tipos de referência (classes, módulos).
- **Semântica de Valor**: Para structs, enums e primitivos, alocados na stack ou embutidos.

Modificadores de propriedade controlam posse e compartilhamento:

- `ref`: Referência mutável sem transferência de posse.
- `storage`: Garante persistência além do escopo.
- `transfer`: Transfere posse para o chamador.
- `cow` (copy-on-write): Compartilhamento eficiente com cópia sob demanda.

Exemplo:

```w
class Data {
  let content: String
}

func processData(data: ref Data) -> storage Data {
  data.content += " Processed"
  return data // Retorna com posse transferida
}

func main() {
  let data = Data { content: "Raw" }
  let processed = processData(ref data)
  print(processed.content) // "Raw Processed"
}
```

### 2.4 Concorrência Estruturada

W suporta concorrência via corrotinas, `async`/`await` e `spawn` para paralelismo:

- **Corrotinas**: Funções pausáveis e retomáveis para operações assíncronas.
- **Async/Await**: Simplifica código assíncrono.
- **Spawn**: Lança tarefas em threads separadas.

Exemplo:

```w
async func fetchUser(userId: String) throws -> User {
  let data = await http.get(`https://api.example.com/users/${userId}`)
  return await data.decode(User.self)
}

func main() {
  let task = spawn fetchUser("123")
  // Outra lógica...
  let user = try await task
  print(user.name)
}
```

### 2.5 Interoperabilidade com C

W é um superconjunto de C, permitindo chamadas diretas a funções C e uso de bibliotecas C. O compilador W gera código C intermediário, que é compilado com um compilador C (como GCC ou Clang).

#### Exemplo de Código W e Seu Equivalente em C

**Código W:**

```w
import { printf } from "c:stdio.h"

func greet(name: String) {
  printf("Hello, %s!\n", name)
}

func main() {
  greet("World")
}
```

**Equivalente em C (Backend Gerado pelo Compilador W):**

```c
#include <stdio.h>

void greet(char* name) {
    printf("Hello, %s!\n", name);
}

int main() {
    greet("World");
    return 0;
}
```

#### Detalhes do Backend em C

- O compilador W transforma o código W em um AST, que é serializado como código C intermediário.
- Funções W são mapeadas para funções C, com tipos W convertidos para tipos C correspondentes (e.g., `String` em W pode ser um `char*` ou uma struct customizada).
- Tipos restritos e gerenciamentos de memória (ARC, `cow`) são implementados em tempo de execução com bibliotecas C geradas automaticamente.
- Chamadas assíncronas e corrotinas são transformadas em callbacks ou threads C, utilizando `corrotinas` ou `pthreads`.

Exemplo mais complexo com gerenciamento de memória:

**Código W:**

```w
class Person {
  let name: String
}

func createPerson(name: String) -> Person {
  return Person { name: name }
}

func main() {
  let person = createPerson("Alice")
  print(person.name)
}
```

**Equivalente em C (com ARC Simulado):**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* name;
    int ref_count;
} Person;

Person* Person_new(const char* name) {
    Person* p = (Person*)malloc(sizeof(Person));
    p->name = strdup(name);
    p->ref_count = 1;
    return p;
}

void Person_retain(Person* p) {
    if (p) p->ref_count++;
}

void Person_release(Person* p) {
    if (p) {
        p->ref_count--;
        if (p->ref_count == 0) {
            free(p->name);
            free(p);
        }
    }
}

void print(const char* str) {
    printf("%s\n", str);
}

void createPerson(const char* name, Person** out) {
    *out = Person_new(name);
}

int main() {
    Person* person;
    createPerson("Alice", &person);
    print(person->name);
    Person_release(person);
    return 0;
}
```

### 2.6 Modularidade

Módulos em W são singletons, encapsulando código e gerenciando memória e paralelismo:

```w
module Math {
  export func add(a: Int, b: Int) -> Int {
    return a + b
  }
}

module Main {
  import { add } from "Math"

  func main() {
    print(add(3, 4)) // 7
  }
}
```

Configurações de módulo controlam recursos:

```w
module Network {
  #config {
    dynamic.maxSize = 1G
    threads = [.background, .network]
    callPolice = .rcu // Read-Copy-Update para concurrência
  }
}
```

## 3. Ferramentas de Desenvolvimento

- **Compilador W**: Utiliza Clang como backend inicial, gerando código C intermediário.
- **Linter**: Verifica código W por boas práticas e segurança.
- **IDE Suporte**: Plugin para VSCode com autocompletar e debug.
- **Gerenciador de Pacotes**: `w build` para gerenciar dependências e construir projetos.