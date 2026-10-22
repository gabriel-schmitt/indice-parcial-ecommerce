# Arquivos de dados com índices parciais — e-commerce

Trabalho 1 de Estrutura de Dados II: arquivos binários de registros de tamanho
fixo, ordenados pela chave, com um índice parcial por arquivo, como na
organização sequencial-indexada.

## Dados

Recorte dos primeiros 200 mil eventos (`dados/2019-Oct-200k.csv`) do dataset
[eCommerce behavior data from multi category store](https://www.kaggle.com/datasets/mkechinov/ecommerce-behavior-data-from-multi-category-store)
(Kaggle), do qual saem dois arquivos:

- `produtos.bin`: um registro por produto (chave `product_id`);
- `acessos.bin`: um registro por evento (view, cart, purchase), com chave
  sequencial gerada na carga.

Os registros têm tamanho fixo, os textos são completados com espaços e cada
registro termina em `'\n'`.

## Consultas

1. Qual a marca mais vendida?
2. Qual a taxa de conversão de um produto (visualizações, carrinho, compras)?
3. Qual o faturamento por categoria?

## Funções, para cada arquivo

1. inserir os dados iniciais;
2. mostrar os dados;
3. pesquisa binária direto no arquivo de dados;
4. consulta pelo índice parcial;
5. inserção;
6. remoção.

Nenhuma busca carrega o arquivo de dados na memória: só o índice fica em RAM.

## Compilar e rodar

```
make
./ecommerce
```
