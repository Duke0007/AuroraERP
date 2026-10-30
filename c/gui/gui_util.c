/*
 * Aurora ERP - gui_util.c
 * Tema escuro (CSS), dialogos, formularios e tabelas reutilizaveis.
 */
#include "gui.h"

Gui G;

static GPtrArray *g_filtros = NULL;
static GPtrArray *g_tvs = NULL;

typedef struct {
    GuiRowPred pred;
} FiltroCtx;

/* ------------------------------------------------------------------ */
/* CSS                                                                 */
/* ------------------------------------------------------------------ */
static const char *CSS =
    "* { font-family: 'Inter', 'Noto Sans', 'Cantarell', 'DejaVu Sans', sans-serif; }\n"
    "window.aurora, .aurora-bg { background-color: #202020; color: #e8e8e8; }\n"
    ".sidebar { background-color: #1a1a1a; border-right: 1px solid #2c2c2c; }\n"
    ".instancia { background-color: #222222; border: 1px solid #2f2f2f; border-radius: 12px; padding: 12px; }\n"
    ".secao { color: #8b8b8b; font-size: 10px; font-weight: 700; letter-spacing: 1px; padding: 14px 12px 4px 12px; }\n"
    "button.nav-btn { background-image: none; background-color: transparent; border: none; box-shadow: none;\n"
    "  border-radius: 8px; padding: 8px 12px; color: #c7c7c7; font-size: 13px; }\n"
    "button.nav-btn:hover { background-color: #2a2a2a; color: #ffffff; }\n"
    "button.nav-btn.nav-active { background-color: #3b82f6; color: #ffffff; font-weight: 600; }\n"
    ".topbar { background-color: #1f1f1f; border-bottom: 1px solid #2c2c2c; padding: 8px 14px; }\n"
    ".logo { background-color: #3b82f6; color: #ffffff; border-radius: 10px; font-weight: 800; font-size: 16px; padding: 6px 10px; }\n"
    ".app-title { font-weight: 700; font-size: 15px; color: #ffffff; }\n"
    ".badge { background-color: rgba(59,130,246,0.16); color: #7db1ff; border: 1px solid rgba(59,130,246,0.45);\n"
    "  border-radius: 6px; padding: 1px 8px; font-size: 11px; font-family: monospace; font-weight: 700; }\n"
    ".badge-ok { background-color: rgba(34,197,94,0.14); color: #5ee08a; border-color: rgba(34,197,94,0.45); }\n"
    "entry, spinbutton { background-image: none; background-color: #2a2a2a; color: #eeeeee; border: 1px solid #3a3a3a;\n"
    "  border-radius: 8px; padding: 7px 10px; box-shadow: none; min-height: 18px; }\n"
    "entry:focus { border-color: #3b82f6; }\n"
    "combobox button.combo, combobox box button { background-image: none; background-color: #2a2a2a; color: #eeeeee;\n"
    "  border: 1px solid #3a3a3a; border-radius: 8px; box-shadow: none; padding: 5px 10px; }\n"
    "button { background-image: none; background-color: #2d2d2d; color: #e8e8e8; border: 1px solid #3a3a3a;\n"
    "  border-radius: 8px; padding: 7px 14px; box-shadow: none; text-shadow: none; }\n"
    "button:hover { background-color: #383838; }\n"
    "button:disabled { color: #6b6b6b; background-color: #262626; }\n"
    "button.primario { background-color: #3b82f6; border-color: #3b82f6; color: #ffffff; font-weight: 600; }\n"
    "button.primario:hover { background-color: #5392f8; }\n"
    "button.perigo { background-color: rgba(239,68,68,0.15); border-color: rgba(239,68,68,0.5); color: #ff8383; }\n"
    "button.perigo:hover { background-color: rgba(239,68,68,0.28); }\n"
    "button.topo { padding: 6px 10px; }\n"
    ".card { background-color: #262626; border: 1px solid #333333; border-radius: 14px; padding: 16px; }\n"
    ".kpi-titulo { color: #9a9a9a; font-size: 11px; font-weight: 700; letter-spacing: 1px; }\n"
    ".kpi-valor { color: #ffffff; font-size: 24px; font-weight: 800; }\n"
    ".kpi-sub { color: #8f8f8f; font-size: 11px; }\n"
    ".pg-titulo { color: #ffffff; font-size: 22px; font-weight: 800; }\n"
    ".pg-sub { color: #8f8f8f; font-size: 12px; }\n"
    ".card-titulo { color: #ffffff; font-size: 14px; font-weight: 700; }\n"
    ".mudo { color: #8f8f8f; font-size: 12px; }\n"
    ".erro { color: #ff7b7b; font-size: 12px; }\n"
    ".ok-texto { color: #5ee08a; }\n"
    "treeview { background-color: #242424; color: #e6e6e6; }\n"
    "treeview:selected { background-color: #2f5fb3; color: #ffffff; }\n"
    "treeview header button { background-image: none; background-color: #1f1f1f; color: #9a9a9a; border: none;\n"
    "  border-bottom: 1px solid #333333; border-radius: 0; font-weight: 700; font-size: 11px; padding: 8px 10px; }\n"
    "scrolledwindow.tabela { border: 1px solid #333333; border-radius: 12px; }\n"
    "list, list row { background-color: transparent; color: #e0e0e0; }\n"
    "list row { padding: 6px 4px; border-bottom: 1px solid #2f2f2f; }\n"
    "notebook > header { background-color: #202020; }\n"
    "notebook > header > tabs > tab { background-color: transparent; color: #a0a0a0; padding: 8px 16px; border: none; }\n"
    "notebook > header > tabs > tab:checked { color: #ffffff; border-bottom: 2px solid #3b82f6; }\n"
    "statusbar-box, .statusbar { background-color: #1a1a1a; border-top: 1px solid #2c2c2c; padding: 4px 14px; color: #9a9a9a; font-size: 11px; }\n"
    ".login-fundo { background-image: linear-gradient(135deg, #1b2335 0%, #202020 55%, #1a1a1a 100%); }\n"
    ".login-card { background-color: #262626; border: 1px solid #343434; border-radius: 18px; padding: 34px; }\n"
    ".login-logo { background-color: #3b82f6; color: #ffffff; border-radius: 16px; font-weight: 800; font-size: 28px; padding: 10px 22px; }\n"
    "dialog, messagedialog { background-color: #232323; color: #e8e8e8; }\n"
    ".dialog-action-area { padding: 6px 14px 14px 14px; }\n"
    "popover, popover.background { background-color: #262626; color: #e8e8e8; border: 1px solid #3a3a3a; border-radius: 10px; }\n"
    "textview { font-family: 'DejaVu Sans Mono', 'Liberation Mono', monospace; font-size: 12px; }\n"
    "textview text { background-color: #1b1b1b; color: #e6e6e6; }\n"
    "tooltip { background-color: #333333; color: #ffffff; border-radius: 6px; }\n";

void gui_aplicar_css(void)
{
    GtkCssProvider *prov = gtk_css_provider_new();
    GError *err = NULL;
    GtkSettings *st = gtk_settings_get_default();

    if (st != NULL) {
        g_object_set(st, "gtk-application-prefer-dark-theme", TRUE, NULL);
    }
    if (!gtk_css_provider_load_from_data(prov, CSS, -1, &err)) {
        g_warning("CSS: %s", err != NULL ? err->message : "erro");
        g_clear_error(&err);
    }
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(prov),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(prov);
}

/* ------------------------------------------------------------------ */
/* Widgets simples                                                     */
/* ------------------------------------------------------------------ */
GtkWidget *gui_label(const char *txt, const char *classe)
{
    GtkWidget *l = gtk_label_new(txt);

    gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
    if (classe != NULL) {
        gtk_style_context_add_class(gtk_widget_get_style_context(l), classe);
    }
    return l;
}

GtkWidget *gui_botao(const char *texto, const char *icone, const char *classe)
{
    GtkWidget *b = gtk_button_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    if (icone != NULL) {
        gtk_box_pack_start(GTK_BOX(box), gtk_image_new_from_icon_name(icone, GTK_ICON_SIZE_BUTTON),
                           FALSE, FALSE, 0);
    }
    if (texto != NULL && texto[0] != '\0') {
        gtk_box_pack_start(GTK_BOX(box), gtk_label_new(texto), FALSE, FALSE, 0);
    }
    gtk_container_add(GTK_CONTAINER(b), box);
    if (classe != NULL) {
        gtk_style_context_add_class(gtk_widget_get_style_context(b), classe);
    }
    return b;
}

void gui_msg(GtkWindow *pai, GtkMessageType tipo, const char *titulo, const char *txt)
{
    GtkWidget *d = gtk_message_dialog_new(pai, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                          tipo, GTK_BUTTONS_OK, "%s", titulo);

    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", txt);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

void gui_erro_rc(int rc)
{
    gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Não foi possível concluir",
            erp_mensagem_erro(rc));
}

gboolean gui_confirmar(const char *txt)
{
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(G.janela),
                                          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", txt);
    gint r;

    gtk_dialog_add_button(GTK_DIALOG(d), "Cancelar", GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(d), "Confirmar", GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
    r = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    return r == GTK_RESPONSE_OK;
}

gboolean gui_pode(Modulo m, AcaoAcesso a, gboolean avisar)
{
    gboolean ok = auth_pode(G.perfil, m, a);

    if (!ok && avisar) {
        char buf[200];

        snprintf(buf, sizeof buf, "NEGADO: %s em '%s'",
                 a == ACAO_EDITAR ? "alteracao" : "acesso", auth_nome_modulo(m));
        auth_log(G.usuario != NULL ? G.usuario->login : "-", buf);
        snprintf(buf, sizeof buf, "O perfil %s não tem permissão para esta ação (%s).",
                 auth_nome_perfil(G.perfil), auth_nome_modulo(m));
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_WARNING, "Acesso negado", buf);
    }
    return ok;
}

void gui_status(const char *txt)
{
    if (G.status_msg != NULL) {
        gtk_label_set_text(GTK_LABEL(G.status_msg), txt);
    }
}

void gui_copiar(const char *texto)
{
    GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    gtk_clipboard_set_text(cb, texto, -1);
}

/* ------------------------------------------------------------------ */
/* Formularios                                                         */
/* ------------------------------------------------------------------ */
gboolean gui_form(const char *titulo, const char *ok, const Campo *campos, int n,
                  char valores[][160])
{
    GtkWidget *d = gtk_dialog_new_with_buttons(titulo, GTK_WINDOW(G.janela),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               "Cancelar", GTK_RESPONSE_CANCEL, ok,
                                               GTK_RESPONSE_OK, NULL);
    GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(d));
    GtkWidget *grid = gtk_grid_new();
    GtkWidget *w[32];
    gboolean resultado;
    int i;

    if (n > 32) {
        n = 32;
    }
    gtk_window_set_position(GTK_WINDOW(d), G.janela != NULL ? GTK_WIN_POS_CENTER_ON_PARENT
                                                            : GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(area), 18);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 14);
    gtk_widget_set_size_request(grid, 420, -1);
    for (i = 0; i < n; i++) {
        gtk_grid_attach(GTK_GRID(grid), gui_label(campos[i].rotulo, "mudo"), 0, i, 1, 1);
        if (campos[i].tipo == CAMPO_COMBO) {
            gchar **op = g_strsplit(campos[i].opcoes != NULL ? campos[i].opcoes : "", "|", -1);
            int k;
            int sel = 0;

            w[i] = gtk_combo_box_text_new();
            for (k = 0; op[k] != NULL; k++) {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w[i]), op[k]);
                if (campos[i].inicial != NULL && strcmp(op[k], campos[i].inicial) == 0) {
                    sel = k;
                }
            }
            gtk_combo_box_set_active(GTK_COMBO_BOX(w[i]), sel);
            g_strfreev(op);
        } else {
            w[i] = gtk_entry_new();
            gtk_entry_set_activates_default(GTK_ENTRY(w[i]), TRUE);
            gtk_entry_set_max_length(GTK_ENTRY(w[i]), 150);
            if (campos[i].tipo == CAMPO_SENHA) {
                gtk_entry_set_visibility(GTK_ENTRY(w[i]), FALSE);
            }
            if (campos[i].inicial != NULL) {
                gtk_entry_set_text(GTK_ENTRY(w[i]), campos[i].inicial);
            }
        }
        gtk_widget_set_hexpand(w[i], TRUE);
        gtk_grid_attach(GTK_GRID(grid), w[i], 1, i, 1, 1);
    }
    gtk_widget_set_margin_bottom(grid, 14);
    gtk_box_pack_start(GTK_BOX(area), grid, TRUE, TRUE, 0);
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    {
        GtkWidget *bok = gtk_dialog_get_widget_for_response(GTK_DIALOG(d), GTK_RESPONSE_OK);

        gtk_style_context_add_class(gtk_widget_get_style_context(bok), "primario");
    }
    gtk_widget_show_all(d);
    if (n > 0) {
        gtk_widget_grab_focus(w[0]);
    }
    resultado = gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK;
    if (resultado) {
        for (i = 0; i < n; i++) {
            if (campos[i].tipo == CAMPO_COMBO) {
                gchar *t = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(w[i]));

                util_copiar_string(valores[i], 160, t != NULL ? t : "");
                g_free(t);
            } else {
                util_copiar_string(valores[i], 160, gtk_entry_get_text(GTK_ENTRY(w[i])));
            }
        }
    }
    gtk_widget_destroy(d);
    return resultado;
}

void gui_mostrar_texto(const char *titulo, const char *texto)
{
    GtkWidget *d = gtk_dialog_new_with_buttons(titulo, GTK_WINDOW(G.janela),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               "Copiar", GTK_RESPONSE_APPLY, "Fechar",
                                               GTK_RESPONSE_CLOSE, NULL);
    GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(d));
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *tv = gtk_text_view_new();

    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv), TRUE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv), 12);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv), 10);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv)), texto, -1);
    gtk_container_add(GTK_CONTAINER(sw), tv);
    gtk_widget_set_size_request(sw, 720, 380);
    gtk_container_set_border_width(GTK_CONTAINER(area), 14);
    gtk_box_pack_start(GTK_BOX(area), sw, TRUE, TRUE, 0);
    gtk_widget_show_all(d);
    for (;;) {
        gint r = gtk_dialog_run(GTK_DIALOG(d));

        if (r == GTK_RESPONSE_APPLY) {
            gui_copiar(texto);
            continue;
        }
        break;
    }
    gtk_widget_destroy(d);
}

void gui_resultado(const char *titulo, const char *texto)
{
    gui_mostrar_texto(titulo, texto);
}

/* ------------------------------------------------------------------ */
/* Estrutura de pagina e tabelas                                       */
/* ------------------------------------------------------------------ */
GtkWidget *gui_pagina(const char *titulo, const char *sub, GtkWidget **acoes)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    GtkWidget *topo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *tit = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *ac = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    gtk_container_set_border_width(GTK_CONTAINER(vbox), 22);
    gtk_box_pack_start(GTK_BOX(tit), gui_label(titulo, "pg-titulo"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tit), gui_label(sub, "pg-sub"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topo), tit, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(topo), ac, FALSE, FALSE, 0);
    gtk_widget_set_valign(ac, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), topo, FALSE, FALSE, 0);
    if (acoes != NULL) {
        *acoes = ac;
    }
    return vbox;
}

static gboolean filtro_visivel(GtkTreeModel *m, GtkTreeIter *it, gpointer data)
{
    FiltroCtx *ctx = data;
    int n;
    int i;

    if (ctx != NULL && ctx->pred != NULL && !ctx->pred(m, it)) {
        return FALSE;
    }
    if (G.busca[0] == '\0') {
        return TRUE;
    }
    n = gtk_tree_model_get_n_columns(m);
    for (i = 0; i < n; i++) {
        gchar *s = NULL;

        gtk_tree_model_get(m, it, i, &s, -1);
        if (s != NULL) {
            gchar *f = g_utf8_casefold(s, -1);
            gboolean achou = strstr(f, G.busca) != NULL;

            g_free(f);
            g_free(s);
            if (achou) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static void badge_cor(GtkTreeViewColumn *col, GtkCellRenderer *r, GtkTreeModel *m,
                      GtkTreeIter *it, gpointer data)
{
    int c = GPOINTER_TO_INT(data);
    gchar *s = NULL;
    const char *cor = "#c9c9c9";

    (void)col;
    gtk_tree_model_get(m, it, c, &s, -1);
    if (s != NULL) {
        if (!strcmp(s, "FATURADA") || !strcmp(s, "LIQUIDADO") || !strcmp(s, "OK") ||
            !strcmp(s, "Ativo")) {
            cor = "#5ee08a";
        } else if (!strcmp(s, "ABERTO") || !strcmp(s, "Baixo") || !strcmp(s, "PARCIAL")) {
            cor = "#f5b84a";
        } else if (!strcmp(s, "CANCELADA") || !strcmp(s, "CANCELADO") ||
                   !strcmp(s, "VENCIDO") || !strcmp(s, "Inativo") || !strcmp(s, "ZERADO")) {
            cor = "#ff7b7b";
        } else if (!strcmp(s, "ORCAMENTO")) {
            cor = "#7db1ff";
        }
        g_free(s);
    }
    g_object_set(r, "foreground", cor, "weight", 700, NULL);
}

GtkWidget *gui_tabela(int n, const char *const *titulos, const char *alinhas,
                      int col_badge, GuiRowPred pred, GtkListStore **store,
                      GtkWidget **tv)
{
    GType tipos[32];
    GtkListStore *st;
    GtkTreeModel *filtro;
    GtkWidget *view;
    GtkWidget *sw;
    FiltroCtx *ctx;
    int i;

    if (n > 32) {
        n = 32;
    }
    for (i = 0; i < n; i++) {
        tipos[i] = G_TYPE_STRING;
    }
    st = gtk_list_store_newv(n, tipos);
    filtro = gtk_tree_model_filter_new(GTK_TREE_MODEL(st), NULL);
    ctx = g_new0(FiltroCtx, 1);
    ctx->pred = pred;
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filtro), filtro_visivel, ctx, g_free);
    if (g_filtros == NULL) {
        g_filtros = g_ptr_array_new();
    }
    g_ptr_array_add(g_filtros, filtro);
    if (g_tvs == NULL) {
        g_tvs = g_ptr_array_new();
    }

    view = gtk_tree_view_new_with_model(filtro);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), FALSE);
    for (i = 0; i < n; i++) {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(titulos[i], r, "text", i, NULL);
        gboolean dir = alinhas != NULL && alinhas[i] == 'r';

        if (dir) {
            g_object_set(r, "xalign", 1.0, NULL);
            gtk_tree_view_column_set_alignment(c, 1.0f);
        }
        g_object_set(r, "ypad", 8, "xpad", 8, NULL);
        gtk_tree_view_column_set_resizable(c, TRUE);
        if (i == col_badge) {
            gtk_tree_view_column_set_cell_data_func(c, r, badge_cor, GINT_TO_POINTER(i), NULL);
        }
        gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    }
    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_style_context_add_class(gtk_widget_get_style_context(sw), "tabela");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sw), view);
    gtk_widget_set_vexpand(sw, TRUE);
    g_ptr_array_add(g_tvs, view);
    if (store != NULL) {
        *store = st;
    }
    if (tv != NULL) {
        *tv = view;
    }
    g_object_unref(filtro);
    g_object_unref(st);
    return sw;
}

void gui_store_add(GtkListStore *s, const char *const *cols, int n)
{
    GtkTreeIter it;
    int i;

    gtk_list_store_append(s, &it);
    for (i = 0; i < n; i++) {
        gtk_list_store_set(s, &it, i, cols[i], -1);
    }
}

gboolean gui_selecionado(GtkWidget *tv, int *id)
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
    GtkTreeModel *m;
    GtkTreeIter it;
    gchar *s = NULL;

    if (!gtk_tree_selection_get_selected(sel, &m, &it)) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_INFO, "Nenhum item selecionado",
                "Selecione uma linha da tabela primeiro.");
        return FALSE;
    }
    gtk_tree_model_get(m, &it, 0, &s, -1);
    *id = s != NULL ? atoi(s) : 0;
    g_free(s);
    return TRUE;
}

void gui_refiltrar(void)
{
    guint i;

    if (g_filtros == NULL) {
        return;
    }
    for (i = 0; i < g_filtros->len; i++) {
        gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(g_ptr_array_index(g_filtros, i)));
    }
}

void gui_limpar_filtros(void)
{
    if (g_filtros != NULL) {
        g_ptr_array_set_size(g_filtros, 0);
    }
    if (g_tvs != NULL) {
        g_ptr_array_set_size(g_tvs, 0);
    }
}

/* Guarda o ID selecionado de cada tabela, atualiza e reaplica a selecao. */
static void atualizar_preservando(int so_modulo)
{
    guint n = g_tvs != NULL ? g_tvs->len : 0;
    gchar **ids = g_new0(gchar *, n + 1);
    guint i;
    int m;

    for (i = 0; i < n; i++) {
        GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(g_ptr_array_index(g_tvs, i)));
        GtkTreeModel *mod;
        GtkTreeIter it;

        if (gtk_tree_selection_get_selected(sel, &mod, &it)) {
            gtk_tree_model_get(mod, &it, 0, &ids[i], -1);
        }
    }
    for (m = 0; m < MOD_TOTAL; m++) {
        if ((so_modulo < 0 || so_modulo == m) && G.atualizar[m] != NULL) {
            G.atualizar[m]();
        }
    }
    for (i = 0; i < n; i++) {
        GtkWidget *tv = g_ptr_array_index(g_tvs, i);
        GtkTreeModel *mod = gtk_tree_view_get_model(GTK_TREE_VIEW(tv));
        GtkTreeIter it;
        gboolean ok;

        if (ids[i] != NULL) {
            for (ok = gtk_tree_model_get_iter_first(mod, &it); ok; ok = gtk_tree_model_iter_next(mod, &it)) {
                gchar *id = NULL;

                gtk_tree_model_get(mod, &it, 0, &id, -1);
                if (id != NULL && strcmp(id, ids[i]) == 0) {
                    gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(tv)), &it);
                    g_free(id);
                    break;
                }
                g_free(id);
            }
        }
        g_free(ids[i]);
    }
    g_free(ids);
}

void gui_atualizar_tudo(void)
{
    atualizar_preservando(-1);
}

void gui_atualizar_modulo(Modulo m)
{
    atualizar_preservando((int)m);
}

void gui_registrar(const char *evento)
{
    auth_log(G.usuario != NULL ? G.usuario->login : "-", evento);
}

/* Auditoria + gravacao automatica dos .dat + refresh das telas */
void gui_gravar(const char *evento)
{
    int rc;
    char buf[256];

    gui_registrar(evento);
    rc = persistencia_salvar_dados();
    gui_atualizar_tudo();
    if (rc != ERP_OK) {
        snprintf(buf, sizeof buf, "Atenção: falha ao gravar os arquivos .dat (%s)", erp_mensagem_erro(rc));
    } else {
        snprintf(buf, sizeof buf, "%s", evento);
    }
    gui_status(buf);
}

/* ------------------------------------------------------------------ */
/* Auxiliares de dados                                                 */
/* ------------------------------------------------------------------ */
void gui_formatar_doc(const char *d, char *out, size_t tam)
{
    size_t n = strlen(d);

    if (n == 11) {
        snprintf(out, tam, "%.3s.%.3s.%.3s-%.2s", d, d + 3, d + 6, d + 9);
    } else if (n == 14) {
        snprintf(out, tam, "%.2s.%.3s.%.3s/%.4s-%.2s", d, d + 2, d + 5, d + 8, d + 12);
    } else {
        snprintf(out, tam, "%s", d);
    }
}

void gui_opcoes_clientes(GString *s, gboolean so_ativos, gboolean com_zero)
{
    int32_t i;

    g_string_truncate(s, 0);
    if (com_zero) {
        g_string_append(s, "0 - (Fornecedor / outro)");
    }
    for (i = 0; i < g_num_clientes; i++) {
        if (so_ativos && !g_clientes[i].ativo) {
            continue;
        }
        g_string_append_printf(s, "%s%d - %s", s->len > 0 ? "|" : "", (int)g_clientes[i].id,
                               g_clientes[i].nome);
    }
}

void gui_opcoes_produtos(GString *s)
{
    int32_t i;

    g_string_truncate(s, 0);
    for (i = 0; i < g_num_produtos; i++) {
        g_string_append_printf(s, "%s%d - %s - %s", s->len > 0 ? "|" : "", (int)g_produtos[i].id,
                               g_produtos[i].codigo, g_produtos[i].descricao);
    }
}

int gui_id_da_opcao(const char *op)
{
    return op != NULL ? atoi(op) : 0;
}
