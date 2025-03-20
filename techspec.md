
## **Especificação Técnica do W Tagged Pointer**

O **W Tagged Pointer** é uma estrutura de dados eficiente que armazena diferentes tipos de valores ou ponteiros em um único registrador (`uintptr_t`), utilizando sempre **2 bits menos significativos** (bits 0 e 1) para indicar o tipo do dado e os bits restantes (bits 2 até o máximo) para outros propósitos como o valor ou endereço. Ele é projetado para ser portátil entre arquiteturas de **32 bits** e **64 bits**, oferecendo suporte a valores escalares (inteiros e floats), ponteiros para objetos compostos (como arrays e strings) e ponteiros compartilhados com contagem de referência.

---

- **Bits 0-1**: **Tipo** do dado (`00` = `INT`, `01` = `FLOAT`, `10` = `COMPOUND`, `11` = `SHARED`).
- **Bits 2 até o máximo**: **Valor**, **Endereço**, **Tags**, **Subtipo**, dependendo do tipo.

--

### **Valores Especiais**

Para `TYPE_INT` e `TYPE_FLOAT`:
- **`NULL`**:
  - Menor valor negativo representavel possivel para os bits de value. (62 bits restantes no caso de tipo escalar em 64 bits)
  - Indica ausência de valor ou erro específico.
  - 64 bits: 10000000000000000000000000000000000000000000000000000000000001 xx

- **`ERRO`**:
  - Maior valor positivo representavel possivel para os bits de value.
  - Indica estado de erro ou over/underflow.
  - 64 bits: 01111111111111111111111111111111111111111111111111111111111111 xx

Para `TYPE_COMPOUND` e `TYPE_SHARED` **`NULL`** e **`ERRO`** são respectivamente todos os bits de **Endereço** `0` ou `1`. Tipicamente em 64 bits temos 48 bits para endereço, mas dependendo da plataforma esse valor pode chegar a 57.

- **`NULL`**:
  - Todos os bits da area reservada para **Endereço** são zero.
  - Indica ausência de valor ou erro específico.
  - 64 bits: 000000000000000000000000000000000000000000000000 xxxxxxxxxxxxxx xx

- **`ERRO`**:
  - Maior valor positivo possivel para os 62 bits restantes. (no caso de tipo escalar em 64 bits)
  - Indica estado de erro ou overflow.
  - 64 bits: 111111111111111111111111111111111111111111111111 xxxxxxxxxxxxxx xx

Em uma eventual plataforma onde os bits de endereço sejam 62, ou mesmo todos os 64 bits o TaggedPointer não teria suporte a tags ou subtipo. Eventualmente essas informações podem ser armazenadas em uma estrutura intermediaria.

---

### **Detalhes por Tipo**

#### **1. Inteiro (`TYPE_INT`)**
- **Tag de Tipo**: `00`.
- **Bits de Valor**: Todos os bits disponíveis.
- **Interpretação**: Inteiro assinado.
- **Valores Especiais**:
  - `INT_NULL`: O primeiro bit é `1` (positivo) e ultimo bit de valor também é `1`, e todos os outros bits são `0`.
    - 64 bits: 10000000000000000000000000000000000000000000000000000000000001 00
  - `INT_ERRO`: O primeiro bit é `0` (netativo) e todos os outros bits são `1`.
    - 64 bits: 01111111111111111111111111111111111111111111111111111111111111 00
 - **Layout Geral**:
  - 64 bits:
    ```
  +---------------------------------+
  | 63                      2 | 1 0 |
  +---------------------------+-----+
  | Valor (62 bits)           | 0 0 |
  +---------------------------+-----+
  ```
  - 32 bits:
    ```
  +---------------------------+-----+
  | 31                      2 | 1 0 |
  +---------------------------+-----+
  | Valor          (30 bits)  | 0 0 |
  +---------------------------+-----+
  ```

#### **2. Float (`TYPE_FLOAT`)**
- **Tag de Tipo**: `01`.
- **Bits de Valor**: Todos os bits disponíveis.
- **Interpretação**: Ponto flutuante (IEEE 754) de 32/64 bits, com truncamento nos ultimos 2 bits.
  - *Em 64 bits*:
    - *double* IEEE-754 cast to 62 bits.
    - Sinal: 1 bit (bit 61)
    - Expoente: 11 bits (bits 60 a 50)
    - Mantissa: 50 bits (bits 49 a 0)
      - É feito o cast dos ultimos 2 digitos da mantissa.
  - *Em 32 bits*:
    - *float* IEEE-754 cast to 30 bits.
    - Sinal: 1 bit (bit 29)
    - Expoente: 8 bits (bits 28 a 21)
    - Mantissa: 21 bits (bits 20 a 0)
      - É feito o cast dos ultimos 2 digitos da mantissa.
- **Valores Especiais**:
  - `FLOAT_NULL`: O primeiro bit é `1` (positivo) e ultimo bit de valor na mantissa também é `1`, e todos os outros bits são `0`.
    - 64 bits: 1 00000000000 00000000000000000000000000000000000000000000000001 00
  - `FLOAT_ERRO`: O primeiro bit é `0` (netativo) e todos os outros bits são `1`, produzindo um NaN especifico.
    - 64 bits: 0 11111111111 11111111111111111111111111111111111111111111111111 01
 - **Layout Geral**:
  - 64 bits:
    ```
  +---------------------------------+
  | 63                      2 | 1 0 |
  +---------------------------+-----+
  | Valor (62 bits)           | 0 01 |
  +---------------------------+-----+
  ```
  - 32 bits:
    ```
  +---------------------------+-----+
  | 31                      2 | 1 0 |
  +---------------------------+-----+
  | Valor (30 bits)           | 0 1 |
  +---------------------------+-----+
  ```

#### **3. Composto (`TYPE_COMPOUND`)**
- **Tag de Tipo**: `10`.
- **Bits de Endereço**: Endereço do objeto composto. Diferente dos objetos escalares, ao obter o value de um objeto composto é esperado que seja obtido a referencia desse objeto. (ex: chat* int*)
- **Tags Adicionais**: Em arquiteturas com endereços menores (ex.: 48 bits em 64-bit), bits extras podem ser usados como tags.
  - **Sub tipo das Tags**: 2 bits adicionais são separados para representar os subtipos.
    - `SUBTYPE_STRING`, `SUBTYPE_ARRAY`, `SUBTYPE_ENUM`, `SUBTYPE_SHARED`
  - **Tipo das Tags**: Quando disponivel as tags devem ser interpretadas como inteiros sem sinal (uint16_t) com cast ao tamanho disponivel para a tag. Ex: 'uint12_t'
  - **Uso das Tags**: Tamanho do array ou string (até 4095 elementos com 12 bits). Contagem de referencias para o shared.
  - `SUBTYPE_SHARED`: Tipo especial responsavel por contagem de referencia. Dedicado a objetos referenciados em multiplos contextos.
- **Valores Especiais**:
  - `NULL`: Todos os bits de endereço (48bits por exemplo) são `0`, e todos os outros bits são `0`.
    - 64 bits: 000000000000000000000000000000000000000000000000 xxxxxxxxxxxx xx 10
  - `ERRO`: O primeiro bit é `0` (netativo) e todos os outros bits são `1`, produzindo um NaN especifico.
    - 64 bits: 111111111111111111111111111111111111111111111111 xxxxxxxxxxxx xx 10

#### **4. Ponteiro Compartilhado (`TYPE_COMMON`)**
- **Tag de Tipo**: `11`.
- **Bits de Endereço**: Objetos definidos pelo usuario. 
- **Tags Adicionais**: Em arquiteturas com endereços menores (ex.: 48 bits em 64-bit), bits extras podem ser usados como tags.
  - **Tipo das Tags**: Quando disponivel as tags devem ser interpretadas como inteiros sem sinal (uint) do tamanho da tag (uint14 por exemplo)
  - **Uso das Tags**:  Uso definido de acordo com o objeto.
- **Valores Especiais**:
  - `NULL`: Todos os bits de endereço (48bits por exemplo) são `0`, e todos os outros bits são `0`.
    - 64 bits: 000000000000000000000000000000000000000000000000 xxxxxxxxxxxxxx 11
  - `ERRO`: O primeiro bit é `0` (netativo) e todos os outros bits são `1`, produzindo um NaN especifico.
    - 64 bits: 111111111111111111111111111111111111111111111111 xxxxxxxxxxxxxx 11

---

### **Portabilidade**

- **32 bits**: 30 bits de valor, com suporte a inteiros e floats truncados. Em 32 bits é feito o truncamento da mantissa tipo "float" de 32 bits para 30. Não há suporte a tags.
- **64 bits**: 62 bits de valor, permitindo maior precisão e endereços maiores. Em 64 bits é feito o truncamento da mantissa tipo "double" de 64 bits para 62.