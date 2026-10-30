/*
 * Aurora ERP - cadastros.c
 * Clientes, produtos, utilitarios de entrada/saida e aritmetica segura.
 */
#include "erp.h"

/* ------------------------------------------------------------------ */
/* Banco em memoria (vetores estaticos)                                */
/* ------------------------------------------------------------------ */
Empresa g_empresa;

Cliente g_clientes[MAX_CLIENTES];
int32_t g_num_clientes   = 0;
int32_t g_prox_id_cliente = 1;

Produto g_produtos[MAX_PRODUTOS];
int32_t g_num_produtos    = 0;
int32_t g_prox_id_produto = 1;

/* ------------------------------------------------------------------ */
/* Utilitarios                                                         */
/* ------------------------------------------------------------------ */
const char *erp_mensagem_erro(int codigo)
{
    switch (codigo) {
    case ERP_OK:                  return "Operacao concluida com sucesso";
    case ERP_ERR_PARAM:           return "Parametro invalido";
    case ERP_ERR_LIMITE:          return "Limite de capacidade atingido";
    case ERP_ERR_NAO_ENCONTRADO:  return "Registro nao encontrado";
    case ERP_ERR_ESTADO:          return "Operacao invalida para o estado atual";
    case ERP_ERR_SALDO:           return "Saldo insuficiente em estoque";
    case ERP_ERR_OVERFLOW:        return "Estouro numerico (valor grande demais)";
    case ERP_ERR_DUPLICADO:       return "Registro duplicado";
    case ERP_ERR_ARQUIVO:         return "Erro de leitura/gravacao de arquivo";
    case ERP_ERR_DOCUMENTO:       return "Documento (CPF/CNPJ) invalido";
    case ERP_ERR_CREDITO:         return "Limite de credito do cliente excedido";
    case ERP_ERR_ACESSO:          return "Acesso negado";
    case ERP_ERR_SENHA_FRACA:     return "Senha fraca (minimo de 6 caracteres)";
    case ERP_ERR_BLOQUEADO:       return "Muitas tentativas: aguarde alguns minutos";
    default:                      return "Erro desconhecido";
    }
}

void util_copiar_string(char *dest, size_t tam, const char *orig)
{
    size_t n;

    if (dest == NULL || tam == 0) {
        return;
    }
    if (orig == NULL) {
        dest[0] = '\0';
        return;
    }
    n = strlen(orig);
    if (n > tam - 1) {
        n = tam - 1;
    }
    memcpy(dest, orig, n);
    dest[n] = '\0';
}

bool util_ler_linha(const char *prompt, char *buf, size_t tam)
{
    size_t len;
    int c;

    if (prompt != NULL) {
        printf("%s", prompt);
        fflush(stdout);
    }
    if (fgets(buf, (int)tam, stdin) == NULL) {
        buf[0] = '\0';
        return false;
    }
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        len--;
    } else {
        /* linha maior que o buffer: descarta o restante */
        while ((c = getchar()) != '\n' && c != EOF) {
        }
    }
    if (len > 0 && buf[len - 1] == '\r') {
        buf[len - 1] = '\0';
    }
    return true;
}

bool util_parse_int64(const char *txt, int64_t *out)
{
    bool negativo = false;
    int64_t acc = 0;
    size_t i = 0;
    bool algum = false;

    if (txt == NULL || out == NULL) {
        return false;
    }
    while (txt[i] == ' ' || txt[i] == '\t') {
        i++;
    }
    if (txt[i] == '-') {
        negativo = true;
        i++;
    } else if (txt[i] == '+') {
        i++;
    }
    for (; txt[i] >= '0' && txt[i] <= '9'; i++) {
        int d = txt[i] - '0';
        if (acc > (INT64_MAX - d) / 10) {
            return false;
        }
        acc = acc * 10 + d;
        algum = true;
    }
    while (txt[i] == ' ' || txt[i] == '\t') {
        i++;
    }
    if (!algum || txt[i] != '\0') {
        return false;
    }
    *out = negativo ? -acc : acc;
    return true;
}

bool util_ler_int64(const char *prompt, int64_t *out)
{
    char buf[64];

    if (!util_ler_linha(prompt, buf, sizeof buf)) {
        return false;
    }
    return util_parse_int64(buf, out);
}

/*
 * Converte texto monetario em centavos sem usar ponto flutuante.
 * Aceita: "1234", "1234,5", "1.234,56", "12.34".
 * O ultimo separador seguido de 1 ou 2 digitos e o separador decimal;
 * os demais separadores sao tratados como milhar.
 */
bool util_parse_centavos(const char *txt, int64_t *out)
{
    char digitos[40];
    size_t n = 0;
    size_t i;
    size_t len;
    long ultimo_sep = -1;
    size_t casas = 0;
    int64_t inteiro = 0;
    int64_t frac = 0;
    int64_t total;

    if (txt == NULL || out == NULL) {
        return false;
    }
    while (*txt == ' ' || *txt == '\t') {
        txt++;
    }
    len = strlen(txt);
    while (len > 0 && (txt[len - 1] == ' ' || txt[len - 1] == '\t')) {
        len--;
    }
    if (len == 0) {
        return false;
    }
    for (i = 0; i < len; i++) {
        if (txt[i] == ',' || txt[i] == '.') {
            ultimo_sep = (long)i;
        } else if (txt[i] < '0' || txt[i] > '9') {
            return false;
        }
    }
    if (ultimo_sep >= 0) {
        casas = len - (size_t)ultimo_sep - 1;
        if (casas != 1 && casas != 2) {
            ultimo_sep = -1; /* separador de milhar */
            casas = 0;
        }
    }
    for (i = 0; i < len; i++) {
        if (txt[i] >= '0' && txt[i] <= '9') {
            if (ultimo_sep >= 0 && (long)i > ultimo_sep) {
                frac = frac * 10 + (txt[i] - '0');
            } else {
                if (n >= sizeof digitos - 1) {
                    return false;
                }
                digitos[n++] = txt[i];
            }
        }
    }
    digitos[n] = '\0';
    if (n == 0 && casas == 0) {
        return false;
    }
    if (n > 0 && !util_parse_int64(digitos, &inteiro)) {
        return false;
    }
    if (casas == 1) {
        frac *= 10;
    }
    if (!util_mul_seguro(inteiro, 100, &total) || !util_soma_segura(total, frac, &total)) {
        return false;
    }
    *out = total;
    return true;
}

bool util_ler_centavos(const char *prompt, int64_t *out)
{
    char buf[64];

    if (!util_ler_linha(prompt, buf, sizeof buf)) {
        return false;
    }
    return util_parse_centavos(buf, out);
}

void util_formatar_moeda(int64_t centavos, char *out, size_t tam)
{
    char digitos[32];
    char agrup[48];
    bool neg = centavos < 0;
    uint64_t v;
    uint64_t reais;
    unsigned cent;
    size_t len;
    size_t i;
    size_t pos = 0;

    if (neg) {
        v = (uint64_t)(-(centavos + 1)) + 1u;
    } else {
        v = (uint64_t)centavos;
    }
    reais = v / 100u;
    cent = (unsigned)(v % 100u);
    snprintf(digitos, sizeof digitos, "%llu", (unsigned long long)reais);
    len = strlen(digitos);
    for (i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) {
            agrup[pos++] = '.';
        }
        agrup[pos++] = digitos[i];
    }
    agrup[pos] = '\0';
    snprintf(out, tam, "%sR$ %s,%02u", neg ? "-" : "", agrup, cent);
}

bool util_mul_seguro(int64_t a, int64_t b, int64_t *res)
{
    if (a < 0 || b < 0) {
        return false;
    }
    if (a != 0 && b > INT64_MAX / a) {
        return false;
    }
    *res = a * b;
    return true;
}

bool util_soma_segura(int64_t a, int64_t b, int64_t *res)
{
    if (a < 0 || b < 0) {
        return false;
    }
    if (a > INT64_MAX - b) {
        return false;
    }
    *res = a + b;
    return true;
}

/* Divisao inteira com arredondamento "meio para cima" (operandos >= 0). */
int64_t util_dividir_arredondado(int64_t numerador, int64_t denominador)
{
    if (denominador <= 0 || numerador < 0) {
        return -1;
    }
    if (numerador > INT64_MAX - denominador / 2) {
        return -1;
    }
    return (numerador + denominador / 2) / denominador;
}

/* ------------------------------------------------------------------ */
/* Empresa / limpeza                                                   */
/* ------------------------------------------------------------------ */
void cadastros_inicializar_empresa_padrao(void)
{
    memset(&g_empresa, 0, sizeof g_empresa);
    util_copiar_string(g_empresa.razao_social, sizeof g_empresa.razao_social,
                       "Aurora Comercio de Alimentos LTDA");
    util_copiar_string(g_empresa.nome_fantasia, sizeof g_empresa.nome_fantasia,
                       "Mercado Aurora");
    util_copiar_string(g_empresa.cnpj, sizeof g_empresa.cnpj, "11444777000161");
    util_copiar_string(g_empresa.cidade, sizeof g_empresa.cidade, "Sao Paulo");
    util_copiar_string(g_empresa.uf_sigla, sizeof g_empresa.uf_sigla, "SP");
    g_empresa.uf_codigo_ibge = 35;
    g_empresa.modelo_nfe     = 55;
    g_empresa.serie_nfe      = 1;
    g_empresa.tipo_emissao   = 1;
}

void cadastros_limpar(void)
{
    memset(g_clientes, 0, sizeof g_clientes);
    g_num_clientes    = 0;
    g_prox_id_cliente = 1;
    memset(g_produtos, 0, sizeof g_produtos);
    g_num_produtos    = 0;
    g_prox_id_produto = 1;
}

/* ------------------------------------------------------------------ */
/* Clientes                                                            */
/* ------------------------------------------------------------------ */
int cadastros_inserir_cliente(const Cliente *novo)
{
    Cliente c;
    char doc[TAM_DOC];
    int32_t i;

    if (novo == NULL || novo->nome[0] == '\0') {
        return ERP_ERR_PARAM;
    }
    if (g_num_clientes >= MAX_CLIENTES) {
        return ERP_ERR_LIMITE;
    }
    if (novo->tipo != PESSOA_FISICA && novo->tipo != PESSOA_JURIDICA) {
        return ERP_ERR_PARAM;
    }
    if (novo->limite_credito_centavos < 0) {
        return ERP_ERR_PARAM;
    }
    if (fiscal_somente_digitos(novo->documento, doc, sizeof doc) < 0) {
        return ERP_ERR_DOCUMENTO;
    }
    if (novo->tipo == PESSOA_FISICA && !fiscal_validar_cpf(doc)) {
        return ERP_ERR_DOCUMENTO;
    }
    if (novo->tipo == PESSOA_JURIDICA && !fiscal_validar_cnpj(doc)) {
        return ERP_ERR_DOCUMENTO;
    }
    for (i = 0; i < g_num_clientes; i++) {
        if (strcmp(g_clientes[i].documento, doc) == 0) {
            return ERP_ERR_DUPLICADO;
        }
    }

    memset(&c, 0, sizeof c);
    c.id   = g_prox_id_cliente;
    c.tipo = novo->tipo;
    util_copiar_string(c.nome, sizeof c.nome, novo->nome);
    util_copiar_string(c.documento, sizeof c.documento, doc);
    util_copiar_string(c.email, sizeof c.email, novo->email);
    util_copiar_string(c.cidade, sizeof c.cidade, novo->cidade);
    util_copiar_string(c.uf, sizeof c.uf, novo->uf);
    c.limite_credito_centavos = novo->limite_credito_centavos;
    c.ativo = true;

    g_clientes[g_num_clientes] = c;
    g_num_clientes++;
    g_prox_id_cliente++;
    return c.id;
}

Cliente *cadastros_buscar_cliente_id(int32_t id)
{
    int32_t i;

    for (i = 0; i < g_num_clientes; i++) {
        if (g_clientes[i].id == id) {
            return &g_clientes[i];
        }
    }
    return NULL;
}

void cadastros_listar_clientes(void)
{
    const char *sep =
        "+-------+------+--------------------------------+----------------+----------------------+------------------+\n";
    int32_t i;
    char limite[TAM_MOEDA];
    char cidade_uf[48];

    printf("\n%s", sep);
    printf("| %-5s | %-4s | %-30s | %-14s | %-20s | %16s |\n",
           "ID", "Tipo", "Nome", "Documento", "Cidade/UF", "Limite credito");
    printf("%s", sep);
    if (g_num_clientes == 0) {
        printf("| %-104s |\n", "Nenhum cliente cadastrado");
    }
    for (i = 0; i < g_num_clientes; i++) {
        const Cliente *c = &g_clientes[i];

        util_formatar_moeda(c->limite_credito_centavos, limite, sizeof limite);
        snprintf(cidade_uf, sizeof cidade_uf, "%s/%s", c->cidade, c->uf);
        printf("| %5d | %-4s | %-30.30s | %-14s | %-20.20s | %16s |\n",
               (int)c->id, c->tipo == PESSOA_FISICA ? "PF" : "PJ", c->nome,
               c->documento, cidade_uf, limite);
    }
    printf("%s", sep);
    printf("Total: %d cliente(s)\n", (int)g_num_clientes);
}

/* ------------------------------------------------------------------ */
/* Produtos                                                            */
/* ------------------------------------------------------------------ */
int cadastros_inserir_produto(const Produto *novo)
{
    Produto p;
    int32_t i;

    if (novo == NULL || novo->codigo[0] == '\0' || novo->descricao[0] == '\0') {
        return ERP_ERR_PARAM;
    }
    if (g_num_produtos >= MAX_PRODUTOS) {
        return ERP_ERR_LIMITE;
    }
    if (novo->preco_venda_centavos < 0) {
        return ERP_ERR_PARAM;
    }
    for (i = 0; i < g_num_produtos; i++) {
        if (strcmp(g_produtos[i].codigo, novo->codigo) == 0) {
            return ERP_ERR_DUPLICADO;
        }
    }

    memset(&p, 0, sizeof p);
    p.id = g_prox_id_produto;
    util_copiar_string(p.codigo, sizeof p.codigo, novo->codigo);
    util_copiar_string(p.descricao, sizeof p.descricao, novo->descricao);
    util_copiar_string(p.unidade, sizeof p.unidade,
                       novo->unidade[0] != '\0' ? novo->unidade : "UN");
    p.saldo_qtd            = 0;  /* o saldo so entra via estoque_registrar_entrada */
    p.cmp_centavos         = 0;
    p.preco_venda_centavos = novo->preco_venda_centavos;
    p.ativo                = true;

    g_produtos[g_num_produtos] = p;
    g_num_produtos++;
    g_prox_id_produto++;
    return p.id;
}

Produto *cadastros_buscar_produto_id(int32_t id)
{
    int32_t i;

    for (i = 0; i < g_num_produtos; i++) {
        if (g_produtos[i].id == id) {
            return &g_produtos[i];
        }
    }
    return NULL;
}

Produto *cadastros_buscar_produto_codigo(const char *codigo)
{
    int32_t i;

    if (codigo == NULL) {
        return NULL;
    }
    for (i = 0; i < g_num_produtos; i++) {
        if (strcmp(g_produtos[i].codigo, codigo) == 0) {
            return &g_produtos[i];
        }
    }
    return NULL;
}

void cadastros_listar_produtos(void)
{
    const char *sep =
        "+-------+------------+------------------------------+-----+----------+---------------+---------------+\n";
    int32_t i;
    char cmp[TAM_MOEDA];
    char preco[TAM_MOEDA];

    printf("\n%s", sep);
    printf("| %-5s | %-10s | %-28s | %-3s | %8s | %13s | %13s |\n",
           "ID", "Codigo", "Descricao", "Un", "Saldo", "CMP", "Preco venda");
    printf("%s", sep);
    if (g_num_produtos == 0) {
        printf("| %-97s |\n", "Nenhum produto cadastrado");
    }
    for (i = 0; i < g_num_produtos; i++) {
        const Produto *p = &g_produtos[i];

        util_formatar_moeda(p->cmp_centavos, cmp, sizeof cmp);
        util_formatar_moeda(p->preco_venda_centavos, preco, sizeof preco);
        printf("| %5d | %-10.10s | %-28.28s | %-3.3s | %8lld | %13s | %13s |\n",
               (int)p->id, p->codigo, p->descricao, p->unidade,
               (long long)p->saldo_qtd, cmp, preco);
    }
    printf("%s", sep);
    printf("Total: %d produto(s)\n", (int)g_num_produtos);
}
