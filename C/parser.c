#include <stdio.h>
#include <regex.h>

int main() {
  // Código W de exemplo
  char *w_code = "fn soma(a: i32, b: i32): i32 { return a + b; }";
  
  // Expressão regular para capturar elementos da função
  char *regex_str = "fn ([^(]+)\\(([^)]*)\\): ([^\\{]+) \\{([^}]*)\\}";
  
  // Compila a expressão regular
  regex_t regex;
  regcomp(&regex, regex_str, REG_EXTENDED);
  
  // Executa a regex no código W
  regmatch_t matches[5];
  if (regexec(&regex, w_code, 5, matches, 0) == 0) {
    // Extrai os grupos correspondentes
    char *nome_funcao = w_code + matches[1].rm_so;
    char *argumentos = w_code + matches[2].rm_so;
    char *tipo_retorno = w_code + matches[3].rm_so;
    char *corpo_funcao = w_code + matches[4].rm_so;
    
    // Ajusta o tamanho das strings
    nome_funcao[matches[1].rm_eo - matches[1].rm_so] = '\0';
    argumentos[matches[2].rm_eo - matches[2].rm_so] = '\0';
    tipo_retorno[matches[3].rm_eo - matches[3].rm_so] = '\0';
    corpo_funcao[matches[4].rm_eo - matches[4].rm_so] = '\0';
    
    printf("%s | %s | %s | %s \n", tipo_retorno, nome_funcao, argumentos, corpo_funcao);
  }
  
  // Libera a regex
  regfree(&regex);
  
  return 0;
}