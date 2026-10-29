#ifndef ECOMMERCE_H
#define ECOMMERCE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* nomes dos arquivos */
#define ARQ_PRODUTOS "produtos.bin"
#define IDX_PRODUTOS "produtos.idx"
#define EXT_PRODUTOS "produtos.ovf"
#define TMP_PRODUTOS "produtos.tmp"
#define TMP_INDICE "produtos.idx.tmp"

/* os tamanhos fecham cada struct num multiplo de 4: senao o gcc poe
   padding depois do fim e o registro deixa de terminar em '\n' */
#define TAM_MARCA 20
#define TAM_CATEGORIA 42

/* uma entrada do indice para cada SPAN_PRODUTOS posicoes fisicas do arquivo
   principal. Registro de 80 bytes: 51 * 80 = 4080, cabe numa pagina de 4 KB */
#define SPAN_PRODUTOS 51
#define MAX_EXTENSAO 64

/* ---------- tipos ---------- */

typedef enum {
  AREA_PRINCIPAL,
  AREA_EXTENSAO,
  AREA_NENHUMA
} Area;

/* localizacao fisica de um registro: area + offset em bytes naquele arquivo */
typedef struct {
  Area area;
  int offset;
} Referencia;

/* layout sem padding: id 0-3, preco 4-7, elo 8-15, ativo 16, marca 17-36,
   categoria 37-78, fim 79 */
typedef struct {
  int id;
  int preco;
  Referencia elo;
  char ativo;
  char marca[TAM_MARCA];
  char categoria[TAM_CATEGORIA];
  char fim;
} Produto;

_Static_assert(sizeof(Produto) == 80, "Produto ganhou padding: o formato do arquivo mudaria");

/* a entrada aponta sempre para a cabeca de um bloco do arquivo principal */
typedef struct {
  int id;
  int offset;
} EntradaIndice;

typedef struct {
  int span;
  int total_principal;
  int quantidade_entradas;
  EntradaIndice *entradas;
} IndiceParcial;

typedef struct {
  FILE *dados;
  FILE *ext;
  int total;
  int total_ext;
  IndiceParcial ind;
} BaseProdutos;

/* resultado de percorrer a cadeia de um bloco atras de um id */
typedef struct {
  int bloco;
  Referencia anterior;
  Referencia atual;
  int comp_indice;
  int lidos;
} Percurso;

/* codigos de retorno da insercao */
enum {
  INS_ERRO = -1,
  INS_DUPLICADO = 0,
  INS_RESERVADO = 1,
  INS_NA_EXTENSAO = 2,
  INS_NA_CABECA = 3,
  INS_BASE_VAZIA = 4
};

/* ---------- produtos.c ---------- */

Referencia ref_principal_offset(int offset);
Referencia ref_principal(int pos);
Referencia ref_extensao(int pos);
Referencia ref_fim(void);
int ref_eh_fim(Referencia r);
void texto_ref(char *buf, size_t tam, Referencia r);

void preencher_texto(char *destino, int tam, const char *origem);

int ler_produto(FILE *f, int i, Produto *destino);
int ler_registro(BaseProdutos *b, Referencia ref, Produto *p);

void imprimir_em(Referencia ref, const Produto *p);
void imprimir_indice_produtos(const IndiceParcial *ind);

int abrir_base_produtos(BaseProdutos *b);
void fechar_base_produtos(BaseProdutos *b);

int buscar_produto(BaseProdutos *b, int id, int incluir_excluidos, Produto *p, Percurso *r);
int inserir_produto(BaseProdutos *b, Produto *novo, int *regenerou);
int remover_produto(BaseProdutos *b, int id);
int reorganizar_produtos(BaseProdutos *b);

#endif
