/*
 * Aurora ERP - gui_pages.c
 * Paginas: Visao Geral, Clientes, Estoque e Vendas.
 */
#include "gui.h"
#include <cairo.h>

#define ESTOQUE_BAIXO 20

/* ================================================================== */
/* Visao Geral                                                         */
/* ================================================================== */
enum { K_FAT, K_RECEBER, K_PAGAR, K_SALDO, K_ESTOQUE, K_CLIENTES, K_NFE, K_TOTAL };

typedef struct {
    GtkWidget *valor;
    GtkWidget *sub;
} Kpi;

typedef struct {
    int     n;
    char    rot[6][32];
    int64_t val[6];
    double  cor[6][3];
    char    titulo[64];
} ChartData;

static Kpi        g_kpi[K_TOTAL];
static ChartData  g_chart;
static GtkWidget *g_chart_area;
static GtkWidget *g_lista_alertas;
static GtkWidget *g_lista_vendas;

static GtkWidget *kpi_card(const char *titulo, Kpi *k)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    gtk_style_context_add_class(gtk_widget_get_style_context(box), "card");
    gtk_widget_set_size_request(box, 230, -1);
    gtk_box_pack_start(GTK_BOX(box), gui_label(titulo, "kpi-titulo"), FALSE, FALSE, 0);
    k->valor = gui_label("-", "kpi-valor");
    k->sub = gui_label("", "kpi-sub");
    gtk_box_pack_start(GTK_BOX(box), k->valor, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), k->sub, FALSE, FALSE, 0);
    return box;
}

static void limpar_lista(GtkWidget *lb)
{
    GList *filhos = gtk_container_get_children(GTK_CONTAINER(lb));
    GList *l;

    for (l = filhos; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(filhos);
}

static void lista_add(GtkWidget *lb, const char *txt)
{
    GtkWidget *l = gui_label(txt, NULL);

    gtk_list_box_insert(GTK_LIST_BOX(lb), l, -1);
    gtk_widget_show(l);
}

static void kpi_set(int k, const char *valor, const char *sub)
{
    if (g_kpi[k].valor != NULL) {
        gtk_label_set_text(GTK_LABEL(g_kpi[k].valor), valor);
        gtk_label_set_text(GTK_LABEL(g_kpi[k].sub), sub);
    }
}

static void dashboard_atualizar(void)
{
    char b1[TAM_MOEDA];
    char b2[TAM_MOEDA];
    char sub[160];
    int32_t hoje = data_hoje();
    int64_t dias_hoje = data_para_dias(hoje);
    int64_t fat = 0;
    int32_t nfat = 0;
    int64_t receber = financeiro_total_aberto(TITULO_RECEBER);
    int64_t pagar = financeiro_total_aberto(TITULO_PAGAR);
    int64_t venc_rec = 0;
    int64_t recebido = 0;
    int64_t pago = 0;
    int32_t prox_pagar = 0;
    int64_t est_valor = 0;
    int32_t baixo = 0;
    int32_t cli_ativos = 0;
    int32_t n_nfe = 0;
    int32_t ult_nfe = 0;
    int32_t i;

    for (i = 0; i < g_num_vendas; i++) {
        const Venda *v = &g_vendas[i];

        if (v->status == VENDA_FATURADA) {
            fat += v->total_liquido_centavos;
            nfat++;
            n_nfe++;
            if (v->numero_nf > ult_nfe) {
                ult_nfe = v->numero_nf;
            }
        }
    }
    for (i = 0; i < g_num_titulos; i++) {
        const TituloFinanceiro *t = &g_titulos[i];

        if (t->status == TITULO_CANCELADO) {
            continue;
        }
        if (t->tipo == TITULO_RECEBER) {
            recebido += t->valor_pago_centavos;
            if (financeiro_saldo_titulo(t) > 0 && data_para_dias(t->data_vencimento) < dias_hoje) {
                venc_rec += financeiro_saldo_titulo(t);
            }
        } else {
            pago += t->valor_pago_centavos;
            if (financeiro_saldo_titulo(t) > 0 &&
                (prox_pagar == 0 || t->data_vencimento < prox_pagar)) {
                prox_pagar = t->data_vencimento;
            }
        }
    }
    for (i = 0; i < g_num_produtos; i++) {
        int64_t v = 0;

        if (util_mul_seguro(g_produtos[i].saldo_qtd, g_produtos[i].cmp_centavos, &v)) {
            est_valor += v;
        }
        if (g_produtos[i].saldo_qtd <= ESTOQUE_BAIXO) {
            baixo++;
        }
    }
    for (i = 0; i < g_num_clientes; i++) {
        if (g_clientes[i].ativo) {
            cli_ativos++;
        }
    }

    util_formatar_moeda(fat, b1, sizeof b1);
    snprintf(sub, sizeof sub, "%d venda(s) faturada(s)", (int)nfat);
    kpi_set(K_FAT, b1, sub);

    util_formatar_moeda(receber, b1, sizeof b1);
    util_formatar_moeda(venc_rec, b2, sizeof b2);
    snprintf(sub, sizeof sub, "Vencido: %s", b2);
    kpi_set(K_RECEBER, b1, sub);

    util_formatar_moeda(pagar, b1, sizeof b1);
    if (prox_pagar > 0) {
        char d[TAM_DATA];

        data_formatar(prox_pagar, d, sizeof d);
        snprintf(sub, sizeof sub, "Próximo vencimento: %s", d);
    } else {
        snprintf(sub, sizeof sub, "Nenhum título em aberto");
    }
    kpi_set(K_PAGAR, b1, sub);

    util_formatar_moeda(receber - pagar, b1, sizeof b1);
    kpi_set(K_SALDO, b1, "A receber − a pagar");

    util_formatar_moeda(est_valor, b1, sizeof b1);
    snprintf(sub, sizeof sub, "%d produto(s) • %d com saldo baixo", (int)g_num_produtos, (int)baixo);
    kpi_set(K_ESTOQUE, b1, sub);

    snprintf(b1, sizeof b1, "%d", (int)cli_ativos);
    snprintf(sub, sizeof sub, "%d cadastrado(s) no total", (int)g_num_clientes);
    kpi_set(K_CLIENTES, b1, sub);

    snprintf(b1, sizeof b1, "%d", (int)n_nfe);
    snprintf(sub, sizeof sub, ult_nfe > 0 ? "Última NF-e nº %d" : "Nenhuma emitida", (int)ult_nfe);
    kpi_set(K_NFE, b1, sub);

    /* grafico */
    memset(&g_chart, 0, sizeof g_chart);
    if (auth_pode(G.perfil, MOD_FINANCEIRO, ACAO_VER)) {
        static const double cores[4][3] = {{0.13, 0.77, 0.37}, {0.94, 0.27, 0.27},
                                           {0.23, 0.51, 0.96}, {0.96, 0.62, 0.04}};
        const char *rot[4] = {"A receber", "A pagar", "Já recebido", "Já pago"};
        int64_t val[4];
        int k;

        val[0] = receber; val[1] = pagar; val[2] = recebido; val[3] = pago;
        g_chart.n = 4;
        snprintf(g_chart.titulo, sizeof g_chart.titulo, "Fluxo financeiro");
        for (k = 0; k < 4; k++) {
            snprintf(g_chart.rot[k], sizeof g_chart.rot[k], "%s", rot[k]);
            g_chart.val[k] = val[k];
            memcpy(g_chart.cor[k], cores[k], sizeof cores[k]);
        }
    } else if (auth_pode(G.perfil, MOD_VENDAS, ACAO_VER)) {
        int64_t soma[MAX_CLIENTES];
        int usado[MAX_CLIENTES];
        int k;
        int r;

        memset(soma, 0, sizeof soma);
        memset(usado, 0, sizeof usado);
        for (i = 0; i < g_num_vendas; i++) {
            if (g_vendas[i].status == VENDA_FATURADA) {
                int32_t c;

                for (c = 0; c < g_num_clientes; c++) {
                    if (g_clientes[c].id == g_vendas[i].cliente_id) {
                        soma[c] += g_vendas[i].total_liquido_centavos;
                    }
                }
            }
        }
        snprintf(g_chart.titulo, sizeof g_chart.titulo, "Faturamento por cliente (top 5)");
        for (r = 0; r < 5; r++) {
            int best = -1;

            for (k = 0; k < g_num_clientes; k++) {
                if (!usado[k] && soma[k] > 0 && (best < 0 || soma[k] > soma[best])) {
                    best = k;
                }
            }
            if (best < 0) {
                break;
            }
            usado[best] = 1;
            snprintf(g_chart.rot[g_chart.n], sizeof g_chart.rot[0], "%.18s", g_clientes[best].nome);
            g_chart.val[g_chart.n] = soma[best];
            g_chart.cor[g_chart.n][0] = 0.23;
            g_chart.cor[g_chart.n][1] = 0.51;
            g_chart.cor[g_chart.n][2] = 0.96;
            g_chart.n++;
        }
    }
    if (g_chart_area != NULL) {
        gtk_widget_queue_draw(g_chart_area);
    }

    /* listas */
    if (g_lista_alertas != NULL) {
        int mostrados = 0;

        limpar_lista(g_lista_alertas);
        for (i = 0; i < g_num_produtos && mostrados < 7; i++) {
            if (g_produtos[i].saldo_qtd <= ESTOQUE_BAIXO) {
                char l[200];

                snprintf(l, sizeof l, "%s • %.30s — saldo %lld", g_produtos[i].codigo,
                         g_produtos[i].descricao, (long long)g_produtos[i].saldo_qtd);
                lista_add(g_lista_alertas, l);
                mostrados++;
            }
        }
        if (mostrados == 0) {
            lista_add(g_lista_alertas, "Nenhum produto com saldo baixo.");
        }
    }
    if (g_lista_vendas != NULL) {
        int mostrados = 0;

        limpar_lista(g_lista_vendas);
        for (i = g_num_vendas - 1; i >= 0 && mostrados < 7; i--) {
            const Venda *v = &g_vendas[i];
            const Cliente *c = cadastros_buscar_cliente_id(v->cliente_id);
            const char *st = v->status == VENDA_FATURADA ? "FATURADA"
                             : v->status == VENDA_CANCELADA ? "CANCELADA" : "ORCAMENTO";
            char l[240];

            util_formatar_moeda(v->total_liquido_centavos, b1, sizeof b1);
            snprintf(l, sizeof l, "#%d  %.26s — %s • %s", (int)v->id,
                     c != NULL ? c->nome : "(removido)", b1, st);
            lista_add(g_lista_vendas, l);
            mostrados++;
        }
        if (mostrados == 0) {
            lista_add(g_lista_vendas, "Nenhuma venda registrada.");
        }
    }
}

static gboolean chart_draw(GtkWidget *w, cairo_t *cr, gpointer data)
{
    int W = gtk_widget_get_allocated_width(w);
    int H = gtk_widget_get_allocated_height(w);
    int64_t maxv = 1;
    int i;
    double base = H - 38.0;
    double topo = 44.0;
    double larg;
    double gap = 22.0;

    (void)data;
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_move_to(cr, 4, 18);
    cairo_show_text(cr, g_chart.titulo);
    if (g_chart.n == 0) {
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12);
        cairo_set_source_rgb(cr, 0.55, 0.55, 0.55);
        cairo_move_to(cr, 4, H / 2.0);
        cairo_show_text(cr, "Sem dados para exibir.");
        return FALSE;
    }
    for (i = 0; i < g_chart.n; i++) {
        if (g_chart.val[i] > maxv) {
            maxv = g_chart.val[i];
        }
    }
    larg = (W - gap * (g_chart.n + 1)) / g_chart.n;
    cairo_set_line_width(cr, 1);
    cairo_set_source_rgb(cr, 0.22, 0.22, 0.22);
    cairo_move_to(cr, 0, base + 0.5);
    cairo_line_to(cr, W, base + 0.5);
    cairo_stroke(cr);
    for (i = 0; i < g_chart.n; i++) {
        double x = gap + i * (larg + gap);
        double h = (double)g_chart.val[i] / (double)maxv * (base - topo);
        char val[TAM_MOEDA];
        cairo_text_extents_t ext;

        if (h < 2 && g_chart.val[i] > 0) {
            h = 2;
        }
        cairo_set_source_rgb(cr, g_chart.cor[i][0], g_chart.cor[i][1], g_chart.cor[i][2]);
        cairo_rectangle(cr, x, base - h, larg, h);
        cairo_fill(cr);
        util_formatar_moeda(g_chart.val[i], val, sizeof val);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 11);
        cairo_set_source_rgb(cr, 0.92, 0.92, 0.92);
        cairo_text_extents(cr, val, &ext);
        cairo_move_to(cr, x + (larg - ext.width) / 2.0, base - h - 6);
        cairo_show_text(cr, val);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11);
        cairo_set_source_rgb(cr, 0.62, 0.62, 0.62);
        cairo_text_extents(cr, g_chart.rot[i], &ext);
        cairo_move_to(cr, x + (larg - ext.width) / 2.0, base + 18);
        cairo_show_text(cr, g_chart.rot[i]);
    }
    return FALSE;
}

GtkWidget *pg_dashboard_criar(void)
{
    char sub[160];
    GtkWidget *vbox;
    GtkWidget *flow = gtk_flow_box_new();
    GtkWidget *linha = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *esq = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *dir = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);

    snprintf(sub, sizeof sub, "Olá, %s. Resumo operacional do ERP Aurora em tempo real.",
             G.usuario != NULL ? G.usuario->nome : "");
    vbox = gui_pagina("Visão Geral", sub, NULL);
    memset(g_kpi, 0, sizeof g_kpi);
    g_chart_area = NULL;
    g_lista_alertas = NULL;
    g_lista_vendas = NULL;

    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 4);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 2);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 14);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 14);

    if (auth_pode(G.perfil, MOD_VENDAS, ACAO_VER)) {
        gtk_container_add(GTK_CONTAINER(flow), kpi_card("FATURAMENTO", &g_kpi[K_FAT]));
    }
    if (auth_pode(G.perfil, MOD_FINANCEIRO, ACAO_VER)) {
        gtk_container_add(GTK_CONTAINER(flow), kpi_card("A RECEBER", &g_kpi[K_RECEBER]));
        gtk_container_add(GTK_CONTAINER(flow), kpi_card("A PAGAR", &g_kpi[K_PAGAR]));
        gtk_container_add(GTK_CONTAINER(flow), kpi_card("SALDO PROJETADO", &g_kpi[K_SALDO]));
    }
    if (auth_pode(G.perfil, MOD_ESTOQUE, ACAO_VER)) {
        gtk_container_add(GTK_CONTAINER(flow), kpi_card("ESTOQUE (CUSTO MÉDIO)", &g_kpi[K_ESTOQUE]));
    }
    if (auth_pode(G.perfil, MOD_CLIENTES, ACAO_VER)) {
        gtk_container_add(GTK_CONTAINER(flow), kpi_card("CLIENTES ATIVOS", &g_kpi[K_CLIENTES]));
    }
    if (auth_pode(G.perfil, MOD_FISCAL, ACAO_VER)) {
        gtk_container_add(GTK_CONTAINER(flow), kpi_card("NF-e EMITIDAS", &g_kpi[K_NFE]));
    }
    gtk_box_pack_start(GTK_BOX(vbox), flow, FALSE, FALSE, 0);

    if (auth_pode(G.perfil, MOD_FINANCEIRO, ACAO_VER) || auth_pode(G.perfil, MOD_VENDAS, ACAO_VER)) {
        gtk_style_context_add_class(gtk_widget_get_style_context(esq), "card");
        g_chart_area = gtk_drawing_area_new();
        gtk_widget_set_size_request(g_chart_area, 480, 260);
        g_signal_connect(g_chart_area, "draw", G_CALLBACK(chart_draw), NULL);
        gtk_box_pack_start(GTK_BOX(esq), g_chart_area, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(linha), esq, TRUE, TRUE, 0);
    }
    if (auth_pode(G.perfil, MOD_ESTOQUE, ACAO_VER)) {
        GtkWidget *c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

        gtk_style_context_add_class(gtk_widget_get_style_context(c), "card");
        gtk_box_pack_start(GTK_BOX(c), gui_label("Alertas de estoque", "card-titulo"), FALSE, FALSE, 0);
        g_lista_alertas = gtk_list_box_new();
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(g_lista_alertas), GTK_SELECTION_NONE);
        gtk_box_pack_start(GTK_BOX(c), g_lista_alertas, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(dir), c, FALSE, FALSE, 0);
    }
    if (auth_pode(G.perfil, MOD_VENDAS, ACAO_VER)) {
        GtkWidget *c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

        gtk_style_context_add_class(gtk_widget_get_style_context(c), "card");
        gtk_box_pack_start(GTK_BOX(c), gui_label("Últimas vendas", "card-titulo"), FALSE, FALSE, 0);
        g_lista_vendas = gtk_list_box_new();
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(g_lista_vendas), GTK_SELECTION_NONE);
        gtk_box_pack_start(GTK_BOX(c), g_lista_vendas, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(dir), c, FALSE, FALSE, 0);
    }
    gtk_widget_set_size_request(dir, 360, -1);
    gtk_box_pack_start(GTK_BOX(linha), dir, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), linha, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(sw), vbox);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    G.atualizar[MOD_DASHBOARD] = dashboard_atualizar;
    dashboard_atualizar();
    return sw;
}

/* ================================================================== */
/* Clientes                                                            */
/* ================================================================== */
static GtkListStore *st_cli;
static GtkWidget *tv_cli;

static void clientes_atualizar(void)
{
    int32_t i;

    gtk_list_store_clear(st_cli);
    for (i = 0; i < g_num_clientes; i++) {
        const Cliente *c = &g_clientes[i];
        char id[16], doc[32], cu[96], lim[TAM_MOEDA], dev[TAM_MOEDA];
        const char *cols[8];

        snprintf(id, sizeof id, "%d", (int)c->id);
        gui_formatar_doc(c->documento, doc, sizeof doc);
        snprintf(cu, sizeof cu, "%s/%s", c->cidade, c->uf);
        if (c->limite_credito_centavos > 0) {
            util_formatar_moeda(c->limite_credito_centavos, lim, sizeof lim);
        } else {
            snprintf(lim, sizeof lim, "Sem limite");
        }
        util_formatar_moeda(financeiro_saldo_devedor_cliente(c->id), dev, sizeof dev);
        cols[0] = id;
        cols[1] = c->tipo == PESSOA_FISICA ? "PF" : "PJ";
        cols[2] = c->nome;
        cols[3] = doc;
        cols[4] = cu;
        cols[5] = lim;
        cols[6] = dev;
        cols[7] = c->ativo ? "Ativo" : "Inativo";
        gui_store_add(st_cli, cols, 8);
    }
}

static void cli_novo(GtkButton *b, gpointer d)
{
    Campo c[] = {
        {"Tipo", CAMPO_COMBO, "Pessoa física", "Pessoa física|Pessoa jurídica"},
        {"Nome / Razão social", CAMPO_TEXTO, "", NULL},
        {"CPF / CNPJ", CAMPO_TEXTO, "", NULL},
        {"E-mail", CAMPO_TEXTO, "", NULL},
        {"Cidade", CAMPO_TEXTO, "", NULL},
        {"UF", CAMPO_TEXTO, "", NULL},
        {"Limite de crédito (R$; 0 = sem limite)", CAMPO_TEXTO, "0", NULL}
    };
    char v[7][160];
    Cliente cl;
    int64_t lim = 0;
    int rc;
    char ev[220];

    (void)b; (void)d;
    if (!gui_pode(MOD_CLIENTES, ACAO_EDITAR, TRUE) ||
        !gui_form("Novo cliente", "Cadastrar", c, 7, v)) {
        return;
    }
    if (!util_parse_centavos(v[6], &lim)) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Valor inválido", "Informe o limite como 1500 ou 1.500,00.");
        return;
    }
    memset(&cl, 0, sizeof cl);
    cl.tipo = strcmp(v[0], "Pessoa jurídica") == 0 ? PESSOA_JURIDICA : PESSOA_FISICA;
    util_copiar_string(cl.nome, sizeof cl.nome, v[1]);
    util_copiar_string(cl.documento, sizeof cl.documento, v[2]);
    util_copiar_string(cl.email, sizeof cl.email, v[3]);
    util_copiar_string(cl.cidade, sizeof cl.cidade, v[4]);
    util_copiar_string(cl.uf, sizeof cl.uf, v[5]);
    cl.limite_credito_centavos = lim;
    rc = cadastros_inserir_cliente(&cl);
    if (rc < 0) {
        gui_erro_rc(rc);
        return;
    }
    snprintf(ev, sizeof ev, "Cliente cadastrado: %.80s", cl.nome);
    gui_gravar(ev);
}

GtkWidget *pg_clientes_criar(void)
{
    static const char *tit[] = {"ID", "TIPO", "NOME / RAZÃO SOCIAL", "DOCUMENTO", "CIDADE/UF",
                                "LIMITE", "SALDO DEVEDOR", "STATUS"};
    GtkWidget *acoes;
    GtkWidget *vbox = gui_pagina("Clientes", "Cadastro de pessoas físicas e jurídicas com limite de crédito", &acoes);
    GtkWidget *b = gui_botao("Novo cliente", "list-add-symbolic", "primario");

    g_signal_connect(b, "clicked", G_CALLBACK(cli_novo), NULL);
    gtk_widget_set_sensitive(b, auth_pode(G.perfil, MOD_CLIENTES, ACAO_EDITAR));
    gtk_box_pack_start(GTK_BOX(acoes), b, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gui_tabela(8, tit, "lllllrrl", 7, NULL, &st_cli, &tv_cli),
                       TRUE, TRUE, 0);
    G.atualizar[MOD_CLIENTES] = clientes_atualizar;
    clientes_atualizar();
    return vbox;
}

/* ================================================================== */
/* Estoque                                                             */
/* ================================================================== */
static GtkListStore *st_est;
static GtkWidget *tv_est;

static void estoque_atualizar(void)
{
    int32_t i;

    gtk_list_store_clear(st_est);
    for (i = 0; i < g_num_produtos; i++) {
        const Produto *p = &g_produtos[i];
        char id[16], saldo[24], cmp[TAM_MOEDA], preco[TAM_MOEDA], tot[TAM_MOEDA];
        int64_t v = 0;
        const char *cols[9];

        (void)util_mul_seguro(p->saldo_qtd, p->cmp_centavos, &v);
        snprintf(id, sizeof id, "%d", (int)p->id);
        snprintf(saldo, sizeof saldo, "%lld", (long long)p->saldo_qtd);
        util_formatar_moeda(p->cmp_centavos, cmp, sizeof cmp);
        util_formatar_moeda(p->preco_venda_centavos, preco, sizeof preco);
        util_formatar_moeda(v, tot, sizeof tot);
        cols[0] = id;
        cols[1] = p->codigo;
        cols[2] = p->descricao;
        cols[3] = p->unidade;
        cols[4] = saldo;
        cols[5] = cmp;
        cols[6] = preco;
        cols[7] = tot;
        cols[8] = p->saldo_qtd == 0 ? "ZERADO" : (p->saldo_qtd <= ESTOQUE_BAIXO ? "Baixo" : "OK");
        gui_store_add(st_est, cols, 9);
    }
}

static void est_novo(GtkButton *b, gpointer d)
{
    Campo c[] = {
        {"Código", CAMPO_TEXTO, "", NULL},
        {"Descrição", CAMPO_TEXTO, "", NULL},
        {"Unidade (UN, KG, PCT...)", CAMPO_TEXTO, "UN", NULL},
        {"Preço de venda (R$)", CAMPO_TEXTO, "", NULL}
    };
    char v[4][160];
    Produto p;
    int64_t preco = 0;
    int rc;
    char ev[220];

    (void)b; (void)d;
    if (!gui_pode(MOD_ESTOQUE, ACAO_EDITAR, TRUE) ||
        !gui_form("Novo produto", "Cadastrar", c, 4, v)) {
        return;
    }
    if (!util_parse_centavos(v[3], &preco)) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Valor inválido", "Informe o preço como 32,90.");
        return;
    }
    memset(&p, 0, sizeof p);
    util_copiar_string(p.codigo, sizeof p.codigo, v[0]);
    util_copiar_string(p.descricao, sizeof p.descricao, v[1]);
    util_copiar_string(p.unidade, sizeof p.unidade, v[2]);
    p.preco_venda_centavos = preco;
    rc = cadastros_inserir_produto(&p);
    if (rc < 0) {
        gui_erro_rc(rc);
        return;
    }
    snprintf(ev, sizeof ev, "Produto cadastrado: %.20s", p.codigo);
    gui_gravar(ev);
}

static char *opcao_produto_selecionado(void)
{
    int id = 0;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv_est));
    GtkTreeModel *m;
    GtkTreeIter it;
    gchar *s = NULL;
    int32_t i;

    if (gtk_tree_selection_get_selected(sel, &m, &it)) {
        gtk_tree_model_get(m, &it, 0, &s, -1);
        id = s != NULL ? atoi(s) : 0;
        g_free(s);
    }
    for (i = 0; i < g_num_produtos; i++) {
        if (g_produtos[i].id == id) {
            return g_strdup_printf("%d - %s - %s", (int)id, g_produtos[i].codigo, g_produtos[i].descricao);
        }
    }
    return NULL;
}

static void est_movimento(gboolean entrada)
{
    GString *op;
    char *pre;
    char v[4][160];
    int64_t qtd = 0;
    int64_t custo = 0;
    int pid;
    int rc;
    char ev[260];
    Campo ce[] = {
        {"Produto", CAMPO_COMBO, NULL, NULL},
        {"Quantidade", CAMPO_TEXTO, "", NULL},
        {"Custo unitário da entrada (R$)", CAMPO_TEXTO, "", NULL}
    };
    Campo cb[] = {
        {"Produto", CAMPO_COMBO, NULL, NULL},
        {"Quantidade", CAMPO_TEXTO, "", NULL},
        {"Motivo", CAMPO_TEXTO, "Ajuste de inventário", NULL}
    };
    Campo *c = entrada ? ce : cb;

    if (!gui_pode(MOD_ESTOQUE, ACAO_EDITAR, TRUE)) {
        return;
    }
    if (g_num_produtos == 0) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_INFO, "Sem produtos", "Cadastre um produto primeiro.");
        return;
    }
    op = g_string_new("");
    gui_opcoes_produtos(op);
    pre = opcao_produto_selecionado();
    c[0].opcoes = op->str;
    c[0].inicial = pre;
    if (!gui_form(entrada ? "Entrada de mercadoria" : "Baixa manual de estoque",
                  entrada ? "Registrar entrada" : "Dar baixa", c, 3, v)) {
        g_string_free(op, TRUE);
        g_free(pre);
        return;
    }
    g_string_free(op, TRUE);
    g_free(pre);
    pid = gui_id_da_opcao(v[0]);
    if (!util_parse_int64(v[1], &qtd)) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Quantidade inválida", "Use um número inteiro.");
        return;
    }
    if (entrada) {
        if (!util_parse_centavos(v[2], &custo)) {
            gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Valor inválido", "Informe o custo como 23,00.");
            return;
        }
        rc = estoque_registrar_entrada(pid, qtd, custo);
    } else {
        rc = estoque_dar_baixa(pid, qtd);
    }
    if (rc != ERP_OK) {
        gui_erro_rc(rc);
        return;
    }
    if (entrada) {
        snprintf(ev, sizeof ev, "Entrada de estoque: produto %d, qtd %lld", pid, (long long)qtd);
    } else {
        snprintf(ev, sizeof ev, "Baixa manual: produto %d, qtd %lld (%.60s)", pid, (long long)qtd, v[2]);
    }
    gui_gravar(ev);
}

static void est_entrada(GtkButton *b, gpointer d) { (void)b; (void)d; est_movimento(TRUE); }
static void est_baixa(GtkButton *b, gpointer d) { (void)b; (void)d; est_movimento(FALSE); }

GtkWidget *pg_estoque_criar(void)
{
    static const char *tit[] = {"ID", "CÓDIGO", "DESCRIÇÃO", "UN", "SALDO", "CMP", "PREÇO VENDA",
                                "VALOR EM ESTOQUE", "SITUAÇÃO"};
    GtkWidget *acoes;
    GtkWidget *vbox = gui_pagina("Estoque", "Saldos e custo médio ponderado (CMP) recalculado a cada entrada", &acoes);
    gboolean ed = auth_pode(G.perfil, MOD_ESTOQUE, ACAO_EDITAR);
    GtkWidget *b1 = gui_botao("Baixa manual", "go-down-symbolic", NULL);
    GtkWidget *b2 = gui_botao("Entrada de mercadoria", "go-up-symbolic", NULL);
    GtkWidget *b3 = gui_botao("Novo produto", "list-add-symbolic", "primario");

    g_signal_connect(b1, "clicked", G_CALLBACK(est_baixa), NULL);
    g_signal_connect(b2, "clicked", G_CALLBACK(est_entrada), NULL);
    g_signal_connect(b3, "clicked", G_CALLBACK(est_novo), NULL);
    gtk_widget_set_sensitive(b1, ed);
    gtk_widget_set_sensitive(b2, ed);
    gtk_widget_set_sensitive(b3, ed);
    gtk_box_pack_start(GTK_BOX(acoes), b1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), b2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), b3, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gui_tabela(9, tit, "llllrrrrl", 8, NULL, &st_est, &tv_est),
                       TRUE, TRUE, 0);
    G.atualizar[MOD_ESTOQUE] = estoque_atualizar;
    estoque_atualizar();
    return vbox;
}

/* ================================================================== */
/* Vendas                                                              */
/* ================================================================== */
static GtkListStore *st_ven;
static GtkWidget *tv_ven;

static const char *status_venda(StatusVenda s)
{
    return s == VENDA_FATURADA ? "FATURADA" : (s == VENDA_CANCELADA ? "CANCELADA" : "ORCAMENTO");
}

static void vendas_atualizar(void)
{
    int32_t i;

    gtk_list_store_clear(st_ven);
    for (i = g_num_vendas - 1; i >= 0; i--) {
        const Venda *v = &g_vendas[i];
        const Cliente *c = cadastros_buscar_cliente_id(v->cliente_id);
        char id[16], data[TAM_DATA], nf[16], itens[16], total[TAM_MOEDA], marg[TAM_MOEDA];
        const char *cols[8];

        snprintf(id, sizeof id, "%d", (int)v->id);
        data_formatar(v->data_emissao, data, sizeof data);
        if (v->numero_nf > 0) {
            snprintf(nf, sizeof nf, "%d", (int)v->numero_nf);
        } else {
            snprintf(nf, sizeof nf, "-");
        }
        snprintf(itens, sizeof itens, "%d", (int)v->num_itens);
        util_formatar_moeda(v->total_liquido_centavos, total, sizeof total);
        if (v->status == VENDA_FATURADA) {
            util_formatar_moeda(v->total_liquido_centavos - v->custo_total_centavos, marg, sizeof marg);
        } else {
            snprintf(marg, sizeof marg, "-");
        }
        cols[0] = id;
        cols[1] = c != NULL ? c->nome : "(removido)";
        cols[2] = data;
        cols[3] = status_venda(v->status);
        cols[4] = nf;
        cols[5] = itens;
        cols[6] = total;
        cols[7] = marg;
        gui_store_add(st_ven, cols, 8);
    }
}

static void ven_novo(GtkButton *b, gpointer d)
{
    GString *op = g_string_new("");
    Campo c[] = {{"Cliente", CAMPO_COMBO, NULL, NULL}};
    char v[1][160];
    int rc;
    char ev[160];

    (void)b; (void)d;
    if (!gui_pode(MOD_VENDAS, ACAO_EDITAR, TRUE)) {
        g_string_free(op, TRUE);
        return;
    }
    gui_opcoes_clientes(op, TRUE, FALSE);
    if (op->len == 0) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_INFO, "Sem clientes", "Cadastre um cliente primeiro.");
        g_string_free(op, TRUE);
        return;
    }
    c[0].opcoes = op->str;
    if (gui_form("Novo pedido de venda", "Criar pedido", c, 1, v)) {
        rc = vendas_criar(gui_id_da_opcao(v[0]));
        if (rc < 0) {
            gui_erro_rc(rc);
        } else {
            snprintf(ev, sizeof ev, "Pedido %d criado (orcamento)", rc);
            gui_gravar(ev);
        }
    }
    g_string_free(op, TRUE);
}

static void ven_item(GtkButton *b, gpointer d)
{
    int vid;
    Venda *vn;
    GString *op;
    Campo c[] = {
        {"Produto", CAMPO_COMBO, NULL, NULL},
        {"Quantidade", CAMPO_TEXTO, "1", NULL},
        {"Desconto (%; ex.: 5 ou 2,5)", CAMPO_TEXTO, "0", NULL}
    };
    char v[3][160];
    int64_t qtd = 0;
    int64_t bp = 0;
    int rc;
    char ev[200];

    (void)b; (void)d;
    if (!gui_pode(MOD_VENDAS, ACAO_EDITAR, TRUE) || !gui_selecionado(tv_ven, &vid)) {
        return;
    }
    vn = vendas_buscar_id(vid);
    if (vn == NULL || vn->status != VENDA_ORCAMENTO) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_WARNING, "Pedido indisponível",
                "Só é possível adicionar itens a pedidos em ORCAMENTO.");
        return;
    }
    op = g_string_new("");
    gui_opcoes_produtos(op);
    c[0].opcoes = op->str;
    if (gui_form("Adicionar item ao pedido", "Adicionar", c, 3, v)) {
        if (!util_parse_int64(v[1], &qtd) || !util_parse_centavos(v[2], &bp) || bp > 10000) {
            gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Dados inválidos",
                    "Quantidade deve ser inteira e o desconto entre 0 e 100%.");
        } else {
            rc = vendas_adicionar_item(vid, gui_id_da_opcao(v[0]), qtd, (int32_t)bp);
            if (rc != ERP_OK) {
                gui_erro_rc(rc);
            } else {
                snprintf(ev, sizeof ev, "Item adicionado ao pedido %d (produto %d, qtd %lld)", vid,
                         gui_id_da_opcao(v[0]), (long long)qtd);
                gui_gravar(ev);
            }
        }
    }
    g_string_free(op, TRUE);
}

static void ven_faturar(GtkButton *b, gpointer d)
{
    int vid;
    Campo c[] = {
        {"Número de parcelas", CAMPO_TEXTO, "1", NULL},
        {"1º vencimento (dd/mm/aaaa; vazio = hoje + 30 dias)", CAMPO_TEXTO, "", NULL}
    };
    char v[2][160];
    int64_t parc = 0;
    int32_t venc = 0;
    int rc;
    char ev[200];

    (void)b; (void)d;
    if (!gui_pode(MOD_VENDAS, ACAO_EDITAR, TRUE) || !gui_selecionado(tv_ven, &vid)) {
        return;
    }
    if (!gui_form("Faturar venda", "Faturar", c, 2, v)) {
        return;
    }
    if (!util_parse_int64(v[0], &parc) || !data_parse_br(v[1], &venc)) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Dados inválidos",
                "Verifique o número de parcelas e a data (dd/mm/aaaa).");
        return;
    }
    rc = vendas_faturar(vid, (int)parc, venc);
    if (rc != ERP_OK) {
        gui_erro_rc(rc);
        return;
    }
    snprintf(ev, sizeof ev, "Venda %d faturada em %d parcela(s) - NF-e %d", vid, (int)parc,
             (int)vendas_buscar_id(vid)->numero_nf);
    gui_gravar(ev);
}

static void ven_cancelar(GtkButton *b, gpointer d)
{
    int vid;
    int rc;
    char ev[120];

    (void)b; (void)d;
    if (!gui_pode(MOD_VENDAS, ACAO_EDITAR, TRUE) || !gui_selecionado(tv_ven, &vid)) {
        return;
    }
    if (!gui_confirmar("Cancelar esta venda? Se estiver faturada, o estoque é estornado e os títulos em aberto são cancelados.")) {
        return;
    }
    rc = vendas_cancelar(vid);
    if (rc != ERP_OK) {
        gui_erro_rc(rc);
        return;
    }
    snprintf(ev, sizeof ev, "Venda %d cancelada", vid);
    gui_gravar(ev);
}

static void ven_detalhes(GtkButton *b, gpointer d)
{
    int vid;
    Venda *v;
    FILE *f;
    long n;
    char *buf;

    (void)b; (void)d;
    if (!gui_selecionado(tv_ven, &vid)) {
        return;
    }
    v = vendas_buscar_id(vid);
    f = tmpfile();
    if (v == NULL || f == NULL) {
        return;
    }
    vendas_imprimir_fp(f, v);
    n = ftell(f);
    rewind(f);
    buf = g_malloc0((gsize)n + 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        buf[0] = '\0';
    }
    fclose(f);
    gui_mostrar_texto("Detalhes da venda", buf);
    g_free(buf);
}

GtkWidget *pg_vendas_criar(void)
{
    static const char *tit[] = {"ID", "CLIENTE", "EMISSÃO", "STATUS", "NF-e", "ITENS", "TOTAL", "MARGEM BRUTA"};
    GtkWidget *acoes;
    GtkWidget *vbox = gui_pagina("Vendas & Pedidos", "Orçamento → faturamento com baixa de estoque, NF-e e contas a receber", &acoes);
    gboolean ed = auth_pode(G.perfil, MOD_VENDAS, ACAO_EDITAR);
    GtkWidget *bd = gui_botao("Detalhes", "document-properties-symbolic", NULL);
    GtkWidget *bc = gui_botao("Cancelar", "process-stop-symbolic", "perigo");
    GtkWidget *bf = gui_botao("Faturar", "emblem-ok-symbolic", NULL);
    GtkWidget *bi = gui_botao("Adicionar item", "list-add-symbolic", NULL);
    GtkWidget *bn = gui_botao("Novo pedido", "document-new-symbolic", "primario");

    g_signal_connect(bd, "clicked", G_CALLBACK(ven_detalhes), NULL);
    g_signal_connect(bc, "clicked", G_CALLBACK(ven_cancelar), NULL);
    g_signal_connect(bf, "clicked", G_CALLBACK(ven_faturar), NULL);
    g_signal_connect(bi, "clicked", G_CALLBACK(ven_item), NULL);
    g_signal_connect(bn, "clicked", G_CALLBACK(ven_novo), NULL);
    gtk_widget_set_sensitive(bc, ed);
    gtk_widget_set_sensitive(bf, ed);
    gtk_widget_set_sensitive(bi, ed);
    gtk_widget_set_sensitive(bn, ed);
    gtk_box_pack_start(GTK_BOX(acoes), bd, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), bc, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), bf, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), bi, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(acoes), bn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gui_tabela(8, tit, "lllllrrr", 3, NULL, &st_ven, &tv_ven),
                       TRUE, TRUE, 0);
    G.atualizar[MOD_VENDAS] = vendas_atualizar;
    vendas_atualizar();
    return vbox;
}
