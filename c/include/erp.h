/*
 * Aurora ERP - Nucleo em C11
 * PIM II - UNIP (ADS)
 * Autor: Gabriel Abreu Ribeiro
 * Repositorio: https://github.com/Duke0007/AuroraERP
 *
 * Cabecalho central: constantes, enums, structs, banco de dados em memoria
 * e prototipos de todas as funcoes dos modulos.
 *
 * Convencoes:
 *  - Valores monetarios sempre em centavos inteiros (int64_t).
 *  - Datas em inteiro AAAAMMDD (int32_t), ex.: 20261028.
 *  - Funcoes de acao retornam ERP_OK (0) ou codigo negativo de erro;
 *    funcoes de criacao retornam o ID (> 0) ou codigo negativo de erro.
 */
#ifndef ERP_H
#define ERP_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Limites do banco em memoria                                         */
/* ------------------------------------------------------------------ */
#define MAX_CLIENTES      200
#define MAX_PRODUTOS      500
#define MAX_VENDAS        500
#define MAX_ITENS_VENDA   50
#define MAX_TITULOS       4000
#define MAX_PARCELAS      24
#define MAX_USUARIOS      50
#define TAM_LOGIN         32
#define TAM_NOME_USUARIO  64
#define TAM_SALT          16
#define TAM_HASH          32

#define TAM_NOME          80
#define TAM_DOC           15
#define TAM_EMAIL         60
#define TAM_CIDADE        40
#define TAM_UF            3
#define TAM_CODIGO        20
#define TAM_DESCRICAO     60
#define TAM_UNIDADE       5
#define TAM_CHAVE_NFE     45   /* 44 digitos + '\0' */
#define TAM_CODIGO_BARRAS 45   /* 44 digitos + '\0' */
#define TAM_LINHA_DIG     64   /* 47 digitos + separadores + '\0' */
#define TAM_MOEDA         40
#define TAM_DATA          11   /* dd/mm/aaaa + '\0' */

/* Arquivos de persistencia */
#define ARQ_CLIENTES "clientes.dat"
#define ARQ_PRODUTOS "produtos.dat"
#define ARQ_VENDAS   "vendas.dat"
#define ARQ_TITULOS  "titulos.dat"
#define ARQ_USUARIOS "usuarios.dat"
#define ARQ_AUDITORIA "auditoria.log"

/* ------------------------------------------------------------------ */
/* Codigos de retorno                                                  */
/* ------------------------------------------------------------------ */
#define ERP_OK                  0
#define ERP_ERR_PARAM          -1
#define ERP_ERR_LIMITE         -2
#define ERP_ERR_NAO_ENCONTRADO -3
#define ERP_ERR_ESTADO         -4
#define ERP_ERR_SALDO          -5
#define ERP_ERR_OVERFLOW       -6
#define ERP_ERR_DUPLICADO      -7
#define ERP_ERR_ARQUIVO        -8
#define ERP_ERR_DOCUMENTO      -9
#define ERP_ERR_CREDITO        -10
#define ERP_ERR_ACESSO         -11  /* credencial invalida ou sem permissao */
#define ERP_ERR_SENHA_FRACA    -12
#define ERP_ERR_BLOQUEADO      -13  /* muitas tentativas de login */

/* ------------------------------------------------------------------ */
/* Enums                                                               */
/* ------------------------------------------------------------------ */
typedef enum {
    PESSOA_FISICA   = 1,
    PESSOA_JURIDICA = 2
} TipoPessoa;

typedef enum {
    VENDA_ORCAMENTO = 0,
    VENDA_FATURADA  = 1,
    VENDA_CANCELADA = 2
} StatusVenda;

typedef enum {
    TITULO_RECEBER = 1,
    TITULO_PAGAR   = 2
} TipoTitulo;

typedef enum {
    TITULO_ABERTO    = 0,
    TITULO_LIQUIDADO = 1,
    TITULO_CANCELADO = 2
} StatusTitulo;

/* ------------------------------------------------------------------ */
/* Structs                                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    char    razao_social[TAM_NOME];
    char    nome_fantasia[TAM_NOME];
    char    cnpj[TAM_DOC];            /* 14 digitos */
    char    cidade[TAM_CIDADE];
    char    uf_sigla[TAM_UF];
    int32_t uf_codigo_ibge;           /* ex.: 35 = SP */
    int32_t modelo_nfe;               /* 55 = NF-e */
    int32_t serie_nfe;
    int32_t tipo_emissao;             /* 1 = normal */
} Empresa;

typedef struct {
    int32_t    id;
    TipoPessoa tipo;
    char       nome[TAM_NOME];
    char       documento[TAM_DOC];    /* somente digitos: CPF(11) ou CNPJ(14) */
    char       email[TAM_EMAIL];
    char       cidade[TAM_CIDADE];
    char       uf[TAM_UF];
    int64_t    limite_credito_centavos; /* 0 = sem limite definido */
    bool       ativo;
} Cliente;

typedef struct {
    int32_t id;
    char    codigo[TAM_CODIGO];
    char    descricao[TAM_DESCRICAO];
    char    unidade[TAM_UNIDADE];
    int64_t saldo_qtd;                /* unidades inteiras em estoque */
    int64_t cmp_centavos;             /* custo medio ponderado unitario */
    int64_t preco_venda_centavos;
    bool    ativo;
} Produto;

typedef struct {
    int32_t produto_id;
    int64_t quantidade;
    int64_t preco_unitario_centavos;
    int32_t desconto_bp;              /* desconto em pontos-base: 100 = 1,00% */
    int64_t desconto_centavos;
    int64_t subtotal_centavos;        /* bruto - desconto */
    int64_t custo_unitario_centavos;  /* CMP no momento do faturamento */
} VendaItem;

typedef struct {
    int32_t     id;
    int32_t     cliente_id;
    int32_t     data_emissao;         /* AAAAMMDD */
    StatusVenda status;
    int32_t     num_itens;
    VendaItem   itens[MAX_ITENS_VENDA];
    int64_t     total_bruto_centavos;
    int64_t     total_desconto_centavos;
    int64_t     total_liquido_centavos;
    int64_t     custo_total_centavos;
    int32_t     num_parcelas;
    int32_t     numero_nf;
    char        chave_nfe[TAM_CHAVE_NFE];
} Venda;

typedef struct {
    int32_t      id;
    TipoTitulo   tipo;
    StatusTitulo status;
    char         descricao[TAM_DESCRICAO];
    int32_t      entidade_id;         /* cliente (receber) ou fornecedor (pagar) */
    int32_t      venda_id;            /* 0 quando nao originado de venda */
    int32_t      parcela;
    int32_t      total_parcelas;
    int64_t      valor_centavos;
    int64_t      valor_pago_centavos;
    int32_t      data_emissao;
    int32_t      data_vencimento;
    int32_t      data_pagamento;      /* 0 enquanto nao houve pagamento */
} TituloFinanceiro;

/* ------------------------------------------------------------------ */
/* Controle de acesso                                                  */
/* ------------------------------------------------------------------ */
typedef enum {
    PERFIL_ADMIN      = 1,
    PERFIL_GERENTE    = 2,
    PERFIL_FISCAL     = 3,
    PERFIL_FINANCEIRO = 4
} PerfilAcesso;

typedef enum {
    MOD_DASHBOARD = 0,
    MOD_CLIENTES,
    MOD_ESTOQUE,
    MOD_VENDAS,
    MOD_FINANCEIRO,
    MOD_FISCAL,
    MOD_USUARIOS,
    MOD_CONFIG,
    MOD_TOTAL
} Modulo;

typedef enum {
    ACAO_VER    = 1,
    ACAO_EDITAR = 2
} AcaoAcesso;

typedef struct {
    int32_t      id;
    char         login[TAM_LOGIN];
    char         nome[TAM_NOME_USUARIO];
    PerfilAcesso perfil;
    uint8_t      salt[TAM_SALT];
    uint8_t      hash[TAM_HASH];   /* SHA-256 iterado sobre salt + senha */
    bool         ativo;
    bool         trocar_senha;     /* forca troca no proximo login */
    int32_t      ultimo_acesso;    /* AAAAMMDD, 0 = nunca */
} Usuario;

/* ------------------------------------------------------------------ */
/* Banco de dados em memoria                                           */
/* ------------------------------------------------------------------ */
extern Empresa          g_empresa;

extern Cliente          g_clientes[MAX_CLIENTES];
extern int32_t          g_num_clientes;
extern int32_t          g_prox_id_cliente;

extern Produto          g_produtos[MAX_PRODUTOS];
extern int32_t          g_num_produtos;
extern int32_t          g_prox_id_produto;

extern Venda            g_vendas[MAX_VENDAS];
extern int32_t          g_num_vendas;
extern int32_t          g_prox_id_venda;
extern int32_t          g_prox_numero_nf;

extern Usuario          g_usuarios[MAX_USUARIOS];
extern int32_t          g_num_usuarios;
extern int32_t          g_prox_id_usuario;

extern TituloFinanceiro g_titulos[MAX_TITULOS];
extern int32_t          g_num_titulos;
extern int32_t          g_prox_id_titulo;

/* ------------------------------------------------------------------ */
/* Utilitarios (cadastros.c)                                           */
/* ------------------------------------------------------------------ */
const char *erp_mensagem_erro(int codigo);
void   util_copiar_string(char *dest, size_t tam, const char *orig);
bool   util_ler_linha(const char *prompt, char *buf, size_t tam);
bool   util_ler_int64(const char *prompt, int64_t *out);
bool   util_ler_centavos(const char *prompt, int64_t *out);
bool   util_parse_int64(const char *txt, int64_t *out);
bool   util_parse_centavos(const char *txt, int64_t *out);
void   util_formatar_moeda(int64_t centavos, char *out, size_t tam);
bool   util_mul_seguro(int64_t a, int64_t b, int64_t *res);
bool   util_soma_segura(int64_t a, int64_t b, int64_t *res);
int64_t util_dividir_arredondado(int64_t numerador, int64_t denominador);

/* ------------------------------------------------------------------ */
/* Cadastros (cadastros.c)                                             */
/* ------------------------------------------------------------------ */
void     cadastros_inicializar_empresa_padrao(void);
void     cadastros_limpar(void);
int      cadastros_inserir_cliente(const Cliente *novo);
int      cadastros_inserir_produto(const Produto *novo);
Cliente *cadastros_buscar_cliente_id(int32_t id);
Produto *cadastros_buscar_produto_id(int32_t id);
Produto *cadastros_buscar_produto_codigo(const char *codigo);
void     cadastros_listar_clientes(void);
void     cadastros_listar_produtos(void);

/* ------------------------------------------------------------------ */
/* Estoque (estoque.c)                                                 */
/* ------------------------------------------------------------------ */
int64_t estoque_calcular_cmp(int64_t saldo_anterior, int64_t cmp_anterior,
                             int64_t qtd_entrada, int64_t valor_entrada_unit);
int     estoque_registrar_entrada(int32_t produto_id, int64_t qtd,
                                  int64_t valor_unit_centavos);
int     estoque_dar_baixa(int32_t produto_id, int64_t qtd);
void    estoque_relatorio(void);

/* ------------------------------------------------------------------ */
/* Fiscal e datas (fiscal.c)                                           */
/* ------------------------------------------------------------------ */
int  fiscal_somente_digitos(const char *origem, char *dest, size_t tam);
bool fiscal_validar_cpf(const char *cpf);
bool fiscal_validar_cnpj(const char *cnpj);

int  fiscal_calcular_dv_mod11_sefaz(const char *digitos);
int  fiscal_gerar_chave_nfe(int uf, int ano, int mes, const char *cnpj,
                            int modelo, int serie, int32_t numero,
                            int tipo_emissao, int32_t codigo_numerico,
                            char *out_chave, size_t tam);
bool fiscal_validar_chave_nfe(const char *chave);

int  fiscal_calcular_dv_mod10_febraban(const char *digitos);
int  fiscal_calcular_dv_mod11_boleto(const char *digitos);
int  fiscal_calcular_fator_vencimento(int32_t data_vencimento);
int  fiscal_gerar_linha_digitavel(const char *banco, char moeda,
                                  const char *campo_livre,
                                  int32_t data_vencimento,
                                  int64_t valor_centavos,
                                  char *out_linha, size_t tam_linha,
                                  char *out_barras, size_t tam_barras);
int  fiscal_linha_para_barras(const char *linha, char *out_barras, size_t tam);
bool fiscal_validar_linha_digitavel(const char *linha);

int32_t data_hoje(void);
bool    data_valida(int32_t data);
int64_t data_para_dias(int32_t data);
int32_t dias_para_data(int64_t dias);
int32_t data_somar_dias(int32_t data, int dias);
void    data_formatar(int32_t data, char *out, size_t tam);
bool    data_parse_br(const char *txt, int32_t *out); /* "" -> 0; dd/mm/aaaa */

/* ------------------------------------------------------------------ */
/* Vendas (vendas.c)                                                   */
/* ------------------------------------------------------------------ */
int    vendas_criar(int32_t cliente_id);
int    vendas_adicionar_item(int32_t venda_id, int32_t produto_id,
                             int64_t quantidade, int32_t desconto_bp);
int    vendas_faturar(int32_t venda_id, int num_parcelas,
                      int32_t data_primeiro_vencimento);
int    vendas_cancelar(int32_t venda_id);
Venda *vendas_buscar_id(int32_t id);
void   vendas_listar(void);
void   vendas_imprimir(const Venda *v);
void   vendas_imprimir_fp(FILE *fp, const Venda *v);

/* ------------------------------------------------------------------ */
/* Financeiro (financeiro.c)                                           */
/* ------------------------------------------------------------------ */
int    financeiro_incluir_titulo(TipoTitulo tipo, const char *descricao,
                                 int32_t entidade_id, int32_t venda_id,
                                 int32_t parcela, int32_t total_parcelas,
                                 int64_t valor_centavos, int32_t data_emissao,
                                 int32_t data_vencimento);
int    financeiro_baixar_titulo(int32_t titulo_id, int64_t valor_pago_centavos,
                                int32_t data_pagamento);
TituloFinanceiro *financeiro_buscar_id(int32_t id);
int64_t financeiro_saldo_titulo(const TituloFinanceiro *t);
int64_t financeiro_total_aberto(TipoTitulo tipo);
int64_t financeiro_saldo_devedor_cliente(int32_t cliente_id);
void   financeiro_listar(int tipo_filtro);
void   financeiro_relatorio_fluxo_caixa(void);
int    financeiro_gerar_boleto(int32_t titulo_id, char *linha, size_t tam_linha,
                               char *barras, size_t tam_barras);
int    financeiro_emitir_boleto(int32_t titulo_id);

/* ------------------------------------------------------------------ */
/* Autenticacao e autorizacao (auth.c)                                 */
/* ------------------------------------------------------------------ */
void        auth_sha256(const uint8_t *dados, size_t tam, uint8_t out[32]);
void        auth_derivar_hash(const char *senha, const uint8_t salt[TAM_SALT],
                              uint8_t out[TAM_HASH]);
int         auth_inicializar(void);
int         auth_salvar(void);
int         auth_login(const char *login, const char *senha, Usuario **out);
bool        auth_verificar_senha(const Usuario *u, const char *senha);
int         auth_criar_usuario(const char *login, const char *nome,
                               const char *senha, PerfilAcesso perfil);
int         auth_trocar_senha(int32_t id, const char *nova, bool exigir_troca);
int         auth_definir_ativo(int32_t id, bool ativo);
int         auth_alterar_perfil(int32_t id, PerfilAcesso perfil);
Usuario    *auth_buscar_id(int32_t id);
Usuario    *auth_buscar_login(const char *login);
bool        auth_pode(PerfilAcesso perfil, Modulo modulo, AcaoAcesso acao);
const char *auth_nome_perfil(PerfilAcesso perfil);
const char *auth_nome_modulo(Modulo modulo);
void        auth_log(const char *usuario, const char *evento);

/* ------------------------------------------------------------------ */
/* SQL Server via ODBC (sql_server.c) - opcional, make SQL=1           */
/* So existem quando compilado com -DUSE_SQLSERVER.                    */
/* ------------------------------------------------------------------ */
#ifdef USE_SQLSERVER
int  sql_conectar(const char *connstr);
void sql_desconectar(void);
int  sql_garantir_schema(void);
int  sql_salvar_dados(void);
int  sql_carregar_dados(void);
#endif

/* ------------------------------------------------------------------ */
/* Persistencia (persistencia.c)                                       */
/* ------------------------------------------------------------------ */
int  persistencia_salvar_dados(void);
int  persistencia_carregar_dados(void);
void sistema_inicializar_dados_padrao(void);

#endif /* ERP_H */
