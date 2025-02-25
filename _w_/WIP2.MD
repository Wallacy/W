## WIP Linguagem W

Este documento é o meu ponto de referência para o desenvolvimento da linguagem W. Aqui eu consolido tudo o que pensei até agora: a visão geral, a sintaxe, os tipos, o gerenciamento de memória, a concorrência, a interoperabilidade com C, a modularidade e o sistema de build. É um guia vivo pra mim mesmo, então vou manter o tom natural e direto. Também incluí referências a bibliotecas que pretendo usar como base e organizei as ideias descartadas em uma seção separada.

---

### **1. Visão Geral da Linguagem W**

W é uma linguagem moderna que eu quero construir como um superconjunto de C. A ideia é pegar o que C tem de bom (performance, controle de baixo nível) e adicionar segurança, expressividade e recursos pra facilitar a vida do desenvolvedor, tipo o que Rust, Zig e Swift fazem, mas sem abandonar a interoperabilidade direta com C. O compilador W vai gerar código C puro (provavelmente C2y), que depois eu passo pra um compilador C como GCC ou Clang. O foco é:

- **Segurança**: Evitar bugs chatos como ponteiros nulos e corridas de dados.
- **Expressividade**: Sintaxe limpa e moderna, com coisas como `async/await` e destruturação.
- **Desempenho**: Igual ou próximo de C, sem overhead desnecessário.
- **Interoperabilidade**: Chamar funções C e usar bibliotecas C direto.
- **Concorrência**: Suporte nativo pra threads e assincronia estruturada.
- **Modularidade**: Módulos que organizam o código e gerenciam memória/paralelismo sozinhos.

---

### **2. Como Vai Funcionar**

#### **2.1 Sintaxe Moderna**

Quero uma sintaxe que seja fácil de ler e escrever, pegando inspiração de linguagens como TypeScript e Swift. Alguns exemplos do que eu pensei:

- **Destruturação**: Pra pegar campos de structs ou arrays direto.

```w
func process({ nome: String, idade: Int }) {
  print(`Oi ${nome}, você tem ${idade} anos`)
}
```

- **Opcionais e Coalescência Nula**: Usar `?` pra tipos que podem ser nulos e `??` pra valores padrão.

```w
let usuario: String? = nil
let nome = usuario ?? "Anônimo"
```

- **Async/Await**: Pra simplificar código assíncrono.

```w
async func baixarDados(url: String) -> String {
  let resposta = await http.get(url)
  return resposta
}
```

#### **2.2 Sistema de Tipos**

O sistema de tipos vai ser estático com inferência, mas com uns extras legais:

- **Tipos Restritos**: Posso definir restrições nos tipos pra garantir segurança em tempo de compilação.

```w
type Nome = String<maxLength: 20, pattern: /^[a-zA-Z]+$/>

func setNome(nome: Nome) {
  print(nome)
}
```

Aqui o compilador já verifica se o `nome` tem até 20 caracteres e só letras. Isso vai ser mapeado pra C com structs ou checagens inline.

#### **2.3 Gerenciamento de Memória**

O gerenciamento de memória é um dos grandes pilares da W. Minha ideia principal é usar **ARC (Automatic Reference Counting)** pra tipos de referência (como classes e módulos), parecido com Swift:

- Quando a contagem de referências chega a zero, o objeto é desalocado automaticamente.
- Isso vai ser implementado no código C gerado com uma biblioteca leve que eu vou criar, tipo um `w_refcount.h`.

Pra structs e tipos primitivos, a semântica é de valor (alocados na stack ou embutidos). Também pensei em modificadores pra controlar posse e cópia:

- `ref`: Referência com ARC.
- `storage`: posse única, transferida pro destino.
- `cow`: Copy-on-write pra otimizar memória.

Exemplo:

```w
class Dado {
  let texto: String
}

func processar(dado: ref Dado) -> storage Dado {
  dado.texto += " processado"
  return dado
}
```

No C gerado, isso vira uma struct com um contador de referência e funções pra incrementar/decrementar.

#### **2.4 Concorrência**

Quero suporte nativo pra concorrência estruturada. As ideias são:

- **Async/Await**: Pra operações assíncronas, que no backend viram callbacks ou corrotinas em C.
- **Spawn**: Pra lançar tarefas em threads separadas.

```w
async func pegarUsuario(id: String) -> Usuario {
  let dados = await http.get(`api/usuarios/${id}`)
  return dados.decode(Usuario)
}

func main() {
  let tarefa = spawn pegarUsuario("123")
  let usuario = await tarefa
  print(usuario.nome)
}
```

No C, o `spawn` vai usar `pthread_create`, e o `async/await` pode ser implementado com uma máquina de estados ou callbacks. Testei isso nos trechos de código com `pthread`, e funciona bem se o caller alocar os argumentos e resultados (evitando `pthread_exit` pra retorno direto).

#### **2.5 Interoperabilidade com C**

W vai ser 100% compatível com C. Qualquer função C pode ser chamada diretamente, e o compilador W gera C como output. Exemplo:

```w
import { printf } from "c:stdio.h"

func dizerOi(nome: String) {
  printf("Oi, %s!\n", nome)
}
```

No C gerado:

```c
#include <stdio.h>
void dizerOi(char* nome) {
  printf("Oi, %s!\n", nome);
}
```

Os tipos de W (como `String`) viram `char*` ou structs customizadas com gerenciamento de memória embutido. A ideia é manter o overhead mínimo.

#### **2.6 Modularidade**

Módulos em W são singletons que encapsulam código e gerenciam memória/paralelismo. Exemplo:

```w
module Matematica {
  export func soma(a: Int, b: Int) -> Int {
    return a + b
  }
}

module Principal {
  import { soma } from "Matematica"

  func main() {
    print(soma(5, 3)) // 8
  }
}
```

Posso configurar os módulos com diretivas:

```w
module Rede {
  #config {
    threads = [.background, .network]
    memory = .rcu
  }
}
```

No C, cada módulo vira um arquivo `.c` com uma struct de exports (como nos meus testes com `file1_exports`).

---

### **3. Sistema de Build**

Vou criar um arquivo `build.w` que será acionado com o comando `w build`. Ele vai:

- Baixar dependências (provavelmente com um gerenciador de pacotes simples).
- Compilar o código W pra C.
- Chamar um compilador C (GCC/Clang) pra gerar o executável.

Exemplo de `build.w`:

```w
module Build {
  func main() {
    let fontes = glob("src/*.w")
    let objetos = fontes.map(f => compile(f, "c"))
    link(objetos, "meu_programa")
  }
}
```

O `w build` vai interpretar esse arquivo e executar as tarefas.

---

### **4. Referências e Bibliotecas**

Essas são as bibliotecas que pretendo usar como base ou inspiração:

- **[pthread](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html)**: Pra threads e concorrência no backend C.
- **[Clang/LLVM](https://clang.llvm.org/)**: Como backend inicial pro compilador W.
- **[Tree-sitter](https://tree-sitter.github.io/)**: Pra criar o parser da linguagem (já tenho uma gramática básica).
- **[mimalloc](https://github.com/microsoft/mimalloc)**: Talvez como alocador customizado pra melhorar o ARC.

---

### **5. Alternativas Consideradas**

Aqui estão ideias que pensei mas descartei ou deixei em aberto:

- **Linguagem Intermediária WC.MD**: Inicialmente pensei em gerar um intermediário antes do C, mas decidi ir direto pra C puro pra simplificar.
- **Garbage Collector**: Considerei um GC ao invés de ARC, mas descartei por causa do overhead e da perda de controle.
- **Atomic por Padrão**: Nos testes com `union vec`, pensei em tornar tudo atômico por padrão, mas prefiro deixar opcional pra dar flexibilidade (usar `_Atomic` só quando precisar).
- **Retorno via `pthread_exit`**: Testei retornar valores via `pthread_exit`, mas prefiro que o caller aloque tudo e receba resultados diretamente nos args, simplificando o gerenciamento de threads.

---

### **6. Próximos Passos**

- Implementar um protótipo do compilador W que gere C básico.
- Testar o ARC com uma biblioteca simples em C.
- Escrever o `build.w`
- Expandir a gramática do Tree-sitter pra cobrir toda a sintaxe.

---