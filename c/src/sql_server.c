/*
 * Aurora ERP - sql_server.c  (MODULO OPCIONAL)
 *
 * Sincroniza o banco em memoria com o Microsoft SQL Server via ODBC.
 * So e compilado com:  make SQL=1   (define -DUSE_SQLSERVER e liga -lodbc)
 * Sem essa opcao o arquivo fica vazio e o projeto continua 100% ISO C.
 *
 * Conexao: variavel de ambiente AURORA_SQL_CONN, por exemplo:
 *   export AURORA_SQL_CONN="Driver={ODBC Driver 18 for SQL Server};Server=localhost,1433;UID=sa;PWD=SuaSenhaForte;TrustServerCertificate=yes;"
 *
 * Estrategia: "salvar" grava tudo numa unica transacao (apaga e reinsere);
 * "carregar" le tudo para buffers temporarios e so substitui a memoria se
 * a leitura inteira funcionar. Valores monetarios ficam em BIGINT (centavos).
 */
#include "erp.h"

#ifdef USE_SQLSERVER

#include <sql.h>
#include <sqlext.h>

static SQLHENV g_env = SQL_NULL_HENV;
static SQLHDBC g_dbc = SQL_NULL_HDBC;

#define SQL_OK(rc) ((rc) == SQL_SUCCESS || (rc) == SQL_SUCCESS_WITH_INFO)

static void sql_diag(SQLSMALLINT tipo, SQLHANDLE h, const char *contexto)
{
    SQLCHAR estado[8];
    SQLCHAR msg[512];
    SQLINTEGER nativo;
    SQLSMALLINT len;
    SQLSMALLINT i = 1;

    fprintf(stderr, "[SQL] Falha em %s\n", contexto);
    while (SQLGetDiagRec(tipo, h, i, estado, &nativo, msg, sizeof msg, &len) ==
           SQL_SUCCESS) {
        fprintf(stderr, "[SQL]   %s (%ld): %s\n", (char *)estado, (long)nativo,
                (char *)msg);
        i++;
    }
}

/* ------------------------------------------------------------------ */
/* Conexao                                                             */
/* ------------------------------------------------------------------ */
void sql_desconectar(void)
{
    if (g_dbc != SQL_NULL_HDBC) {
        SQLDisconnect(g_dbc);
        SQLFreeHandle(SQL_HANDLE_DBC, g_dbc);
        g_dbc = SQL_NULL_HDBC;
    }
    if (g_env != SQL_NULL_HENV) {
        SQLFreeHandle(SQL_HANDLE_ENV, g_env);
        g_env = SQL_NULL_HENV;
    }
}

int sql_conectar(const char *connstr)
{
    SQLRETURN rc;
    SQLCHAR saida[1024];
    SQLSMALLINT saida_len;

    if (g_dbc != SQL_NULL_HDBC) {
        return ERP_OK;
    }
    if (connstr == NULL || connstr[0] == '\0') {
        connstr = getenv("AURORA_SQL_CONN");
    }
    if (connstr == NULL || connstr[0] == '\0') {
        fprintf(stderr, "[SQL] Defina a variavel de ambiente AURORA_SQL_CONN.\n");
        return ERP_ERR_PARAM;
    }
    rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &g_env);
    if (!SQL_OK(rc)) {
        return ERP_ERR_ARQUIVO;
    }
    SQLSetEnvAttr(g_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)(intptr_t)SQL_OV_ODBC3, 0);
    rc = SQLAllocHandle(SQL_HANDLE_DBC, g_env, &g_dbc);
    if (!SQL_OK(rc)) {
        sql_desconectar();
        return ERP_ERR_ARQUIVO;
    }
    rc = SQLDriverConnect(g_dbc, NULL, (SQLCHAR *)connstr, SQL_NTS, saida,
                          (SQLSMALLINT)sizeof saida, &saida_len, SQL_DRIVER_NOPROMPT);
    if (!SQL_OK(rc)) {
        sql_diag(SQL_HANDLE_DBC, g_dbc, "SQLDriverConnect");
        sql_desconectar();
        return ERP_ERR_ARQUIVO;
    }
    return ERP_OK;
}

static int sql_exec_direto(const char *sql)
{
    SQLHSTMT st;
    SQLRETURN rc;

    if (!SQL_OK(SQLAllocHandle(SQL_HANDLE_STMT, g_dbc, &st))) {
        return ERP_ERR_ARQUIVO;
    }
    rc = SQLExecDirect(st, (SQLCHAR *)sql, SQL_NTS);
    if (!SQL_OK(rc) && rc != SQL_NO_DATA) {
        sql_diag(SQL_HANDLE_STMT, st, sql);
        SQLFreeHandle(SQL_HANDLE_STMT, st);
        return ERP_ERR_ARQUIVO;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    return ERP_OK;
}

/* Cria o banco AuroraERP e as tabelas, se ainda nao existirem. */
int sql_garantir_schema(void)
{
    static const char *comandos[] = {
        "IF DB_ID(N'AuroraERP') IS NULL CREATE DATABASE AuroraERP",
        "USE AuroraERP",
        "IF OBJECT_ID(N'dbo.clientes', N'U') IS NULL CREATE TABLE dbo.clientes ("
        " id INT NOT NULL PRIMARY KEY, tipo INT NOT NULL,"
        " nome VARCHAR(80) NOT NULL, documento VARCHAR(14) NOT NULL UNIQUE,"
        " email VARCHAR(60) NOT NULL, cidade VARCHAR(40) NOT NULL,"
        " uf VARCHAR(2) NOT NULL, limite_credito_centavos BIGINT NOT NULL,"
        " ativo BIT NOT NULL)",
        "IF OBJECT_ID(N'dbo.produtos', N'U') IS NULL CREATE TABLE dbo.produtos ("
        " id INT NOT NULL PRIMARY KEY, codigo VARCHAR(20) NOT NULL UNIQUE,"
        " descricao VARCHAR(60) NOT NULL, unidade VARCHAR(5) NOT NULL,"
        " saldo_qtd BIGINT NOT NULL, cmp_centavos BIGINT NOT NULL,"
        " preco_venda_centavos BIGINT NOT NULL, ativo BIT NOT NULL)",
        "IF OBJECT_ID(N'dbo.vendas', N'U') IS NULL CREATE TABLE dbo.vendas ("
        " id INT NOT NULL PRIMARY KEY,"
        " cliente_id INT NOT NULL REFERENCES dbo.clientes(id),"
        " data_emissao DATE NOT NULL, status INT NOT NULL,"
        " total_bruto_centavos BIGINT NOT NULL,"
        " total_desconto_centavos BIGINT NOT NULL,"
        " total_liquido_centavos BIGINT NOT NULL,"
        " custo_total_centavos BIGINT NOT NULL, num_parcelas INT NOT NULL,"
        " numero_nf INT NOT NULL, chave_nfe VARCHAR(44) NOT NULL)",
        "IF OBJECT_ID(N'dbo.venda_itens', N'U') IS NULL CREATE TABLE dbo.venda_itens ("
        " venda_id INT NOT NULL REFERENCES dbo.vendas(id), item_num INT NOT NULL,"
        " produto_id INT NOT NULL REFERENCES dbo.produtos(id),"
        " quantidade BIGINT NOT NULL, preco_unitario_centavos BIGINT NOT NULL,"
        " desconto_bp INT NOT NULL, desconto_centavos BIGINT NOT NULL,"
        " subtotal_centavos BIGINT NOT NULL, custo_unitario_centavos BIGINT NOT NULL,"
        " PRIMARY KEY (venda_id, item_num))",
        "IF OBJECT_ID(N'dbo.titulos', N'U') IS NULL CREATE TABLE dbo.titulos ("
        " id INT NOT NULL PRIMARY KEY, tipo INT NOT NULL, status INT NOT NULL,"
        " descricao VARCHAR(60) NOT NULL, entidade_id INT NOT NULL,"
        " venda_id INT NOT NULL, parcela INT NOT NULL, total_parcelas INT NOT NULL,"
        " valor_centavos BIGINT NOT NULL, valor_pago_centavos BIGINT NOT NULL,"
        " data_emissao DATE NOT NULL, data_vencimento DATE NOT NULL,"
        " data_pagamento DATE NULL)"
    };
    size_t i;
    int rc;

    for (i = 0; i < sizeof comandos / sizeof comandos[0]; i++) {
        rc = sql_exec_direto(comandos[i]);
        if (rc != ERP_OK) {
            return rc;
        }
    }
    return ERP_OK;
}

/* ------------------------------------------------------------------ */
/* Auxiliares de bind / leitura                                        */
/* ------------------------------------------------------------------ */
static void bind_i32(SQLHSTMT st, SQLUSMALLINT n, SQLINTEGER *v)
{
    SQLBindParameter(st, n, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, v, 0, NULL);
}

static void bind_i64(SQLHSTMT st, SQLUSMALLINT n, SQLBIGINT *v)
{
    SQLBindParameter(st, n, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 0, 0, v, 0, NULL);
}

static void bind_txt(SQLHSTMT st, SQLUSMALLINT n, char *buf, SQLLEN *ind)
{
    SQLBindParameter(st, n, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 255, 0, buf, 0, ind);
}

static void bind_data(SQLHSTMT st, SQLUSMALLINT n, char *buf, SQLLEN *ind)
{
    SQLBindParameter(st, n, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_TYPE_DATE, 10, 0, buf, 0, ind);
}

static void data_para_txt(int32_t d, char *out)
{
    snprintf(out, 16, "%04d-%02d-%02d", (int)((d / 10000) % 10000),
             (int)((d / 100) % 100), (int)(d % 100));
}

static int32_t txt_para_data(const char *s)
{
    int a;
    int m;
    int d;

    if (sscanf(s, "%d-%d-%d", &a, &m, &d) != 3) {
        return 0;
    }
    return (int32_t)(a * 10000 + m * 100 + d);
}

static SQLINTEGER ler_i32(SQLHSTMT st, SQLUSMALLINT col)
{
    SQLINTEGER v = 0;
    SQLLEN ind = 0;

    SQLGetData(st, col, SQL_C_SLONG, &v, 0, &ind);
    return (ind == SQL_NULL_DATA) ? 0 : v;
}

static SQLBIGINT ler_i64(SQLHSTMT st, SQLUSMALLINT col)
{
    SQLBIGINT v = 0;
    SQLLEN ind = 0;

    SQLGetData(st, col, SQL_C_SBIGINT, &v, 0, &ind);
    return (ind == SQL_NULL_DATA) ? 0 : v;
}

static void ler_txt(SQLHSTMT st, SQLUSMALLINT col, char *dest, size_t tam)
{
    SQLLEN ind = 0;

    dest[0] = '\0';
    if (!SQL_OK(SQLGetData(st, col, SQL_C_CHAR, dest, (SQLLEN)tam, &ind)) ||
        ind == SQL_NULL_DATA) {
        dest[0] = '\0';
    }
}

static int32_t ler_data(SQLHSTMT st, SQLUSMALLINT col)
{
    char buf[16];

    ler_txt(st, col, buf, sizeof buf);
    return buf[0] == '\0' ? 0 : txt_para_data(buf);
}

/* ------------------------------------------------------------------ */
/* Salvar                                                              */
/* ------------------------------------------------------------------ */
static int salvar_clientes(void)
{
    SQLHSTMT st;
    SQLINTEGER id, tipo, ativo;
    SQLBIGINT limite;
    char nome[TAM_NOME], doc[TAM_DOC], email[TAM_EMAIL], cidade[TAM_CIDADE], uf[TAM_UF];
    SQLLEN ind = SQL_NTS;
    int32_t i;

    if (!SQL_OK(SQLAllocHandle(SQL_HANDLE_STMT, g_dbc, &st))) {
        return ERP_ERR_ARQUIVO;
    }
    if (!SQL_OK(SQLPrepare(st, (SQLCHAR *)"INSERT INTO dbo.clientes (id,tipo,nome,"
                           "documento,email,cidade,uf,limite_credito_centavos,ativo)"
                           " VALUES (?,?,?,?,?,?,?,?,?)", SQL_NTS))) {
        sql_diag(SQL_HANDLE_STMT, st, "prepare clientes");
        SQLFreeHandle(SQL_HANDLE_STMT, st);
        return ERP_ERR_ARQUIVO;
    }
    bind_i32(st, 1, &id);   bind_i32(st, 2, &tipo);
    bind_txt(st, 3, nome, &ind);   bind_txt(st, 4, doc, &ind);
    bind_txt(st, 5, email, &ind);  bind_txt(st, 6, cidade, &ind);
    bind_txt(st, 7, uf, &ind);     bind_i64(st, 8, &limite);
    bind_i32(st, 9, &ativo);
    for (i = 0; i < g_num_clientes; i++) {
        const Cliente *c = &g_clientes[i];

        id = c->id; tipo = (SQLINTEGER)c->tipo; ativo = c->ativo ? 1 : 0;
        limite = c->limite_credito_centavos;
        util_copiar_string(nome, sizeof nome, c->nome);
        util_copiar_string(doc, sizeof doc, c->documento);
        util_copiar_string(email, sizeof email, c->email);
        util_copiar_string(cidade, sizeof cidade, c->cidade);
        util_copiar_string(uf, sizeof uf, c->uf);
        if (!SQL_OK(SQLExecute(st))) {
            sql_diag(SQL_HANDLE_STMT, st, "insert clientes");
            SQLFreeHandle(SQL_HANDLE_STMT, st);
            return ERP_ERR_ARQUIVO;
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    return ERP_OK;
}

static int salvar_produtos(void)
{
    SQLHSTMT st;
    SQLINTEGER id, ativo;
    SQLBIGINT saldo, cmp, preco;
    char codigo[TAM_CODIGO], desc[TAM_DESCRICAO], un[TAM_UNIDADE];
    SQLLEN ind = SQL_NTS;
    int32_t i;

    if (!SQL_OK(SQLAllocHandle(SQL_HANDLE_STMT, g_dbc, &st))) {
        return ERP_ERR_ARQUIVO;
    }
    if (!SQL_OK(SQLPrepare(st, (SQLCHAR *)"INSERT INTO dbo.produtos (id,codigo,"
                           "descricao,unidade,saldo_qtd,cmp_centavos,"
                           "preco_venda_centavos,ativo) VALUES (?,?,?,?,?,?,?,?)",
                           SQL_NTS))) {
        sql_diag(SQL_HANDLE_STMT, st, "prepare produtos");
        SQLFreeHandle(SQL_HANDLE_STMT, st);
        return ERP_ERR_ARQUIVO;
    }
    bind_i32(st, 1, &id);  bind_txt(st, 2, codigo, &ind);
    bind_txt(st, 3, desc, &ind);  bind_txt(st, 4, un, &ind);
    bind_i64(st, 5, &saldo);  bind_i64(st, 6, &cmp);
    bind_i64(st, 7, &preco);  bind_i32(st, 8, &ativo);
    for (i = 0; i < g_num_produtos; i++) {
        const Produto *p = &g_produtos[i];

        id = p->id; ativo = p->ativo ? 1 : 0;
        saldo = p->saldo_qtd; cmp = p->cmp_centavos; preco = p->preco_venda_centavos;
        util_copiar_string(codigo, sizeof codigo, p->codigo);
        util_copiar_string(desc, sizeof desc, p->descricao);
        util_copiar_string(un, sizeof un, p->unidade);
        if (!SQL_OK(SQLExecute(st))) {
            sql_diag(SQL_HANDLE_STMT, st, "insert produtos");
            SQLFreeHandle(SQL_HANDLE_STMT, st);
            return ERP_ERR_ARQUIVO;
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    return ERP_OK;
}

static int salvar_vendas(void)
{
    SQLHSTMT sv;
    SQLHSTMT si;
    SQLINTEGER id, cliente, status, parcelas, nf, item_num, prod, desc_bp;
    SQLBIGINT bruto, desc, liq, custo, qtd, preco, desc_c, subt, custo_u;
    char data[16], chave[TAM_CHAVE_NFE];
    SQLLEN ind_txt = SQL_NTS;
    int32_t i;
    int32_t j;

    if (!SQL_OK(SQLAllocHandle(SQL_HANDLE_STMT, g_dbc, &sv))) {
        return ERP_ERR_ARQUIVO;
    }
    if (!SQL_OK(SQLAllocHandle(SQL_HANDLE_STMT, g_dbc, &si))) {
        SQLFreeHandle(SQL_HANDLE_STMT, sv);
        return ERP_ERR_ARQUIVO;
    }
    if (!SQL_OK(SQLPrepare(sv, (SQLCHAR *)"INSERT INTO dbo.vendas (id,cliente_id,"
                           "data_emissao,status,total_bruto_centavos,"
                           "total_desconto_centavos,total_liquido_centavos,"
                           "custo_total_centavos,num_parcelas,numero_nf,chave_nfe)"
                           " VALUES (?,?,?,?,?,?,?,?,?,?,?)", SQL_NTS)) ||
        !SQL_OK(SQLPrepare(si, (SQLCHAR *)"INSERT INTO dbo.venda_itens (venda_id,"
                           "item_num,produto_id,quantidade,preco_unitario_centavos,"
                           "desconto_bp,desconto_centavos,subtotal_centavos,"
                           "custo_unitario_centavos) VALUES (?,?,?,?,?,?,?,?,?)",
                           SQL_NTS))) {
        sql_diag(SQL_HANDLE_STMT, sv, "prepare vendas");
        SQLFreeHandle(SQL_HANDLE_STMT, sv);
        SQLFreeHandle(SQL_HANDLE_STMT, si);
        return ERP_ERR_ARQUIVO;
    }
    bind_i32(sv, 1, &id);  bind_i32(sv, 2, &cliente);
    bind_data(sv, 3, data, &ind_txt);  bind_i32(sv, 4, &status);
    bind_i64(sv, 5, &bruto);  bind_i64(sv, 6, &desc);
    bind_i64(sv, 7, &liq);  bind_i64(sv, 8, &custo);
    bind_i32(sv, 9, &parcelas);  bind_i32(sv, 10, &nf);
    bind_txt(sv, 11, chave, &ind_txt);

    bind_i32(si, 1, &id);  bind_i32(si, 2, &item_num);
    bind_i32(si, 3, &prod);  bind_i64(si, 4, &qtd);
    bind_i64(si, 5, &preco);  bind_i32(si, 6, &desc_bp);
    bind_i64(si, 7, &desc_c);  bind_i64(si, 8, &subt);
    bind_i64(si, 9, &custo_u);

    for (i = 0; i < g_num_vendas; i++) {
        const Venda *v = &g_vendas[i];

        id = v->id; cliente = v->cliente_id; status = (SQLINTEGER)v->status;
        data_para_txt(v->data_emissao, data);
        bruto = v->total_bruto_centavos; desc = v->total_desconto_centavos;
        liq = v->total_liquido_centavos; custo = v->custo_total_centavos;
        parcelas = v->num_parcelas; nf = v->numero_nf;
        util_copiar_string(chave, sizeof chave, v->chave_nfe);
        if (!SQL_OK(SQLExecute(sv))) {
            sql_diag(SQL_HANDLE_STMT, sv, "insert vendas");
            SQLFreeHandle(SQL_HANDLE_STMT, sv);
            SQLFreeHandle(SQL_HANDLE_STMT, si);
            return ERP_ERR_ARQUIVO;
        }
        for (j = 0; j < v->num_itens; j++) {
            const VendaItem *it = &v->itens[j];

            item_num = j + 1; prod = it->produto_id; qtd = it->quantidade;
            preco = it->preco_unitario_centavos; desc_bp = it->desconto_bp;
            desc_c = it->desconto_centavos; subt = it->subtotal_centavos;
            custo_u = it->custo_unitario_centavos;
            if (!SQL_OK(SQLExecute(si))) {
                sql_diag(SQL_HANDLE_STMT, si, "insert venda_itens");
                SQLFreeHandle(SQL_HANDLE_STMT, sv);
                SQLFreeHandle(SQL_HANDLE_STMT, si);
                return ERP_ERR_ARQUIVO;
            }
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, sv);
    SQLFreeHandle(SQL_HANDLE_STMT, si);
    return ERP_OK;
}

static int salvar_titulos(void)
{
    SQLHSTMT st;
    SQLINTEGER id, tipo, status, ent, venda, parc, tparc;
    SQLBIGINT valor, pago;
    char desc[TAM_DESCRICAO], emissao[16], venc[16], pgto[16];
    SQLLEN ind_txt = SQL_NTS;
    SQLLEN ind_pgto = SQL_NTS;
    int32_t i;

    if (!SQL_OK(SQLAllocHandle(SQL_HANDLE_STMT, g_dbc, &st))) {
        return ERP_ERR_ARQUIVO;
    }
    if (!SQL_OK(SQLPrepare(st, (SQLCHAR *)"INSERT INTO dbo.titulos (id,tipo,status,"
                           "descricao,entidade_id,venda_id,parcela,total_parcelas,"
                           "valor_centavos,valor_pago_centavos,data_emissao,"
                           "data_vencimento,data_pagamento)"
                           " VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)", SQL_NTS))) {
        sql_diag(SQL_HANDLE_STMT, st, "prepare titulos");
        SQLFreeHandle(SQL_HANDLE_STMT, st);
        return ERP_ERR_ARQUIVO;
    }
    bind_i32(st, 1, &id);  bind_i32(st, 2, &tipo);  bind_i32(st, 3, &status);
    bind_txt(st, 4, desc, &ind_txt);
    bind_i32(st, 5, &ent);  bind_i32(st, 6, &venda);
    bind_i32(st, 7, &parc);  bind_i32(st, 8, &tparc);
    bind_i64(st, 9, &valor);  bind_i64(st, 10, &pago);
    bind_data(st, 11, emissao, &ind_txt);
    bind_data(st, 12, venc, &ind_txt);
    bind_data(st, 13, pgto, &ind_pgto);
    for (i = 0; i < g_num_titulos; i++) {
        const TituloFinanceiro *t = &g_titulos[i];

        id = t->id; tipo = (SQLINTEGER)t->tipo; status = (SQLINTEGER)t->status;
        util_copiar_string(desc, sizeof desc, t->descricao);
        ent = t->entidade_id; venda = t->venda_id;
        parc = t->parcela; tparc = t->total_parcelas;
        valor = t->valor_centavos; pago = t->valor_pago_centavos;
        data_para_txt(t->data_emissao, emissao);
        data_para_txt(t->data_vencimento, venc);
        if (t->data_pagamento > 0) {
            data_para_txt(t->data_pagamento, pgto);
            ind_pgto = SQL_NTS;
        } else {
            pgto[0] = '\0';
            ind_pgto = SQL_NULL_DATA;
        }
        if (!SQL_OK(SQLExecute(st))) {
            sql_diag(SQL_HANDLE_STMT, st, "insert titulos");
            SQLFreeHandle(SQL_HANDLE_STMT, st);
            return ERP_ERR_ARQUIVO;
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    return ERP_OK;
}

int sql_salvar_dados(void)
{
    int rc;

    rc = sql_conectar(NULL);
    if (rc != ERP_OK) {
        return rc;
    }
    rc = sql_garantir_schema();   /* DDL fora da transacao */
    if (rc != ERP_OK) {
        return rc;
    }
    SQLSetConnectAttr(g_dbc, SQL_ATTR_AUTOCOMMIT,
                      (SQLPOINTER)(intptr_t)SQL_AUTOCOMMIT_OFF, 0);
    rc = sql_exec_direto("DELETE FROM dbo.venda_itens");
    if (rc == ERP_OK) rc = sql_exec_direto("DELETE FROM dbo.titulos");
    if (rc == ERP_OK) rc = sql_exec_direto("DELETE FROM dbo.vendas");
    if (rc == ERP_OK) rc = sql_exec_direto("DELETE FROM dbo.produtos");
    if (rc == ERP_OK) rc = sql_exec_direto("DELETE FROM dbo.clientes");
    if (rc == ERP_OK) rc = salvar_clientes();
    if (rc == ERP_OK) rc = salvar_produtos();
    if (rc == ERP_OK) rc = salvar_vendas();
    if (rc == ERP_OK) rc = salvar_titulos();
    SQLEndTran(SQL_HANDLE_DBC, g_dbc, rc == ERP_OK ? SQL_COMMIT : SQL_ROLLBACK);
    SQLSetConnectAttr(g_dbc, SQL_ATTR_AUTOCOMMIT,
                      (SQLPOINTER)(intptr_t)SQL_AUTOCOMMIT_ON, 0);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Carregar                                                            */
/* ------------------------------------------------------------------ */
static int abrir_select(SQLHSTMT *st, const char *sql)
{
    if (!SQL_OK(SQLAllocHandle(SQL_HANDLE_STMT, g_dbc, st))) {
        return ERP_ERR_ARQUIVO;
    }
    if (!SQL_OK(SQLExecDirect(*st, (SQLCHAR *)sql, SQL_NTS))) {
        sql_diag(SQL_HANDLE_STMT, *st, sql);
        SQLFreeHandle(SQL_HANDLE_STMT, *st);
        return ERP_ERR_ARQUIVO;
    }
    return ERP_OK;
}

int sql_carregar_dados(void)
{
    Cliente *tc = NULL;
    Produto *tp = NULL;
    Venda *tv = NULL;
    TituloFinanceiro *tt = NULL;
    int32_t nc = 0;
    int32_t np = 0;
    int32_t nv = 0;
    int32_t nt = 0;
    int32_t i;
    SQLHSTMT st;
    int rc;

    rc = sql_conectar(NULL);
    if (rc != ERP_OK) {
        return rc;
    }
    if (sql_exec_direto("USE AuroraERP") != ERP_OK) {
        return ERP_ERR_ARQUIVO;
    }
    tc = calloc(MAX_CLIENTES, sizeof *tc);
    tp = calloc(MAX_PRODUTOS, sizeof *tp);
    tv = calloc(MAX_VENDAS, sizeof *tv);
    tt = calloc(MAX_TITULOS, sizeof *tt);
    if (tc == NULL || tp == NULL || tv == NULL || tt == NULL) {
        rc = ERP_ERR_ARQUIVO;
        goto fim;
    }

    /* clientes */
    rc = abrir_select(&st, "SELECT id,tipo,nome,documento,email,cidade,uf,"
                           "limite_credito_centavos,ativo FROM dbo.clientes ORDER BY id");
    if (rc != ERP_OK) goto fim;
    while (SQL_OK(SQLFetch(st))) {
        Cliente *c;

        if (nc >= MAX_CLIENTES) { rc = ERP_ERR_LIMITE; break; }
        c = &tc[nc++];
        c->id = ler_i32(st, 1); c->tipo = (TipoPessoa)ler_i32(st, 2);
        ler_txt(st, 3, c->nome, sizeof c->nome);
        ler_txt(st, 4, c->documento, sizeof c->documento);
        ler_txt(st, 5, c->email, sizeof c->email);
        ler_txt(st, 6, c->cidade, sizeof c->cidade);
        ler_txt(st, 7, c->uf, sizeof c->uf);
        c->limite_credito_centavos = ler_i64(st, 8);
        c->ativo = ler_i32(st, 9) != 0;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    if (rc != ERP_OK) goto fim;

    /* produtos */
    rc = abrir_select(&st, "SELECT id,codigo,descricao,unidade,saldo_qtd,"
                           "cmp_centavos,preco_venda_centavos,ativo "
                           "FROM dbo.produtos ORDER BY id");
    if (rc != ERP_OK) goto fim;
    while (SQL_OK(SQLFetch(st))) {
        Produto *p;

        if (np >= MAX_PRODUTOS) { rc = ERP_ERR_LIMITE; break; }
        p = &tp[np++];
        p->id = ler_i32(st, 1);
        ler_txt(st, 2, p->codigo, sizeof p->codigo);
        ler_txt(st, 3, p->descricao, sizeof p->descricao);
        ler_txt(st, 4, p->unidade, sizeof p->unidade);
        p->saldo_qtd = ler_i64(st, 5); p->cmp_centavos = ler_i64(st, 6);
        p->preco_venda_centavos = ler_i64(st, 7);
        p->ativo = ler_i32(st, 8) != 0;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    if (rc != ERP_OK) goto fim;

    /* vendas */
    rc = abrir_select(&st, "SELECT id,cliente_id,CONVERT(VARCHAR(10),data_emissao,23),"
                           "status,total_bruto_centavos,total_desconto_centavos,"
                           "total_liquido_centavos,custo_total_centavos,num_parcelas,"
                           "numero_nf,chave_nfe FROM dbo.vendas ORDER BY id");
    if (rc != ERP_OK) goto fim;
    while (SQL_OK(SQLFetch(st))) {
        Venda *v;

        if (nv >= MAX_VENDAS) { rc = ERP_ERR_LIMITE; break; }
        v = &tv[nv++];
        v->id = ler_i32(st, 1); v->cliente_id = ler_i32(st, 2);
        v->data_emissao = ler_data(st, 3);
        v->status = (StatusVenda)ler_i32(st, 4);
        v->total_bruto_centavos = ler_i64(st, 5);
        v->total_desconto_centavos = ler_i64(st, 6);
        v->total_liquido_centavos = ler_i64(st, 7);
        v->custo_total_centavos = ler_i64(st, 8);
        v->num_parcelas = ler_i32(st, 9); v->numero_nf = ler_i32(st, 10);
        ler_txt(st, 11, v->chave_nfe, sizeof v->chave_nfe);
    }
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    if (rc != ERP_OK) goto fim;

    /* itens das vendas */
    rc = abrir_select(&st, "SELECT venda_id,produto_id,quantidade,"
                           "preco_unitario_centavos,desconto_bp,desconto_centavos,"
                           "subtotal_centavos,custo_unitario_centavos "
                           "FROM dbo.venda_itens ORDER BY venda_id,item_num");
    if (rc != ERP_OK) goto fim;
    while (SQL_OK(SQLFetch(st))) {
        int32_t vid = ler_i32(st, 1);
        Venda *v = NULL;
        VendaItem *it;

        for (i = 0; i < nv; i++) {
            if (tv[i].id == vid) { v = &tv[i]; break; }
        }
        if (v == NULL || v->num_itens >= MAX_ITENS_VENDA) { rc = ERP_ERR_LIMITE; break; }
        it = &v->itens[v->num_itens++];
        it->produto_id = ler_i32(st, 2); it->quantidade = ler_i64(st, 3);
        it->preco_unitario_centavos = ler_i64(st, 4);
        it->desconto_bp = ler_i32(st, 5); it->desconto_centavos = ler_i64(st, 6);
        it->subtotal_centavos = ler_i64(st, 7);
        it->custo_unitario_centavos = ler_i64(st, 8);
    }
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    if (rc != ERP_OK) goto fim;

    /* titulos */
    rc = abrir_select(&st, "SELECT id,tipo,status,descricao,entidade_id,venda_id,"
                           "parcela,total_parcelas,valor_centavos,valor_pago_centavos,"
                           "CONVERT(VARCHAR(10),data_emissao,23),"
                           "CONVERT(VARCHAR(10),data_vencimento,23),"
                           "CONVERT(VARCHAR(10),data_pagamento,23) "
                           "FROM dbo.titulos ORDER BY id");
    if (rc != ERP_OK) goto fim;
    while (SQL_OK(SQLFetch(st))) {
        TituloFinanceiro *t;

        if (nt >= MAX_TITULOS) { rc = ERP_ERR_LIMITE; break; }
        t = &tt[nt++];
        t->id = ler_i32(st, 1); t->tipo = (TipoTitulo)ler_i32(st, 2);
        t->status = (StatusTitulo)ler_i32(st, 3);
        ler_txt(st, 4, t->descricao, sizeof t->descricao);
        t->entidade_id = ler_i32(st, 5); t->venda_id = ler_i32(st, 6);
        t->parcela = ler_i32(st, 7); t->total_parcelas = ler_i32(st, 8);
        t->valor_centavos = ler_i64(st, 9); t->valor_pago_centavos = ler_i64(st, 10);
        t->data_emissao = ler_data(st, 11); t->data_vencimento = ler_data(st, 12);
        t->data_pagamento = ler_data(st, 13);
    }
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    if (rc != ERP_OK) goto fim;

    /* Tudo lido: substitui o banco em memoria */
    memset(g_clientes, 0, sizeof g_clientes);
    memset(g_produtos, 0, sizeof g_produtos);
    memset(g_vendas, 0, sizeof g_vendas);
    memset(g_titulos, 0, sizeof g_titulos);
    memcpy(g_clientes, tc, sizeof(Cliente) * (size_t)nc);
    memcpy(g_produtos, tp, sizeof(Produto) * (size_t)np);
    memcpy(g_vendas, tv, sizeof(Venda) * (size_t)nv);
    memcpy(g_titulos, tt, sizeof(TituloFinanceiro) * (size_t)nt);
    g_num_clientes = nc; g_num_produtos = np;
    g_num_vendas = nv;   g_num_titulos = nt;
    g_prox_id_cliente = g_prox_id_produto = g_prox_id_venda = 1;
    g_prox_id_titulo = g_prox_numero_nf = 1;
    for (i = 0; i < nc; i++) if (g_clientes[i].id >= g_prox_id_cliente) g_prox_id_cliente = g_clientes[i].id + 1;
    for (i = 0; i < np; i++) if (g_produtos[i].id >= g_prox_id_produto) g_prox_id_produto = g_produtos[i].id + 1;
    for (i = 0; i < nv; i++) {
        if (g_vendas[i].id >= g_prox_id_venda) g_prox_id_venda = g_vendas[i].id + 1;
        if (g_vendas[i].numero_nf >= g_prox_numero_nf) g_prox_numero_nf = g_vendas[i].numero_nf + 1;
    }
    for (i = 0; i < nt; i++) if (g_titulos[i].id >= g_prox_id_titulo) g_prox_id_titulo = g_titulos[i].id + 1;
    rc = ERP_OK;

fim:
    free(tc);
    free(tp);
    free(tv);
    free(tt);
    return rc;
}

#else /* USE_SQLSERVER nao definido: unidade de traducao vazia mas valida em ISO C */

typedef int aurora_sql_server_desativado;

#endif /* USE_SQLSERVER */
