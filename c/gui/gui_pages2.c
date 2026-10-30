/*
 * Aurora ERP - gui_pages2.c
 * Paginas: Financeiro, NF-e & Boletos, Usuarios e Configuracoes.
 */
#include "gui.h"

static void fmt_num(int64_t c, char *out, size_t tam)
{
    snprintf(out, tam, "%lld,%02lld", (long long)(c / 100), (long long)(c % 100));
}

/* ================================================================== */
/* Financeiro                                                          */
/* ================================================================== */
static GtkListStore *st_fin;
static GtkWidget *tv_fin;
static GtkWidget *lb_receber, *lb_pagar, *lb_saldo;
static int g_fin_filtro = 0; /* 0 todos, 1 receber, 2 pagar, 3 em aberto, 4 vencidos */

static const char *status_titulo_txt(const TituloFinanceiro *t)
{
    if (t->status == TITULO_CANCELADO) {
        return "CANCELADO";
    }
    if (t->status == TITULO_LIQUIDADO) {
        return "LIQUIDADO";
    }
    if (data_para_dias(t->data_vencimento) < data_para_dias(data_hoje())) {
        return "VENCIDO";
    }
    return t->valor_pago_centavos > 0 ? "PARCIAL" : "ABERTO";
}

static gboolean fin_pred(GtkTreeModel *m, GtkTreeIter *it)
{
    gchar *tipo = NULL;
    gchar *st = NULL;
    gboolean ok = TRUE;

    if (g_fin_filtro == 0) {
        return TRUE;
    }
    gtk_tree_model_get(m, it, 1, &tipo, 8, &st, -1);
    if (tipo == NULL || st == NULL) {
        g_free(tipo);
        g_free(st);
        return TRUE;
    }
    switch (g_fin_filtro) {
    case 1: ok = strcmp(tipo, "RECEBER") == 0; break;
    case 2: ok = strcmp(tipo, "PAGAR") == 0; break;
    case 3: ok = !strcmp(st, "ABERTO") || !strcmp(st, "PARCIAL") || !strcmp(st, "VENCIDO"); break;
    case 4: ok = strcmp(st, "VENCIDO") == 0; break;
    default: break;
    }
    g_free(tipo);
    g_free(st);
    return ok;
}

static void financeiro_atualizar(void)
{
    int32_t i;
    char b[TAM_MOEDA];
    int64_t rec = financeiro_total_aberto(TITULO_RECEBER);
    int64_t pag = financeiro_total_aberto(TITULO_PAGAR);

    gtk_list_store_clear(st_fin);
    for (i = 0; i < g_num_titulos; i++) {
        const TituloFinanceiro *t = &g_titulos[i];
        const Cliente *c = t->tipo == TITULO_RECEBER ? cadastros_buscar_cliente_id(t->entidade_id) : NULL;
        char id[16], venc[TAM_DATA], val[TAM_MOEDA], pago[TAM_MOEDA], saldo[TAM_MOEDA];
        const char *cols[9];

        snprintf(id, sizeof id, "%d", (int)t->id);
        data_formatar(t->data_vencimento, venc, sizeof venc);
        util_formatar_moeda(t->valor_centavos, val, sizeof val);
        util_formatar_moeda(t->valor_pago_centavos, pago, sizeof pago);
        util_formatar_moeda(financeiro_saldo_titulo(t), saldo, sizeof saldo);
        cols[0] = id;
        cols[1] = t->tipo == TITULO_RECEBER ? "RECEBER" : "PAGAR";
        cols[2] = t->descricao;
        cols[3] = c != NULL ? c->nome : "-";
        cols[4] = venc;
        cols[5] = val;
        cols[6] = pago;
        cols[7] = saldo;
        cols[8] = status_titulo_txt(t);
        gui_store_add(st_fin, cols, 9);
    }
    util_formatar_moeda(rec, b, sizeof b);
    gtk_label_set_text(GTK_LABEL(lb_receber), b);
    util_formatar_moeda(pag, b, sizeof b);
    gtk_label_set_text(GTK_LABEL(lb_pagar), b);
    util_formatar_moeda(rec - pag, b, sizeof b);
    gtk_label_set_text(GTK_LABEL(lb_saldo), b);
}

static void fin_filtro_mudou(GtkComboBox *c, gpointer d)
{
    (void)d;
    g_fin_filtro = gtk_combo_box_get_active(c);
    gui_refiltrar();
}

static void fin_novo(GtkButton *b, gpointer d)
{
    GString *op = g_string_new("");
    char hoje[TAM_DATA];
    Campo c[] = {
        {"Tipo", CAMPO_COMBO, "A receber", "A receber|A pagar"},
        {"Descrição", CAMPO_TEXTO, "", NULL},
        {"Cliente (somente contas a receber)", CAMPO_COMBO, NULL, NULL},
        {"Valor (R$)", CAMPO_TEXTO, "", NULL},
        {"Vencimento (dd/mm/aaaa)", CAMPO_TEXTO, "", NULL}
    };
    char v[5][160];
    int64_t valor = 0;
    int32_t venc = 0;
    int rc;
    char ev[200];

    (void)b; (void)d;
    if (!gui_pode(MOD_FINANCEIRO, ACAO_EDITAR, TRUE)) {
        g_string_free(op, TRUE);
        return;
    }
    gui_opcoes_clientes(op, TRUE, TRUE);
    c[2].opcoes = op->str;
    data_formatar(data_hoje(), hoje, sizeof hoje);
    c[4].inicial = hoje;
    if (gui_form("Novo título financeiro", "Incluir", c, 5, v)) {
        if (!util_parse_centavos(v[3], &valor) || !data_parse_br(v[4], &venc) || venc == 0) {
            gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Dados inválidos",
                    "Verifique o valor (ex.: 1.250,00) e a data (dd/mm/aaaa).");
        } else {
            int receber = strcmp(v[0], "A pagar") != 0;

            rc = financeiro_incluir_titulo(receber ? TITULO_RECEBER : TITULO_PAGAR, v[1],
                                           receber ? gui_id_da_opcao(v[2]) : 0, 0, 1, 1, valor,
                                           data_hoje(), venc);
            if (rc < 0) {
                gui_erro_rc(rc);
            } else {
                snprintf(ev, sizeof ev, "Título %d incluído (%s)", rc, receber ? "receber" : "pagar");
                gui_gravar(ev);
            }
        }
    }
    g_string_free(op, TRUE);
}

static void fin_baixar(GtkButton *b, gpointer d)
{
    int tid;
    TituloFinanceiro *t;
    char sug[40];
    char hoje[TAM_DATA];
    Campo c[] = {
        {"Valor pago (R$)", CAMPO_TEXTO, NULL, NULL},
        {"Data do pagamento (dd/mm/aaaa)", CAMPO_TEXTO, NULL, NULL}
    };
    char v[2][160];
    int64_t valor = 0;
    int32_t data = 0;
    int rc;
    char ev[200];

    (void)b; (void)d;
    if (!gui_pode(MOD_FINANCEIRO, ACAO_EDITAR, TRUE) || !gui_selecionado(tv_fin, &tid)) {
        return;
    }
    t = financeiro_buscar_id(tid);
    if (t == NULL || t->status != TITULO_ABERTO) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_WARNING, "Título indisponível",
                "Só é possível baixar títulos em aberto.");
        return;
    }
    fmt_num(financeiro_saldo_titulo(t), sug, sizeof sug);
    data_formatar(data_hoje(), hoje, sizeof hoje);
    c[0].inicial = sug;
    c[1].inicial = hoje;
    if (!gui_form("Baixar / liquidar título", "Confirmar baixa", c, 2, v)) {
        return;
    }
    if (!util_parse_centavos(v[0], &valor) || !data_parse_br(v[1], &data)) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Dados inválidos", "Verifique o valor e a data.");
        return;
    }
    if (data == 0) {
        data = data_hoje();
    }
    rc = financeiro_baixar_titulo(tid, valor, data);
    if (rc != ERP_OK) {
        gui_erro_rc(rc);
        return;
    }
    snprintf(ev, sizeof ev, "Baixa no título %d", tid);
    gui_gravar(ev);
}

static void fin_boleto(GtkButton *b, gpointer d)
{
    int tid;
    char linha[TAM_LINHA_DIG];
    char barras[TAM_CODIGO_BARRAS];
    char txt[400];
    char val[TAM_MOEDA];
    char venc[TAM_DATA];
    TituloFinanceiro *t;
    int rc;

    (void)b; (void)d;
    if (!gui_pode(MOD_FINANCEIRO, ACAO_EDITAR, TRUE) || !gui_selecionado(tv_fin, &tid)) {
        return;
    }
    rc = financeiro_gerar_boleto(tid, linha, sizeof linha, barras, sizeof barras);
    if (rc != ERP_OK) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_WARNING, "Boleto indisponível",
                "O boleto só pode ser emitido para títulos a receber em aberto.");
        return;
    }
    t = financeiro_buscar_id(tid);
    util_formatar_moeda(financeiro_saldo_titulo(t), val, sizeof val);
    data_formatar(t->data_vencimento, venc, sizeof venc);
    snprintf(txt, sizeof txt,
             "BOLETO — título %d\n%s\n\nVencimento : %s\nValor      : %s\n\nLinha digitável:\n%s\n\n"
             "Código de barras:\n%s\n",
             tid, t->descricao, venc, val, linha, barras);
    gui_registrar("Boleto emitido");
    gui_mostrar_texto("Boleto bancário", txt);
}

static GtkWidget *mini_kpi(const char *titulo, GtkWidget **lbl)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

    gtk_style_context_add_class(gtk_widget_get_style_context(box), "card");
    gtk_box_pack_start(GTK_BOX(box), gui_label(titulo, "kpi-titulo"), FALSE, FALSE, 0);
    *lbl = gui_label("-", "kpi-valor");
    gtk_box_pack_start(GTK_BOX(box), *lbl, FALSE, FALSE, 0);
    gtk_widget_set_hexpand(box, TRUE);
    return box;
}

GtkWidget *pg_financeiro_criar(void)
{
    static const char *tit[] = {"ID", "TIPO", "DESCRIÇÃO", "CLIENTE", "VENCIMENTO", "VALOR", "PAGO",
                                "SALDO", "STATUS"};
    GtkWidget *acoes;
    GtkWidget *vbox = gui_pagina("Financeiro & Caixa", "Contas a pagar e a receber, baixas e fluxo de caixa projetado", &acoes);
    gboolean ed = auth_pode(G.perfil, MOD_FINANCEIRO, ACAO_EDITAR);
    GtkWidget *filtro = gtk_combo_box_text_new();
    GtkWidget *bb = gui_botao("Emitir boleto", "x-office-document-symbolic", NULL);
    GtkWidget *bx = gui_botao("Baixar título", "emblem-ok-symbolic", NULL);
    GtkWidget *bn = gui_botao("Novo título", "list-add-symbolic", "primario");
    GtkWidget *kpis = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    const char *op[] = {"Todos", "A receber", "A pagar", "Em aberto", "Vencidos"};
    int i;

    for (i = 0; i < 5; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filtro), op[i]);
    }
    g_fin_filtro = 0;
    gtk_combo_box_set_active(GTK_COMBO_BOX(filtro), 0);
    g_signal_connect(filtro, "changed", G_CALLBACK(fin_filtro_mudou), NULL);
    g_signal_connect(bb, "clicked", G_CALLBACK(fin_boleto), NULL);
    g_signal_connect(bx, "clicked", G_CALLBACK(fin_baixar), NULL);
    g_signal_connect(bn, "clicked", G_CALLBACK(fin_novo), NULL);
    gtk_widget_set_sensitive(bb, ed);
    gtk_widget_set_sensitive(bx, ed);
    gtk_widget_set_sensitive(bn, ed);
    gtk_box_pack_start(GTK_BOX(acoes), filtro, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), bb, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), bx, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), bn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(kpis), mini_kpi("TOTAL A RECEBER", &lb_receber), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(kpis), mini_kpi("TOTAL A PAGAR", &lb_pagar), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(kpis), mini_kpi("SALDO PROJETADO", &lb_saldo), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), kpis, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gui_tabela(9, tit, "lllllrrrl", 8, fin_pred, &st_fin, &tv_fin),
                       TRUE, TRUE, 0);
    G.atualizar[MOD_FINANCEIRO] = financeiro_atualizar;
    financeiro_atualizar();
    return vbox;
}

/* ================================================================== */
/* NF-e & Boletos                                                      */
/* ================================================================== */
static GtkListStore *st_nfe;
static GtkWidget *tv_nfe;

static void nfe_atualizar(void)
{
    int32_t i;

    gtk_list_store_clear(st_nfe);
    for (i = g_num_vendas - 1; i >= 0; i--) {
        const Venda *v = &g_vendas[i];
        const Cliente *c;
        char nf[16], vid[16], data[TAM_DATA], total[TAM_MOEDA];
        const char *cols[6];

        if (v->status != VENDA_FATURADA && v->status != VENDA_CANCELADA) {
            continue;
        }
        if (v->numero_nf <= 0) {
            continue;
        }
        c = cadastros_buscar_cliente_id(v->cliente_id);
        snprintf(nf, sizeof nf, "%d", (int)v->numero_nf);
        snprintf(vid, sizeof vid, "%d", (int)v->id);
        data_formatar(v->data_emissao, data, sizeof data);
        util_formatar_moeda(v->total_liquido_centavos, total, sizeof total);
        cols[0] = nf;
        cols[1] = vid;
        cols[2] = data;
        cols[3] = c != NULL ? c->nome : "(removido)";
        cols[4] = total;
        cols[5] = v->chave_nfe;
        gui_store_add(st_nfe, cols, 6);
    }
}

static const char *chave_selecionada(void)
{
    int nf;
    int32_t i;

    if (!gui_selecionado(tv_nfe, &nf)) {
        return NULL;
    }
    for (i = 0; i < g_num_vendas; i++) {
        if (g_vendas[i].numero_nf == nf) {
            return g_vendas[i].chave_nfe;
        }
    }
    return NULL;
}

static void nfe_copiar(GtkButton *b, gpointer d)
{
    const char *ch = chave_selecionada();

    (void)b; (void)d;
    if (ch != NULL) {
        gui_copiar(ch);
        gui_status("Chave de acesso copiada para a área de transferência.");
    }
}

static void nfe_validar_sel(GtkButton *b, gpointer d)
{
    const char *ch = chave_selecionada();

    (void)b; (void)d;
    if (ch != NULL) {
        gui_msg(GTK_WINDOW(G.janela),
                fiscal_validar_chave_nfe(ch) ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR,
                fiscal_validar_chave_nfe(ch) ? "Chave válida" : "Chave inválida",
                fiscal_validar_chave_nfe(ch)
                    ? "O dígito verificador (Módulo 11 SEFAZ) confere."
                    : "O dígito verificador não confere.");
    }
}

/* ---- ferramentas ---- */
static void tool_gerar_chave(GtkButton *b, gpointer d)
{
    char uf[16], ano[16], mes[16], mod[16], ser[16], tp[16], cnf[16];
    int32_t hoje = data_hoje();
    Campo c[9];
    char v[9][160];
    int64_t n[9];
    int k;
    char chave[TAM_CHAVE_NFE];
    char txt[400];
    int rc;

    (void)b; (void)d;
    if (!gui_pode(MOD_FISCAL, ACAO_EDITAR, TRUE)) {
        return;
    }
    snprintf(uf, sizeof uf, "%d", (int)g_empresa.uf_codigo_ibge);
    snprintf(ano, sizeof ano, "%d", (int)(hoje / 10000));
    snprintf(mes, sizeof mes, "%d", (int)((hoje / 100) % 100));
    snprintf(mod, sizeof mod, "%d", (int)g_empresa.modelo_nfe);
    snprintf(ser, sizeof ser, "%d", (int)g_empresa.serie_nfe);
    snprintf(tp, sizeof tp, "%d", (int)g_empresa.tipo_emissao);
    snprintf(cnf, sizeof cnf, "%d", 12345678);
    c[0] = (Campo){"UF (código IBGE)", CAMPO_TEXTO, uf, NULL};
    c[1] = (Campo){"Ano de emissão", CAMPO_TEXTO, ano, NULL};
    c[2] = (Campo){"Mês de emissão", CAMPO_TEXTO, mes, NULL};
    c[3] = (Campo){"CNPJ do emitente", CAMPO_TEXTO, g_empresa.cnpj, NULL};
    c[4] = (Campo){"Modelo", CAMPO_TEXTO, mod, NULL};
    c[5] = (Campo){"Série", CAMPO_TEXTO, ser, NULL};
    c[6] = (Campo){"Número da NF", CAMPO_TEXTO, "1", NULL};
    c[7] = (Campo){"Tipo de emissão", CAMPO_TEXTO, tp, NULL};
    c[8] = (Campo){"Código numérico (8 dígitos)", CAMPO_TEXTO, cnf, NULL};
    if (!gui_form("Gerar chave de acesso NF-e", "Gerar", c, 9, v)) {
        return;
    }
    for (k = 0; k < 9; k++) {
        if (k == 3) {
            continue;
        }
        if (!util_parse_int64(v[k], &n[k])) {
            gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Dados inválidos", "Os campos numéricos aceitam apenas números inteiros.");
            return;
        }
    }
    rc = fiscal_gerar_chave_nfe((int)n[0], (int)n[1], (int)n[2], v[3], (int)n[4], (int)n[5],
                                (int32_t)n[6], (int)n[7], (int32_t)n[8], chave, sizeof chave);
    if (rc != ERP_OK) {
        gui_erro_rc(rc);
        return;
    }
    snprintf(txt, sizeof txt,
             "Chave de acesso (44 dígitos):\n%s\n\nEstrutura:\n  UF %.2s | AAMM %.4s | CNPJ %.14s\n"
             "  modelo %.2s | série %.3s | nº %.9s | tpEmis %.1s | cNF %.8s | DV %.1s\n",
             chave, chave, chave + 2, chave + 6, chave + 20, chave + 22, chave + 25, chave + 34,
             chave + 35, chave + 43);
    gui_registrar("Chave NF-e gerada (ferramenta)");
    gui_mostrar_texto("Chave de acesso NF-e", txt);
}

static void tool_validar_chave(GtkButton *b, gpointer d)
{
    Campo c[] = {{"Chave de acesso (44 dígitos)", CAMPO_TEXTO, "", NULL}};
    char v[1][160];
    char dig[64];

    (void)b; (void)d;
    if (!gui_form("Validar chave de acesso", "Validar", c, 1, v)) {
        return;
    }
    if (fiscal_somente_digitos(v[0], dig, sizeof dig) < 0) {
        dig[0] = '\0';
    }
    gui_msg(GTK_WINDOW(G.janela), fiscal_validar_chave_nfe(dig) ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR,
            fiscal_validar_chave_nfe(dig) ? "Chave válida" : "Chave inválida",
            fiscal_validar_chave_nfe(dig) ? "44 dígitos e DV (Módulo 11 SEFAZ) corretos."
                                          : "Tamanho ou dígito verificador incorretos.");
}

static void tool_gerar_boleto(GtkButton *b, gpointer d)
{
    Campo c[] = {
        {"Banco (3 dígitos)", CAMPO_TEXTO, "341", NULL},
        {"Campo livre (25 dígitos)", CAMPO_TEXTO, "1234045678910900000000001", NULL},
        {"Valor (R$)", CAMPO_TEXTO, "", NULL},
        {"Vencimento (dd/mm/aaaa)", CAMPO_TEXTO, "", NULL}
    };
    char v[4][160];
    int64_t valor = 0;
    int32_t venc = 0;
    char linha[TAM_LINHA_DIG];
    char barras[TAM_CODIGO_BARRAS];
    char txt[500];
    int rc;

    (void)b; (void)d;
    if (!gui_pode(MOD_FISCAL, ACAO_EDITAR, TRUE) || !gui_form("Gerar linha digitável", "Gerar", c, 4, v)) {
        return;
    }
    if (!util_parse_centavos(v[2], &valor) || !data_parse_br(v[3], &venc) || venc == 0) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Dados inválidos", "Verifique o valor e o vencimento.");
        return;
    }
    rc = fiscal_gerar_linha_digitavel(v[0], '9', v[1], venc, valor, linha, sizeof linha, barras, sizeof barras);
    if (rc != ERP_OK) {
        gui_erro_rc(rc);
        return;
    }
    snprintf(txt, sizeof txt, "Linha digitável:\n%s\n\nCódigo de barras:\n%s\n\nFator de vencimento: %04d\n",
             linha, barras, fiscal_calcular_fator_vencimento(venc));
    gui_mostrar_texto("Linha digitável do boleto", txt);
}

static void tool_validar_linha(GtkButton *b, gpointer d)
{
    Campo c[] = {{"Linha digitável (47 dígitos)", CAMPO_TEXTO, "", NULL}};
    char v[1][160];

    (void)b; (void)d;
    if (!gui_form("Validar linha digitável", "Validar", c, 1, v)) {
        return;
    }
    gui_msg(GTK_WINDOW(G.janela),
            fiscal_validar_linha_digitavel(v[0]) ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR,
            fiscal_validar_linha_digitavel(v[0]) ? "Linha válida" : "Linha inválida",
            fiscal_validar_linha_digitavel(v[0])
                ? "Os 3 DVs (Módulo 10) e o DV geral (Módulo 11) conferem."
                : "Algum dígito verificador não confere ou o tamanho está incorreto.");
}

static void tool_dv11(GtkButton *b, gpointer d)
{
    Campo c[] = {{"Dígitos (43 para uma chave NF-e)", CAMPO_TEXTO, "", NULL}};
    char v[1][160];
    char dig[96];
    char txt[160];
    int dv;

    (void)b; (void)d;
    if (!gui_form("DV Módulo 11 (SEFAZ)", "Calcular", c, 1, v)) {
        return;
    }
    if (fiscal_somente_digitos(v[0], dig, sizeof dig) <= 0) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Entrada inválida", "Informe apenas dígitos.");
        return;
    }
    dv = fiscal_calcular_dv_mod11_sefaz(dig);
    snprintf(txt, sizeof txt, "DV Módulo 11 (pesos 2 a 9) de %.60s = %d", dig, dv);
    gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_INFO, "Resultado", txt);
}

static void tool_doc(GtkButton *b, gpointer d)
{
    Campo c[] = {{"CPF ou CNPJ", CAMPO_TEXTO, "", NULL}};
    char v[1][160];
    char dig[TAM_DOC];
    int n;
    gboolean ok = FALSE;

    (void)b; (void)d;
    if (!gui_form("Validar CPF / CNPJ", "Validar", c, 1, v)) {
        return;
    }
    n = fiscal_somente_digitos(v[0], dig, sizeof dig);
    if (n == 11) {
        ok = fiscal_validar_cpf(dig);
    } else if (n == 14) {
        ok = fiscal_validar_cnpj(dig);
    }
    gui_msg(GTK_WINDOW(G.janela), ok ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR,
            ok ? "Documento válido" : "Documento inválido",
            ok ? "Os dígitos verificadores conferem." : "Tamanho ou dígitos verificadores incorretos.");
}

static GtkWidget *tool_card(const char *titulo, const char *desc, const char *botao, GCallback cb, gboolean ativo)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *l = gui_label(desc, "mudo");
    GtkWidget *bt = gui_botao(botao, NULL, NULL);

    gtk_style_context_add_class(gtk_widget_get_style_context(box), "card");
    gtk_widget_set_size_request(box, 300, -1);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(l), 40);
    gtk_box_pack_start(GTK_BOX(box), gui_label(titulo, "card-titulo"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), l, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), bt, FALSE, FALSE, 0);
    gtk_widget_set_halign(bt, GTK_ALIGN_START);
    g_signal_connect(bt, "clicked", cb, NULL);
    gtk_widget_set_sensitive(bt, ativo);
    return box;
}

GtkWidget *pg_fiscal_criar(void)
{
    static const char *tit[] = {"NF-e Nº", "VENDA", "EMISSÃO", "CLIENTE", "VALOR", "CHAVE DE ACESSO (44 DÍGITOS)"};
    GtkWidget *acoes;
    GtkWidget *vbox = gui_pagina("NF-e & Boletos", "Chave de acesso (Módulo 11 SEFAZ) e linha digitável (FEBRABAN Módulo 10/11)", &acoes);
    GtkWidget *nb = gtk_notebook_new();
    GtkWidget *aba1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *flow = gtk_flow_box_new();
    GtkWidget *bv = gui_botao("Validar chave", "emblem-ok-symbolic", NULL);
    GtkWidget *bc = gui_botao("Copiar chave", "edit-copy-symbolic", NULL);
    gboolean ed = auth_pode(G.perfil, MOD_FISCAL, ACAO_EDITAR);

    g_signal_connect(bv, "clicked", G_CALLBACK(nfe_validar_sel), NULL);
    g_signal_connect(bc, "clicked", G_CALLBACK(nfe_copiar), NULL);
    gtk_box_pack_start(GTK_BOX(acoes), bv, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), bc, FALSE, FALSE, 0);

    gtk_container_set_border_width(GTK_CONTAINER(aba1), 10);
    gtk_box_pack_start(GTK_BOX(aba1), gui_tabela(6, tit, "rrlllL", -1, NULL, &st_nfe, &tv_nfe), TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), aba1, gtk_label_new("NF-e emitidas"));

    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 3);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 14);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 14);
    gtk_container_set_border_width(GTK_CONTAINER(flow), 14);
    gtk_container_add(GTK_CONTAINER(flow), tool_card("Gerar chave de acesso", "Monta os 43 dígitos (UF, AAMM, CNPJ, modelo, série, número, tipo de emissão e código) e calcula o DV Módulo 11.", "Gerar chave", G_CALLBACK(tool_gerar_chave), ed));
    gtk_container_add(GTK_CONTAINER(flow), tool_card("Validar chave de acesso", "Confere o tamanho de 44 dígitos e o dígito verificador.", "Validar chave", G_CALLBACK(tool_validar_chave), TRUE));
    gtk_container_add(GTK_CONTAINER(flow), tool_card("DV Módulo 11 (SEFAZ)", "Calcula o dígito verificador com pesos de 2 a 9 para qualquer sequência numérica.", "Calcular DV", G_CALLBACK(tool_dv11), TRUE));
    gtk_container_add(GTK_CONTAINER(flow), tool_card("Gerar linha digitável", "Gera a linha digitável (3 campos Módulo 10 + DV geral Módulo 11 + fator e valor) e o código de barras.", "Gerar linha", G_CALLBACK(tool_gerar_boleto), ed));
    gtk_container_add(GTK_CONTAINER(flow), tool_card("Validar linha digitável", "Confere os três DVs de campo e o DV geral do código de barras.", "Validar linha", G_CALLBACK(tool_validar_linha), TRUE));
    gtk_container_add(GTK_CONTAINER(flow), tool_card("Validar CPF / CNPJ", "Verifica os dígitos verificadores de documentos.", "Validar documento", G_CALLBACK(tool_doc), TRUE));
    {
        GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

        gtk_container_add(GTK_CONTAINER(sw), flow);
        gtk_notebook_append_page(GTK_NOTEBOOK(nb), sw, gtk_label_new("Ferramentas fiscais"));
    }
    gtk_box_pack_start(GTK_BOX(vbox), nb, TRUE, TRUE, 0);
    G.atualizar[MOD_FISCAL] = nfe_atualizar;
    nfe_atualizar();
    return vbox;
}

/* ================================================================== */
/* Usuarios                                                            */
/* ================================================================== */
static GtkListStore *st_usr;
static GtkWidget *tv_usr;
#define OPCOES_PERFIL "Administrador|Gerente|Fiscal|Financeiro"

static PerfilAcesso perfil_de_nome(const char *n)
{
    if (!strcmp(n, "Administrador")) return PERFIL_ADMIN;
    if (!strcmp(n, "Gerente")) return PERFIL_GERENTE;
    if (!strcmp(n, "Fiscal")) return PERFIL_FISCAL;
    return PERFIL_FINANCEIRO;
}

static void usuarios_atualizar(void)
{
    int32_t i;

    gtk_list_store_clear(st_usr);
    for (i = 0; i < g_num_usuarios; i++) {
        const Usuario *u = &g_usuarios[i];
        char id[16], ac[TAM_DATA];
        const char *cols[6];

        snprintf(id, sizeof id, "%d", (int)u->id);
        if (u->ultimo_acesso > 0) {
            data_formatar(u->ultimo_acesso, ac, sizeof ac);
        } else {
            snprintf(ac, sizeof ac, "nunca");
        }
        cols[0] = id;
        cols[1] = u->login;
        cols[2] = u->nome;
        cols[3] = auth_nome_perfil(u->perfil);
        cols[4] = u->ativo ? "Ativo" : "Inativo";
        cols[5] = ac;
        gui_store_add(st_usr, cols, 6);
    }
}

static void usr_gravar(const char *ev)
{
    if (auth_salvar() != ERP_OK) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Falha ao gravar", "Não foi possível gravar usuarios.dat.");
    }
    gui_gravar(ev);
}

static void usr_novo(GtkButton *b, gpointer d)
{
    Campo c[] = {
        {"Login (3 a 31: letras, números, . _ -)", CAMPO_TEXTO, "", NULL},
        {"Nome completo", CAMPO_TEXTO, "", NULL},
        {"Senha inicial (mín. 6 caracteres)", CAMPO_SENHA, "", NULL},
        {"Perfil de acesso", CAMPO_COMBO, "Gerente", OPCOES_PERFIL}
    };
    char v[4][160];
    int rc;
    char ev[200];

    (void)b; (void)d;
    if (!gui_pode(MOD_USUARIOS, ACAO_EDITAR, TRUE) || !gui_form("Novo usuário", "Criar usuário", c, 4, v)) {
        return;
    }
    rc = auth_criar_usuario(v[0], v[1], v[2], perfil_de_nome(v[3]));
    memset(v[2], 0, sizeof v[2]);
    if (rc < 0) {
        gui_erro_rc(rc);
        return;
    }
    snprintf(ev, sizeof ev, "Usuario criado: %.30s (%.20s)", v[0], v[3]);
    usr_gravar(ev);
}

static void usr_senha(GtkButton *b, gpointer d)
{
    int uid;
    Campo c[] = {{"Nova senha temporária (mín. 6)", CAMPO_SENHA, "", NULL}};
    char v[1][160];
    int rc;
    char ev[160];

    (void)b; (void)d;
    if (!gui_pode(MOD_USUARIOS, ACAO_EDITAR, TRUE) || !gui_selecionado(tv_usr, &uid) ||
        !gui_form("Redefinir senha", "Redefinir", c, 1, v)) {
        return;
    }
    rc = auth_trocar_senha(uid, v[0], TRUE);
    memset(v[0], 0, sizeof v[0]);
    if (rc != ERP_OK) {
        gui_erro_rc(rc);
        return;
    }
    snprintf(ev, sizeof ev, "Senha redefinida para o usuario %d (troca obrigatoria)", uid);
    usr_gravar(ev);
}

static void usr_perfil(GtkButton *b, gpointer d)
{
    int uid;
    Usuario *u;
    Campo c[] = {{"Novo perfil", CAMPO_COMBO, NULL, OPCOES_PERFIL}};
    char v[1][160];
    int rc;
    char ev[160];

    (void)b; (void)d;
    if (!gui_pode(MOD_USUARIOS, ACAO_EDITAR, TRUE) || !gui_selecionado(tv_usr, &uid)) {
        return;
    }
    u = auth_buscar_id(uid);
    if (u == NULL) {
        return;
    }
    c[0].inicial = auth_nome_perfil(u->perfil);
    if (!gui_form("Alterar perfil", "Salvar", c, 1, v)) {
        return;
    }
    rc = auth_alterar_perfil(uid, perfil_de_nome(v[0]));
    if (rc != ERP_OK) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Não foi possível alterar",
                rc == ERP_ERR_ESTADO ? "O sistema precisa de pelo menos um administrador ativo."
                                     : erp_mensagem_erro(rc));
        return;
    }
    snprintf(ev, sizeof ev, "Perfil do usuario %d alterado para %.20s", uid, v[0]);
    usr_gravar(ev);
}

static void usr_ativo(GtkButton *b, gpointer d)
{
    int uid;
    Usuario *u;
    int rc;
    char ev[160];

    (void)b; (void)d;
    if (!gui_pode(MOD_USUARIOS, ACAO_EDITAR, TRUE) || !gui_selecionado(tv_usr, &uid)) {
        return;
    }
    u = auth_buscar_id(uid);
    if (u == NULL) {
        return;
    }
    if (u == G.usuario) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_WARNING, "Operação não permitida",
                "Você não pode desativar o próprio usuário.");
        return;
    }
    rc = auth_definir_ativo(uid, !u->ativo);
    if (rc != ERP_OK) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Não foi possível alterar",
                "O sistema precisa de pelo menos um administrador ativo.");
        return;
    }
    snprintf(ev, sizeof ev, "Usuario %d %s", uid, u->ativo ? "ativado" : "desativado");
    usr_gravar(ev);
}

GtkWidget *pg_usuarios_criar(void)
{
    static const char *tit[] = {"ID", "LOGIN", "NOME", "PERFIL", "STATUS", "ÚLTIMO ACESSO"};
    GtkWidget *acoes;
    GtkWidget *vbox = gui_pagina("Usuários & Acesso", "Perfis: Administrador, Gerente, Fiscal e Financeiro", &acoes);
    GtkWidget *b1 = gui_botao("Ativar / desativar", "system-lock-screen-symbolic", NULL);
    GtkWidget *b2 = gui_botao("Alterar perfil", "avatar-default-symbolic", NULL);
    GtkWidget *b3 = gui_botao("Redefinir senha", "dialog-password-symbolic", NULL);
    GtkWidget *b4 = gui_botao("Novo usuário", "list-add-symbolic", "primario");
    GtkWidget *info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *l = gui_label("Permissões — Administrador: tudo. Gerente: clientes, estoque e vendas (edita); financeiro e fiscal (consulta). "
                             "Fiscal: NF-e e boletos (edita); clientes, estoque e vendas (consulta). "
                             "Financeiro: financeiro (edita); clientes, vendas e NF-e (consulta).", "mudo");

    g_signal_connect(b1, "clicked", G_CALLBACK(usr_ativo), NULL);
    g_signal_connect(b2, "clicked", G_CALLBACK(usr_perfil), NULL);
    g_signal_connect(b3, "clicked", G_CALLBACK(usr_senha), NULL);
    g_signal_connect(b4, "clicked", G_CALLBACK(usr_novo), NULL);
    gtk_box_pack_start(GTK_BOX(acoes), b1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), b2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), b3, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), b4, FALSE, FALSE, 0);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(info), "card");
    gtk_box_pack_start(GTK_BOX(info), l, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), info, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gui_tabela(6, tit, "llllll", 4, NULL, &st_usr, &tv_usr), TRUE, TRUE, 0);
    G.atualizar[MOD_USUARIOS] = usuarios_atualizar;
    usuarios_atualizar();
    return vbox;
}

/* ================================================================== */
/* Configuracoes & Backup                                              */
/* ================================================================== */
static gboolean copiar_arquivo(const char *src, const char *dst)
{
    FILE *a = fopen(src, "rb");
    FILE *b;
    char buf[8192];
    size_t n;
    gboolean ok = TRUE;

    if (a == NULL) {
        return FALSE;
    }
    b = fopen(dst, "wb");
    if (b == NULL) {
        fclose(a);
        return FALSE;
    }
    while ((n = fread(buf, 1, sizeof buf, a)) > 0) {
        if (fwrite(buf, 1, n, b) != n) {
            ok = FALSE;
            break;
        }
    }
    fclose(a);
    if (fclose(b) != 0) {
        ok = FALSE;
    }
    return ok;
}

static void cfg_salvar(GtkButton *b, gpointer d)
{
    int rc;

    (void)b; (void)d;
    if (!gui_pode(MOD_CONFIG, ACAO_EDITAR, TRUE)) {
        return;
    }
    rc = persistencia_salvar_dados();
    if (rc == ERP_OK) {
        gui_registrar("Dados gravados manualmente");
        gui_status("Dados gravados em clientes.dat, produtos.dat, vendas.dat e titulos.dat.");
    } else {
        gui_erro_rc(rc);
    }
}

static void cfg_carregar(GtkButton *b, gpointer d)
{
    int rc;

    (void)b; (void)d;
    if (!gui_pode(MOD_CONFIG, ACAO_EDITAR, TRUE) ||
        !gui_confirmar("Recarregar os dados do disco? Alterações não gravadas serão perdidas.")) {
        return;
    }
    rc = persistencia_carregar_dados();
    if (rc != ERP_OK) {
        gui_erro_rc(rc);
        return;
    }
    gui_registrar("Dados recarregados do disco");
    gui_atualizar_tudo();
    gui_status("Dados recarregados do disco.");
}

static void cfg_backup(GtkButton *b, gpointer d)
{
    static const char *arqs[] = {ARQ_CLIENTES, ARQ_PRODUTOS, ARQ_VENDAS, ARQ_TITULOS, ARQ_USUARIOS};
    time_t agora = time(NULL);
    struct tm *tmv = localtime(&agora);
    char ts[32] = "00000000_000000";
    char dst[160];
    char msg[160];
    int i;
    int ok = 0;

    (void)b; (void)d;
    if (!gui_pode(MOD_CONFIG, ACAO_EDITAR, TRUE)) {
        return;
    }
    persistencia_salvar_dados();
    if (tmv != NULL) {
        strftime(ts, sizeof ts, "%Y%m%d_%H%M%S", tmv);
    }
    for (i = 0; i < 5; i++) {
        snprintf(dst, sizeof dst, "backup_%s_%s", ts, arqs[i]);
        if (copiar_arquivo(arqs[i], dst)) {
            ok++;
        }
    }
    snprintf(msg, sizeof msg, "Backup concluído: %d de 5 arquivos copiados (prefixo backup_%s_).", ok, ts);
    gui_registrar("Backup dos arquivos .dat");
    gui_msg(GTK_WINDOW(G.janela), ok == 5 ? GTK_MESSAGE_INFO : GTK_MESSAGE_WARNING, "Backup", msg);
}

static void cfg_demo(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    if (!gui_pode(MOD_CONFIG, ACAO_EDITAR, TRUE) ||
        !gui_confirmar("Isso APAGA clientes, produtos, vendas e títulos atuais e carrega os dados de demonstração. Continuar?")) {
        return;
    }
    sistema_inicializar_dados_padrao();
    gui_gravar("Dados de demonstracao restaurados");
}

static void cfg_log(GtkButton *b, gpointer d)
{
    FILE *f;
    char *buf;
    long n;
    long ini;
    int linhas = 0;
    long i;

    (void)b; (void)d;
    if (!gui_pode(MOD_CONFIG, ACAO_VER, TRUE)) {
        return;
    }
    f = fopen(ARQ_AUDITORIA, "rb");
    if (f == NULL) {
        gui_mostrar_texto("Log de auditoria", "(vazio)");
        return;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    if (n > 200000) {
        fseek(f, n - 200000, SEEK_SET);
        n = 200000;
    } else {
        rewind(f);
    }
    buf = g_malloc0((gsize)n + 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        n = 0;
    }
    buf[n] = '\0';
    fclose(f);
    ini = 0;
    for (i = n - 1; i >= 0; i--) {
        if (buf[i] == '\n' && ++linhas > 200) {
            ini = i + 1;
            break;
        }
    }
    gui_mostrar_texto("Log de auditoria (últimas 200 linhas)", buf + ini);
    g_free(buf);
}

#ifdef USE_SQLSERVER
static void cfg_sql_salvar(GtkButton *b, gpointer d)
{
    int rc;

    (void)b; (void)d;
    if (!gui_pode(MOD_CONFIG, ACAO_EDITAR, TRUE)) {
        return;
    }
    rc = sql_salvar_dados();
    if (rc == ERP_OK) {
        gui_registrar("Dados enviados ao SQL Server");
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_INFO, "SQL Server", "Dados gravados no banco AuroraERP.");
    } else {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "SQL Server",
                "Falha na gravação. Confira AURORA_SQL_CONN e a saída do terminal ([SQL]).");
    }
}

static void cfg_sql_carregar(GtkButton *b, gpointer d)
{
    int rc;

    (void)b; (void)d;
    if (!gui_pode(MOD_CONFIG, ACAO_EDITAR, TRUE) ||
        !gui_confirmar("Substituir os dados atuais pelos do SQL Server?")) {
        return;
    }
    rc = sql_carregar_dados();
    if (rc == ERP_OK) {
        gui_gravar("Dados carregados do SQL Server");
    } else {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "SQL Server",
                "Falha na leitura. Confira AURORA_SQL_CONN e a saída do terminal ([SQL]).");
    }
}
#endif

static GtkWidget *cfg_card(const char *titulo, const char *desc)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *l = gui_label(desc, "mudo");

    gtk_style_context_add_class(gtk_widget_get_style_context(box), "card");
    gtk_widget_set_size_request(box, 320, -1);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(l), 42);
    gtk_box_pack_start(GTK_BOX(box), gui_label(titulo, "card-titulo"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
    return box;
}

static void cfg_botao(GtkWidget *card, const char *txt, GCallback cb, const char *classe)
{
    GtkWidget *b = gui_botao(txt, NULL, classe);

    g_signal_connect(b, "clicked", cb, NULL);
    gtk_widget_set_halign(b, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(card), b, FALSE, FALSE, 0);
}

GtkWidget *pg_config_criar(void)
{
    GtkWidget *vbox = gui_pagina("Configurações & Backup", "Persistência em arquivos .dat, backup e trilha de auditoria", NULL);
    GtkWidget *flow = gtk_flow_box_new();
    GtkWidget *c1 = cfg_card("Gravação dos dados", "As alterações são gravadas automaticamente em clientes.dat, produtos.dat, vendas.dat e titulos.dat.");
    GtkWidget *c2 = cfg_card("Backup", "Copia todos os arquivos .dat para a pasta atual com data e hora no nome.");
    GtkWidget *c3 = cfg_card("Auditoria", "Registro de logins, falhas, acessos negados e operações (auditoria.log).");
    GtkWidget *c4 = cfg_card("Dados de demonstração", "Substitui clientes, produtos, vendas e títulos por um conjunto de exemplo. Os usuários não são alterados.");
    char sobre[400];
    GtkWidget *c5;

    snprintf(sobre, sizeof sobre, "Aurora ERP — PIM II UNIP\nNúcleo em C11 • GTK %u.%u.%u\nAutor: Gabriel Abreu Ribeiro",
             gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version());
    c5 = cfg_card("Sobre", sobre);
    cfg_botao(c1, "Gravar agora", G_CALLBACK(cfg_salvar), NULL);
    cfg_botao(c1, "Recarregar do disco", G_CALLBACK(cfg_carregar), NULL);
    cfg_botao(c2, "Gerar backup", G_CALLBACK(cfg_backup), "primario");
    cfg_botao(c3, "Ver log de auditoria", G_CALLBACK(cfg_log), NULL);
    cfg_botao(c4, "Restaurar demonstração", G_CALLBACK(cfg_demo), "perigo");
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 3);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 14);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 14);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
    gtk_container_add(GTK_CONTAINER(flow), c1);
    gtk_container_add(GTK_CONTAINER(flow), c2);
    gtk_container_add(GTK_CONTAINER(flow), c3);
    gtk_container_add(GTK_CONTAINER(flow), c4);
#ifdef USE_SQLSERVER
    {
        GtkWidget *c6 = cfg_card("SQL Server (ODBC)", "Sincroniza com o banco AuroraERP. Requer a variável AURORA_SQL_CONN.");

        cfg_botao(c6, "Enviar para o SQL Server", G_CALLBACK(cfg_sql_salvar), NULL);
        cfg_botao(c6, "Carregar do SQL Server", G_CALLBACK(cfg_sql_carregar), NULL);
        gtk_container_add(GTK_CONTAINER(flow), c6);
    }
#endif
    gtk_container_add(GTK_CONTAINER(flow), c5);
    gtk_box_pack_start(GTK_BOX(vbox), flow, FALSE, FALSE, 0);
    return vbox;
}
