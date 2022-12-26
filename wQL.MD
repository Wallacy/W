Web QL

Baseado em todos os status do rest, headers etc.

SELECT -> GET -> retorna apenas result JSON.
Query -> GET -> Um ou mais resultados em um result json array [] na ordem de cada query
INSERT -> POST
UPDATE -> PUT
DELETE -> Delete
-> Options -> Introspeção, se manda na / tras todos os top level objects, mas pode ser /object para apenas um subset.
CALL -> POST
Query -> POST/PUT/PATCH -> Um ou mais operações de escrita com o resultado de cada operação em  um result json array [] na ordem de cada query
Query(mix) -> POST -> Um ou mais operações de escrita com o resultado de cada operação em  um result json array [] na ordem de cada query

*Query é implicitamente o tipo usado no method. Se for get é select, se for post é insert...
Só de misturar é necessario desambiguar.

//
List ?
//

tentar bolar algo com o tagget template do js

wql`select name from country.states` -> ["São Paulo", "Brasilia"...]

wql`query name from country.states` -> { query: ["São Paulo", "Brasilia"...] }
wql`query select name from country.states` -> { query: ["São Paulo", "Brasilia"...] }

wql`
query name from country.states
query name from country.states.city
` -> [["São Paulo", "Brasilia"...], ["Salvador", "Patos de Minas"...]]

wql`
query insert name=${name} on country.states
query name from country.states.city
` -> ["200: Insert "Magic" on country.states", ["Salvador", "Patos de Minas"...]]

wql`
select {
  some:{
    complex: {
      object
    }
  }
}
` -> { ... }

// Se não colocar o ON se considera que o search space é todo os root objects.
// Nesse caso funciona bem parecido com graphql
// O "on" é opcional


Atraversar o objeto por padrão como se fosse rest:

/root/propertie/etc...

analisar do mais generico para o mais especifico, dessa forma se o handler do root retornar tudo pra baixo não precisa processar os proximos niveis.



// old...
Mesma coisa do W porem você só define o modelo de dados e ele abastrai todo backend.
Você usa define via plugins storages, como sqlite, file system, KV (Workers / Durable), Redis, etc...

A ideia é ser compativel com grapqh também.

No caso do a ideia é declarar a transformação dos dados e criar os tipos de dados expandindo os primitivos.
Coisas como validações etc entram como anotations (diretivas do graphql).
A ideia é que se programe quase nada do back, tudo seja plugin, anotation ou middleware.

Também seja agnostico ao modo de transporte, TCP, UDP, websocket, etc... Dai já tenha os proviles para cada situação.

... wip

//

Tava pensando em algo onde as query são enviadas via header.... e basicamente a query é um json simples com palceholder:
{
  product: ""
  price: 0.0
}

Dessa forma ele ta pedindo product com value string e price com value double; Qualquer valor colocado na tag será considerado palceholder;

Eventualmente o mesmo pode ser usado para formatar date/time e outros campos. (max length em string?)

Criar os paths atumaticamente usando json schema é uma opção, mas parece mais dificil também. Algo parecido com o TRPC é legal, eventualmente isso pode ser usado em algum modo "dinamico" one não tem validação real, apenas promessa que irá tudo funcionar porque pode ser validado as duas pontas (back/front) via typescript.

//
