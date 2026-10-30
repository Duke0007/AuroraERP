/*
 * Aurora ERP - persistencia.c
 * Gravacao e leitura de clientes.dat, produtos.dat, vendas.dat e titulos.dat
 * com FILE* (fopen/fwrite/fread/fclose).
 *
 * Formato de cada arquivo:
 *   [ArquivoCabecalho][registro 0][registro 1]...[registro N-1]
 * A gravacao e feita em arquivo temporario seguido de rename(), para nunca
 * deixar um .dat pela metade. Os arquivos sao binarios e dependem do layout
 * de structs da plataforma (o cabecalho guarda o tamanho do registro e e
 * validado na leitura).
 */
#include "erp.h"

#define ARQ_MAGIC  0x41524F52u /* "AROR" */
#define ARQ_VERSAO 1u

typedef struct {
    uint32_t magic;
    uint32_t versao;
    uint32_t quantidade;
    uint32_t tam_registro;
} ArquivoCabecalho;

static int salvar_vetor(const char *nome, const void *base, size_t tam_reg,
                        uint32_t qtd)
{
    char tmp[64];
    FILE *f;
    ArquivoCabecalho h;

    if (snprintf(tmp, sizeof tmp, "%s.tmp", nome) >= (int)sizeof tmp) {
        return ERP_ERR_ARQUIVO;
    }
    f = fopen(tmp, "wb");
    if (f == NULL) {
        return ERP_ERR_ARQUIVO;
    }
    memset(&h, 0, sizeof h);
    h.magic        = ARQ_MAGIC;
    h.versao       = ARQ_VERSAO;
    h.quantidade   = qtd;
    h.tam_registro = (uint32_t)tam_reg;
    if (fwrite(&h, sizeof h, 1, f) != 1) {
        fclose(f);
        remove(tmp);
        return ERP_ERR_ARQUIVO;
    }
    if (qtd > 0 && fwrite(base, tam_reg, qtd, f) != qtd) {
        fclose(f);
        remove(tmp);
        return ERP_ERR_ARQUIVO;
    }
    if (fflush(f) != 0 || fclose(f) != 0) {
        remove(tmp);
        return ERP_ERR_ARQUIVO;
    }
    if (rename(tmp, nome) != 0) {
        /* alguns sistemas nao sobrescrevem no rename */
        remove(nome);
        if (rename(tmp, nome) != 0) {
            remove(tmp);
            return ERP_ERR_ARQUIVO;
        }
    }
    return ERP_OK;
}

/* Le o arquivo para um buffer alocado; o chamador libera com free(). */
static int carregar_vetor(const char *nome, size_t tam_reg, uint32_t max,
                          void **out_buf, uint32_t *out_qtd)
{
    FILE *f = fopen(nome, "rb");
    ArquivoCabecalho h;
    void *buf = NULL;

    *out_buf = NULL;
    *out_qtd = 0;
    if (f == NULL) {
        return ERP_ERR_ARQUIVO;
    }
    if (fread(&h, sizeof h, 1, f) != 1 || h.magic != ARQ_MAGIC ||
        h.versao != ARQ_VERSAO || h.tam_registro != (uint32_t)tam_reg ||
        h.quantidade > max) {
        fclose(f);
        return ERP_ERR_ARQUIVO;
    }
    if (h.quantidade > 0) {
        buf = malloc(tam_reg * h.quantidade);
        if (buf == NULL) {
            fclose(f);
            return ERP_ERR_ARQUIVO;
        }
        if (fread(buf, tam_reg, h.quantidade, f) != h.quantidade) {
            free(buf);
            fclose(f);
            return ERP_ERR_ARQUIVO;
        }
    }
    fclose(f);
    *out_buf = buf;
    *out_qtd = h.quantidade;
    return ERP_OK;
}

int persistencia_salvar_dados(void)
{
    int rc;

    rc = salvar_vetor(ARQ_CLIENTES, g_clientes, sizeof(Cliente),
                      (uint32_t)g_num_clientes);
    if (rc != ERP_OK) {
        return rc;
    }
    rc = salvar_vetor(ARQ_PRODUTOS, g_produtos, sizeof(Produto),
                      (uint32_t)g_num_produtos);
    if (rc != ERP_OK) {
        return rc;
    }
    rc = salvar_vetor(ARQ_VENDAS, g_vendas, sizeof(Venda),
                      (uint32_t)g_num_vendas);
    if (rc != ERP_OK) {
        return rc;
    }
    return salvar_vetor(ARQ_TITULOS, g_titulos, sizeof(TituloFinanceiro),
                        (uint32_t)g_num_titulos);
}

/*
 * Carrega os quatro arquivos para buffers temporarios e so substitui o
 * banco em memoria se TODOS forem lidos e validados (tudo ou nada).
 */
int persistencia_carregar_dados(void)
{
    void *bc = NULL;
    void *bp = NULL;
    void *bv = NULL;
    void *bt = NULL;
    uint32_t nc = 0;
    uint32_t np = 0;
    uint32_t nv = 0;
    uint32_t nt = 0;
    int rc;
    uint32_t i;

    rc = carregar_vetor(ARQ_CLIENTES, sizeof(Cliente), MAX_CLIENTES, &bc, &nc);
    if (rc == ERP_OK) {
        rc = carregar_vetor(ARQ_PRODUTOS, sizeof(Produto), MAX_PRODUTOS, &bp, &np);
    }
    if (rc == ERP_OK) {
        rc = carregar_vetor(ARQ_VENDAS, sizeof(Venda), MAX_VENDAS, &bv, &nv);
    }
    if (rc == ERP_OK) {
        rc = carregar_vetor(ARQ_TITULOS, sizeof(TituloFinanceiro), MAX_TITULOS,
                            &bt, &nt);
    }
    if (rc != ERP_OK) {
        free(bc);
        free(bp);
        free(bv);
        free(bt);
        return rc;
    }

    memset(g_clientes, 0, sizeof g_clientes);
    memset(g_produtos, 0, sizeof g_produtos);
    memset(g_vendas, 0, sizeof g_vendas);
    memset(g_titulos, 0, sizeof g_titulos);
    if (nc > 0) {
        memcpy(g_clientes, bc, sizeof(Cliente) * nc);
    }
    if (np > 0) {
        memcpy(g_produtos, bp, sizeof(Produto) * np);
    }
    if (nv > 0) {
        memcpy(g_vendas, bv, sizeof(Venda) * nv);
    }
    if (nt > 0) {
        memcpy(g_titulos, bt, sizeof(TituloFinanceiro) * nt);
    }
    free(bc);
    free(bp);
    free(bv);
    free(bt);

    g_num_clientes = (int32_t)nc;
    g_num_produtos = (int32_t)np;
    g_num_vendas   = (int32_t)nv;
    g_num_titulos  = (int32_t)nt;

    /* Recalcula os contadores de ID a partir dos registros lidos */
    g_prox_id_cliente = 1;
    g_prox_id_produto = 1;
    g_prox_id_venda   = 1;
    g_prox_id_titulo  = 1;
    g_prox_numero_nf  = 1;
    for (i = 0; i < nc; i++) {
        if (g_clientes[i].id >= g_prox_id_cliente) {
            g_prox_id_cliente = g_clientes[i].id + 1;
        }
    }
    for (i = 0; i < np; i++) {
        if (g_produtos[i].id >= g_prox_id_produto) {
            g_prox_id_produto = g_produtos[i].id + 1;
        }
    }
    for (i = 0; i < nv; i++) {
        if (g_vendas[i].id >= g_prox_id_venda) {
            g_prox_id_venda = g_vendas[i].id + 1;
        }
        if (g_vendas[i].numero_nf >= g_prox_numero_nf) {
            g_prox_numero_nf = g_vendas[i].numero_nf + 1;
        }
    }
    for (i = 0; i < nt; i++) {
        if (g_titulos[i].id >= g_prox_id_titulo) {
            g_prox_id_titulo = g_titulos[i].id + 1;
        }
    }
    return ERP_OK;
}

/* Zera o banco e carrega dados de demonstracao. */
void sistema_inicializar_dados_padrao(void)
{
    Cliente c;
    Produto p;
    int id;
    int32_t hoje = data_hoje();

    cadastros_inicializar_empresa_padrao();
    cadastros_limpar();
    memset(g_vendas, 0, sizeof g_vendas);
    g_num_vendas     = 0;
    g_prox_id_venda  = 1;
    g_prox_numero_nf = 1;
    memset(g_titulos, 0, sizeof g_titulos);
    g_num_titulos    = 0;
    g_prox_id_titulo = 1;

    /* Clientes */
    memset(&c, 0, sizeof c);
    c.tipo = PESSOA_FISICA;
    util_copiar_string(c.nome, sizeof c.nome, "Maria da Silva");
    util_copiar_string(c.documento, sizeof c.documento, "11144477735");
    util_copiar_string(c.email, sizeof c.email, "maria@exemplo.com.br");
    util_copiar_string(c.cidade, sizeof c.cidade, "Sao Paulo");
    util_copiar_string(c.uf, sizeof c.uf, "SP");
    c.limite_credito_centavos = 500000;
    (void)cadastros_inserir_cliente(&c);

    memset(&c, 0, sizeof c);
    c.tipo = PESSOA_JURIDICA;
    util_copiar_string(c.nome, sizeof c.nome, "Padaria Pao Quente LTDA");
    util_copiar_string(c.documento, sizeof c.documento, "11222333000181");
    util_copiar_string(c.email, sizeof c.email, "compras@paoquente.com.br");
    util_copiar_string(c.cidade, sizeof c.cidade, "Campinas");
    util_copiar_string(c.uf, sizeof c.uf, "SP");
    c.limite_credito_centavos = 2000000;
    (void)cadastros_inserir_cliente(&c);

    memset(&c, 0, sizeof c);
    c.tipo = PESSOA_FISICA;
    util_copiar_string(c.nome, sizeof c.nome, "Joao Pereira");
    util_copiar_string(c.documento, sizeof c.documento, "52998224725");
    util_copiar_string(c.email, sizeof c.email, "joao.pereira@exemplo.com.br");
    util_copiar_string(c.cidade, sizeof c.cidade, "Santos");
    util_copiar_string(c.uf, sizeof c.uf, "SP");
    c.limite_credito_centavos = 300000;
    (void)cadastros_inserir_cliente(&c);

    /* Produtos + entradas de estoque (gera CMP real) */
    memset(&p, 0, sizeof p);
    util_copiar_string(p.codigo, sizeof p.codigo, "P001");
    util_copiar_string(p.descricao, sizeof p.descricao, "Arroz tipo 1 5kg");
    util_copiar_string(p.unidade, sizeof p.unidade, "PCT");
    p.preco_venda_centavos = 3290;
    id = cadastros_inserir_produto(&p);
    if (id > 0) {
        (void)estoque_registrar_entrada(id, 100, 2200);
        (void)estoque_registrar_entrada(id, 50, 2500); /* CMP = 2300 */
    }

    memset(&p, 0, sizeof p);
    util_copiar_string(p.codigo, sizeof p.codigo, "P002");
    util_copiar_string(p.descricao, sizeof p.descricao, "Feijao carioca 1kg");
    util_copiar_string(p.unidade, sizeof p.unidade, "PCT");
    p.preco_venda_centavos = 899;
    id = cadastros_inserir_produto(&p);
    if (id > 0) {
        (void)estoque_registrar_entrada(id, 80, 650);
    }

    memset(&p, 0, sizeof p);
    util_copiar_string(p.codigo, sizeof p.codigo, "P003");
    util_copiar_string(p.descricao, sizeof p.descricao, "Oleo de soja 900ml");
    util_copiar_string(p.unidade, sizeof p.unidade, "UN");
    p.preco_venda_centavos = 799;
    id = cadastros_inserir_produto(&p);
    if (id > 0) {
        (void)estoque_registrar_entrada(id, 120, 580);
    }

    memset(&p, 0, sizeof p);
    util_copiar_string(p.codigo, sizeof p.codigo, "P004");
    util_copiar_string(p.descricao, sizeof p.descricao, "Cafe torrado 500g");
    util_copiar_string(p.unidade, sizeof p.unidade, "PCT");
    p.preco_venda_centavos = 1990;
    id = cadastros_inserir_produto(&p);
    if (id > 0) {
        (void)estoque_registrar_entrada(id, 60, 1450);
    }

    memset(&p, 0, sizeof p);
    util_copiar_string(p.codigo, sizeof p.codigo, "P005");
    util_copiar_string(p.descricao, sizeof p.descricao, "Acucar refinado 5kg");
    util_copiar_string(p.unidade, sizeof p.unidade, "PCT");
    p.preco_venda_centavos = 2490;
    id = cadastros_inserir_produto(&p);
    if (id > 0) {
        (void)estoque_registrar_entrada(id, 90, 1800);
    }

    /* Conta a pagar de demonstracao */
    (void)financeiro_incluir_titulo(TITULO_PAGAR,
                                    "Fornecedor Distribuidora Alfa NF 1234", 0, 0,
                                    1, 1, 1250000, hoje, data_somar_dias(hoje, 15));
}
