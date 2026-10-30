/*
 * Aurora ERP - vendas.c
 * Pedido de venda (ORCAMENTO), itens com desconto, faturamento
 * (baixa de estoque + NF-e + contas a receber) e cancelamento.
 */
#include "erp.h"

Venda   g_vendas[MAX_VENDAS];
int32_t g_num_vendas     = 0;
int32_t g_prox_id_venda  = 1;
int32_t g_prox_numero_nf = 1;

Venda *vendas_buscar_id(int32_t id)
{
    int32_t i;

    for (i = 0; i < g_num_vendas; i++) {
        if (g_vendas[i].id == id) {
            return &g_vendas[i];
        }
    }
    return NULL;
}

int vendas_criar(int32_t cliente_id)
{
    Cliente *c = cadastros_buscar_cliente_id(cliente_id);
    Venda v;

    if (c == NULL) {
        return ERP_ERR_NAO_ENCONTRADO;
    }
    if (!c->ativo) {
        return ERP_ERR_ESTADO;
    }
    if (g_num_vendas >= MAX_VENDAS) {
        return ERP_ERR_LIMITE;
    }
    memset(&v, 0, sizeof v);
    v.id           = g_prox_id_venda;
    v.cliente_id   = cliente_id;
    v.data_emissao = data_hoje();
    v.status       = VENDA_ORCAMENTO;

    g_vendas[g_num_vendas] = v;
    g_num_vendas++;
    g_prox_id_venda++;
    return v.id;
}

/*
 * Subtotal e desconto: bruto = preco * quantidade;
 * desconto = bruto * desconto_bp / 10000 (arredondado); subtotal = bruto - desconto.
 */
int vendas_adicionar_item(int32_t venda_id, int32_t produto_id,
                          int64_t quantidade, int32_t desconto_bp)
{
    Venda *v = vendas_buscar_id(venda_id);
    Produto *p = cadastros_buscar_produto_id(produto_id);
    VendaItem it;
    int64_t bruto;
    int64_t desconto;
    int64_t t_bruto;
    int64_t t_desc;
    int64_t t_liq;

    if (v == NULL || p == NULL) {
        return ERP_ERR_NAO_ENCONTRADO;
    }
    if (v->status != VENDA_ORCAMENTO || !p->ativo) {
        return ERP_ERR_ESTADO;
    }
    if (quantidade <= 0 || desconto_bp < 0 || desconto_bp > 10000) {
        return ERP_ERR_PARAM;
    }
    if (v->num_itens >= MAX_ITENS_VENDA) {
        return ERP_ERR_LIMITE;
    }
    if (!util_mul_seguro(p->preco_venda_centavos, quantidade, &bruto)) {
        return ERP_ERR_OVERFLOW;
    }
    if (!util_mul_seguro(bruto, desconto_bp, &desconto)) {
        return ERP_ERR_OVERFLOW;
    }
    desconto = util_dividir_arredondado(desconto, 10000);
    if (desconto < 0 || desconto > bruto) {
        return ERP_ERR_OVERFLOW;
    }
    if (!util_soma_segura(v->total_bruto_centavos, bruto, &t_bruto) ||
        !util_soma_segura(v->total_desconto_centavos, desconto, &t_desc)) {
        return ERP_ERR_OVERFLOW;
    }
    t_liq = t_bruto - t_desc;

    memset(&it, 0, sizeof it);
    it.produto_id              = produto_id;
    it.quantidade              = quantidade;
    it.preco_unitario_centavos = p->preco_venda_centavos;
    it.desconto_bp             = desconto_bp;
    it.desconto_centavos       = desconto;
    it.subtotal_centavos       = bruto - desconto;
    it.custo_unitario_centavos = 0;

    v->itens[v->num_itens] = it;
    v->num_itens++;
    v->total_bruto_centavos    = t_bruto;
    v->total_desconto_centavos = t_desc;
    v->total_liquido_centavos  = t_liq;
    return ERP_OK;
}

/*
 * Faturamento:
 *  1. valida estado, credito, capacidade e saldo de TODOS os itens;
 *  2. da baixa no estoque (com reversao se algo falhar);
 *  3. gera numero e chave da NF-e;
 *  4. gera as parcelas de contas a receber (a cada 30 dias);
 *  5. muda o status para FATURADA.
 */
int vendas_faturar(int32_t venda_id, int num_parcelas,
                   int32_t data_primeiro_vencimento)
{
    Venda *v = vendas_buscar_id(venda_id);
    Cliente *c;
    int64_t saldos_antes[MAX_ITENS_VENDA];
    int32_t baixados = 0;
    int32_t i;
    int32_t j;
    int32_t n_titulos_antes;
    int32_t prox_id_titulo_antes;
    int64_t custo_total = 0;
    int64_t base;
    int64_t resto;
    int32_t hoje;
    int32_t vencimento;
    int32_t numero_nf;
    int32_t cod_num;
    int rc;
    char chave[TAM_CHAVE_NFE];

    if (v == NULL) {
        return ERP_ERR_NAO_ENCONTRADO;
    }
    if (v->status != VENDA_ORCAMENTO || v->num_itens <= 0) {
        return ERP_ERR_ESTADO;
    }
    if (num_parcelas < 1 || num_parcelas > MAX_PARCELAS ||
        v->total_liquido_centavos < num_parcelas) {
        return ERP_ERR_PARAM;
    }
    hoje = data_hoje();
    if (data_primeiro_vencimento == 0) {
        data_primeiro_vencimento = data_somar_dias(hoje, 30);
    }
    if (!data_valida(data_primeiro_vencimento)) {
        return ERP_ERR_PARAM;
    }
    c = cadastros_buscar_cliente_id(v->cliente_id);
    if (c == NULL || !c->ativo) {
        return ERP_ERR_ESTADO;
    }
    if (c->limite_credito_centavos > 0) {
        int64_t exposicao;

        if (!util_soma_segura(financeiro_saldo_devedor_cliente(c->id),
                              v->total_liquido_centavos, &exposicao) ||
            exposicao > c->limite_credito_centavos) {
            return ERP_ERR_CREDITO;
        }
    }
    if (g_num_titulos + num_parcelas > MAX_TITULOS) {
        return ERP_ERR_LIMITE;
    }

    /* 1. Valida o saldo de todos os itens (agrupando produtos repetidos) */
    for (i = 0; i < v->num_itens; i++) {
        Produto *p = cadastros_buscar_produto_id(v->itens[i].produto_id);
        int64_t demanda = 0;

        if (p == NULL || !p->ativo) {
            return ERP_ERR_ESTADO;
        }
        for (j = 0; j < v->num_itens; j++) {
            if (v->itens[j].produto_id == p->id) {
                if (!util_soma_segura(demanda, v->itens[j].quantidade, &demanda)) {
                    return ERP_ERR_OVERFLOW;
                }
            }
        }
        if (demanda > p->saldo_qtd) {
            return ERP_ERR_SALDO;
        }
    }

    /* 2. Baixa no estoque, guardando o saldo anterior para reversao */
    for (i = 0; i < v->num_itens; i++) {
        Produto *p = cadastros_buscar_produto_id(v->itens[i].produto_id);

        saldos_antes[i] = p->saldo_qtd;
        rc = estoque_dar_baixa(p->id, v->itens[i].quantidade);
        if (rc != ERP_OK) {
            for (j = baixados - 1; j >= 0; j--) {
                Produto *pr = cadastros_buscar_produto_id(v->itens[j].produto_id);
                pr->saldo_qtd = saldos_antes[j];
            }
            return rc;
        }
        baixados++;
    }

    /* 3. NF-e */
    numero_nf = g_prox_numero_nf;
    cod_num = (int32_t)(((uint64_t)numero_nf * 2654435761ULL) % 100000000ULL);
    if (cod_num == numero_nf) {
        cod_num = (cod_num + 1) % 100000000;
    }
    rc = fiscal_gerar_chave_nfe(g_empresa.uf_codigo_ibge, hoje / 10000,
                                (hoje / 100) % 100, g_empresa.cnpj,
                                g_empresa.modelo_nfe, g_empresa.serie_nfe,
                                numero_nf, g_empresa.tipo_emissao, cod_num,
                                chave, sizeof chave);
    if (rc != ERP_OK) {
        for (j = baixados - 1; j >= 0; j--) {
            Produto *pr = cadastros_buscar_produto_id(v->itens[j].produto_id);
            pr->saldo_qtd = saldos_antes[j];
        }
        return rc;
    }

    /* 4. Contas a receber */
    n_titulos_antes      = g_num_titulos;
    prox_id_titulo_antes = g_prox_id_titulo;
    base  = v->total_liquido_centavos / num_parcelas;
    resto = v->total_liquido_centavos % num_parcelas;
    for (i = 0; i < num_parcelas; i++) {
        char desc[TAM_DESCRICAO];
        int64_t valor = base + ((i == num_parcelas - 1) ? resto : 0);

        vencimento = data_somar_dias(data_primeiro_vencimento, 30 * i);
        snprintf(desc, sizeof desc, "Venda %d NF %d parc %d/%d", (int)v->id,
                 (int)numero_nf, (int)(i + 1), num_parcelas);
        rc = financeiro_incluir_titulo(TITULO_RECEBER, desc, v->cliente_id,
                                       v->id, i + 1, num_parcelas, valor, hoje,
                                       vencimento);
        if (rc < 0) {
            g_num_titulos    = n_titulos_antes;
            g_prox_id_titulo = prox_id_titulo_antes;
            for (j = baixados - 1; j >= 0; j--) {
                Produto *pr = cadastros_buscar_produto_id(v->itens[j].produto_id);
                pr->saldo_qtd = saldos_antes[j];
            }
            return rc;
        }
    }

    /* 5. Efetiva a venda: custo (CMP vigente) e status */
    for (i = 0; i < v->num_itens; i++) {
        Produto *p = cadastros_buscar_produto_id(v->itens[i].produto_id);
        int64_t custo_linha = 0;

        v->itens[i].custo_unitario_centavos = p->cmp_centavos;
        if (util_mul_seguro(p->cmp_centavos, v->itens[i].quantidade, &custo_linha)) {
            (void)util_soma_segura(custo_total, custo_linha, &custo_total);
        }
    }
    v->custo_total_centavos = custo_total;
    v->numero_nf    = numero_nf;
    g_prox_numero_nf++;
    util_copiar_string(v->chave_nfe, sizeof v->chave_nfe, chave);
    v->num_parcelas = num_parcelas;
    v->data_emissao = hoje;
    v->status       = VENDA_FATURADA;
    return ERP_OK;
}

int vendas_cancelar(int32_t venda_id)
{
    Venda *v = vendas_buscar_id(venda_id);
    int32_t i;

    if (v == NULL) {
        return ERP_ERR_NAO_ENCONTRADO;
    }
    if (v->status == VENDA_CANCELADA) {
        return ERP_ERR_ESTADO;
    }
    if (v->status == VENDA_ORCAMENTO) {
        v->status = VENDA_CANCELADA;
        return ERP_OK;
    }
    /* FATURADA: so cancela se nenhum titulo tiver recebimento */
    for (i = 0; i < g_num_titulos; i++) {
        if (g_titulos[i].venda_id == v->id &&
            g_titulos[i].valor_pago_centavos > 0) {
            return ERP_ERR_ESTADO;
        }
    }
    for (i = 0; i < v->num_itens; i++) {
        int rc = estoque_registrar_entrada(v->itens[i].produto_id,
                                           v->itens[i].quantidade,
                                           v->itens[i].custo_unitario_centavos);
        if (rc != ERP_OK) {
            return rc;
        }
    }
    for (i = 0; i < g_num_titulos; i++) {
        if (g_titulos[i].venda_id == v->id &&
            g_titulos[i].status == TITULO_ABERTO) {
            g_titulos[i].status = TITULO_CANCELADO;
        }
    }
    v->status = VENDA_CANCELADA;
    return ERP_OK;
}

static const char *texto_status_venda(StatusVenda s)
{
    switch (s) {
    case VENDA_ORCAMENTO: return "ORCAMENTO";
    case VENDA_FATURADA:  return "FATURADA";
    case VENDA_CANCELADA: return "CANCELADA";
    default:              return "?";
    }
}

void vendas_listar(void)
{
    const char *sep =
        "+-------+------------------------------+------------+-----------+------+---------------+\n";
    int32_t i;
    char data[TAM_DATA];
    char total[TAM_MOEDA];

    printf("\n%s", sep);
    printf("| %-5s | %-28s | %-10s | %-9s | %-4s | %13s |\n", "ID", "Cliente",
           "Emissao", "Status", "NF", "Total");
    printf("%s", sep);
    if (g_num_vendas == 0) {
        printf("| %-83s |\n", "Nenhuma venda registrada");
    }
    for (i = 0; i < g_num_vendas; i++) {
        const Venda *v = &g_vendas[i];
        const Cliente *c = cadastros_buscar_cliente_id(v->cliente_id);

        data_formatar(v->data_emissao, data, sizeof data);
        util_formatar_moeda(v->total_liquido_centavos, total, sizeof total);
        printf("| %5d | %-28.28s | %-10s | %-9s | %4d | %13s |\n", (int)v->id,
               c != NULL ? c->nome : "(removido)", data,
               texto_status_venda(v->status), (int)v->numero_nf, total);
    }
    printf("%s", sep);
}

void vendas_imprimir_fp(FILE *fp, const Venda *v)
{
    int32_t i;
    char b1[TAM_MOEDA];
    char b2[TAM_MOEDA];
    char b3[TAM_MOEDA];
    char data[TAM_DATA];
    const Cliente *c;

    if (v == NULL) {
        return;
    }
    c = cadastros_buscar_cliente_id(v->cliente_id);
    data_formatar(v->data_emissao, data, sizeof data);
    fprintf(fp, "\n=== VENDA %d - %s ===\n", (int)v->id, texto_status_venda(v->status));
    fprintf(fp, "Cliente : %s\n", c != NULL ? c->nome : "(removido)");
    fprintf(fp, "Data    : %s\n", data);
    if (v->status == VENDA_FATURADA) {
        fprintf(fp, "NF-e    : %d\nChave   : %s\n", (int)v->numero_nf, v->chave_nfe);
        fprintf(fp, "Parcelas: %d\n", (int)v->num_parcelas);
    }
    fprintf(fp, "%-4s %-24s %6s %13s %6s %13s\n", "Item", "Produto", "Qtd",
           "Preco un.", "Desc.", "Subtotal");
    for (i = 0; i < v->num_itens; i++) {
        const VendaItem *it = &v->itens[i];
        const Produto *p = cadastros_buscar_produto_id(it->produto_id);

        util_formatar_moeda(it->preco_unitario_centavos, b1, sizeof b1);
        util_formatar_moeda(it->subtotal_centavos, b2, sizeof b2);
        fprintf(fp, "%-4d %-24.24s %6lld %13s %2d.%02d%% %13s\n", (int)(i + 1),
               p != NULL ? p->descricao : "(removido)", (long long)it->quantidade,
               b1, (int)(it->desconto_bp / 100), (int)(it->desconto_bp % 100), b2);
    }
    util_formatar_moeda(v->total_bruto_centavos, b1, sizeof b1);
    util_formatar_moeda(v->total_desconto_centavos, b2, sizeof b2);
    util_formatar_moeda(v->total_liquido_centavos, b3, sizeof b3);
    fprintf(fp, "Bruto: %s | Desconto: %s | Liquido: %s\n", b1, b2, b3);
    if (v->status == VENDA_FATURADA) {
        util_formatar_moeda(v->custo_total_centavos, b1, sizeof b1);
        util_formatar_moeda(v->total_liquido_centavos - v->custo_total_centavos,
                            b2, sizeof b2);
        fprintf(fp, "Custo (CMP): %s | Margem bruta: %s\n", b1, b2);
    }
}

void vendas_imprimir(const Venda *v)
{
    vendas_imprimir_fp(stdout, v);
}
