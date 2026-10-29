# Índices parciais — e-commerce

Trabalho 1 de Estrutura de Dados II: dois arquivos binários de registros de
tamanho fixo, ordenados pela chave, cada um com um índice esparso
(organização sequencial-indexada) e uma área de extensão encadeada para as
inserções.

## Compilar e rodar

```
make
./ecommerce
```

Os arquivos de dados e de índice já estão no repositório. Para gerá-los de
novo a partir do CSV (inserção dos dados iniciais):

```
uv run --with pandas python3 scripts/gerar_dados.py
```

ou, com o pandas instalado, `python3 scripts/gerar_dados.py`. O programa C
recria os `.idx` e os `.ovf` ao abrir.

## Dados

Recorte dos primeiros 200 mil eventos (`dados/2019-Oct-200k.csv`, de
01/10/2019) do dataset
[eCommerce behavior data from multi category store](https://www.kaggle.com/datasets/mkechinov/ecommerce-behavior-data-from-multi-category-store)
(Kaggle). Cada linha do CSV é um evento: um usuário viu um produto, pôs no
carrinho ou comprou.

## Consultas

1. Qual a marca mais vendida?
2. Qual a taxa de conversão de um produto (visualizações, carrinho, compras)?
3. Qual o faturamento por categoria?

As três leem `acessos.bin` um registro por vez e buscam o produto de cada
acesso pelo índice de `produtos`. Nenhum arquivo de dados é carregado inteiro
na memória: só os índices ficam em RAM.

## Arquivos de dados

Registros de tamanho fixo, sem padding (conferido por `_Static_assert`). Os
textos são completados com espaços, sem `'\0'`, e cada registro termina em
`'\n'`. Os números são `int` binários; um deles pode conter o byte `0x0A`,
então um editor de texto às vezes quebra a linha no meio de um registro.

### `produtos.bin` — 30.223 registros de 80 bytes

| bytes | campo | |
|---|---|---|
| 0–3 | `id` | `product_id`, **chave** (sem repetição) |
| 4–7 | `preco` | último preço visto, em centavos |
| 8–15 | `elo` | área + offset do próximo registro da cadeia |
| 16 | `ativo` | `'1'` ativo, `'0'` removido |
| 17–36 | `marca` | última marca não vazia vista (repete) |
| 37–78 | `categoria` | `category_code` (repete) |
| 79 | `fim` | `'\n'` |

Um produto aparece em vários eventos do CSV; o gerador agrupa por
`product_id` e ordena pela chave.

### `acessos.bin` — 200.000 registros de 56 bytes

| bytes | campo | |
|---|---|---|
| 0–3 | `id` | **chave** sequencial, gerada na ordem do CSV |
| 4–7 | `produto` | `product_id` (repete) |
| 8–11 | `usuario` | `user_id` (repete) |
| 12–15 | `preco` | preço no momento do evento, em centavos |
| 16–23 | `elo` | área + offset do próximo registro da cadeia |
| 24 | `ativo` | `'1'` ativo, `'0'` removido |
| 25–35 | `tipo` | `view`, `cart` ou `purchase` (repete) |
| 36–54 | `hora` | `aaaa-mm-dd hh:mm:ss` |
| 55 | `fim` | `'\n'` |

O CSV não tem chave única por evento, então o `id` é um número sequencial.
O preço fica no acesso porque o preço de um produto muda ao longo do dia, e o
faturamento usa o preço de cada compra.

## Arquivos de índice e de extensão

O arquivo principal é dividido em blocos de tamanho fixo. Cada bloco é uma
cadeia ligada pelo campo `elo`, e o índice tem uma entrada por bloco, com o
`id` e o offset da cabeça.

| | registros por bloco | entradas no índice |
|---|---|---|
| produtos | 51 (51 × 80 = 4.080 bytes) | 593 |
| acessos | 73 (73 × 56 = 4.088 bytes) | 2.740 |

O tamanho do bloco faz cada bloco caber numa página de 4 KB. Há um nível de
índice: com 593 e 2.740 entradas de 8 bytes, o índice inteiro cabe em RAM.

Formato do `.idx`: três `int` (registros por bloco, total de registros do
principal, número de entradas) seguidos das entradas `{int id; int offset}`.
O índice é gravado ao sair e ao regenerar, e carregado ao abrir. Se não bater
com o arquivo de dados, é recriado.

`produtos.ovf` e `acessos.ovf` são as áreas de extensão: até 64 registros no
mesmo formato do principal, sem ordem física.

## Operações

- **Consulta pelo índice:** pesquisa binária no índice para achar o bloco e,
  depois, `seek` no arquivo de dados, seguindo a cadeia até o `id`.
- **Pesquisa binária direto no arquivo:** feita sobre o arquivo principal,
  que continua ordenado por posição. A extensão é lida em sequência no fim.
- **Inserção:** o registro vai para a extensão e é ligado na cadeia, depois
  do seu predecessor. Em produtos, um `id` menor que a primeira cabeça ocupa
  a posição dela, e a cabeça antiga vai para a extensão. Em acessos, o `id`
  novo é sempre o maior + 1, então entra no fim da cadeia do último bloco.
- **Remoção:** lógica (`ativo = '0'`). O registro e o elo continuam onde
  estavam, e o `id` fica reservado até a regeneração.
- **Regeneração:** quando a extensão enche, ou pelo menu. Percorre as
  cadeias em ordem, descarta os removidos, reescreve o principal, refaz os
  elos e reconstrói o índice.

| | produtos | acessos |
|---|---|---|
| listar faixa do principal | 1 | 10 |
| listar área de extensão | 2 | 11 |
| consulta pelo índice | 3 | 12 |
| pesquisa binária no arquivo | 9 | 13 |
| imprimir índice | 4 | 14 |
| imprimir cadeia de um bloco | 5 | 15 |
| inserir | 6 | 16 |
| remover | 7 | 17 |
| regenerar | 8 | 18 |

As opções 19, 20 e 21 são as consultas.

## Resultados

```
produtos.bin: 30223 produtos de 80 bytes, 0/64 na extensao
produtos.idx: 593 entradas, uma a cada 51 registros
acessos.bin: 200000 acessos de 56 bytes, 0/64 na extensao
acessos.idx: 2740 entradas, uma a cada 73 registros
```

Marca mais vendida (opção 19):

```
  3639 compras, 301 sem marca, 329 marcas

   1  samsung                 857 compras
   2  apple                   659 compras
   3  xiaomi                  299 compras
```

Taxa de conversão do produto 1004767 (opção 20):

```
   P526   1004767  1     254.76  samsung               electronics.smartphone                      -> P527
  view 1595  cart 154  purchase 127
  conversao (compras / visualizacoes): 8.0%
```

Faturamento por categoria (opção 21):

```
  3639 compras, 836 sem categoria, 88 categorias

   1  electronics.smartphone                          750087.55   1665 compras
   2  computers.notebook                               52420.21     97 compras
   3  electronics.video.tv                             28758.76     76 compras

  total das categorias: 990080.34
```

## Uso de IA

Usamos o Claude Opus 5.5, da Anthropic, como apoio ao longo do trabalho, principalmente em:

- discussão do formato dos registros e do índice;
- escrita e revisão de funções em C;
- testes das buscas, da inserção, da remoção e das consultas.

As decisões de estrutura e de funcionamento foram nossas; a IA funcionou como
par de programação.
