/*
 * Aurora ERP - gui_main.c
 * Janela de login (usuario + senha), janela principal com barra lateral,
 * troca de senha, bloqueio por inatividade e laco principal.
 */
#include "gui.h"

#define IDLE_SEGUNDOS (15 * 60)

/* ================================================================== */
/* Login                                                               */
/* ================================================================== */
static GtkWidget *w_login;
static GtkWidget *e_user;
static GtkWidget *e_pass;
static GtkWidget *l_erro;
static Usuario   *login_usuario;
static gboolean   login_ok;

static void login_destruida(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    if (!login_ok) {
        gtk_main_quit();
    }
}

static void login_tentar(GtkWidget *w, gpointer d)
{
    Usuario *u = NULL;
    int rc;
    char *login = g_strdup(gtk_entry_get_text(GTK_ENTRY(e_user)));
    char *senha = g_strdup(gtk_entry_get_text(GTK_ENTRY(e_pass)));

    (void)w; (void)d;
    g_strstrip(login);
    if (login[0] == '\0' || senha[0] == '\0') {
        gtk_label_set_text(GTK_LABEL(l_erro), "Informe usuário e senha.");
        goto fim;
    }
    rc = auth_login(login, senha, &u);
    if (rc == ERP_OK) {
        login_usuario = u;
        login_ok = TRUE;
        gtk_main_quit();
    } else {
        gtk_label_set_text(GTK_LABEL(l_erro),
                           rc == ERP_ERR_BLOQUEADO
                               ? "Muitas tentativas. Aguarde 5 minutos e tente novamente."
                               : "Usuário ou senha inválidos.");
        gtk_entry_set_text(GTK_ENTRY(e_pass), "");
        gtk_widget_grab_focus(e_pass);
    }
fim:
    memset(senha, 0, strlen(senha));
    g_free(senha);
    g_free(login);
}

static void login_olho(GtkEntry *e, GtkEntryIconPosition pos, GdkEvent *ev, gpointer d)
{
    gboolean vis = gtk_entry_get_visibility(e);

    (void)pos; (void)ev; (void)d;
    gtk_entry_set_visibility(e, !vis);
    gtk_entry_set_icon_from_icon_name(e, GTK_ENTRY_ICON_SECONDARY,
                                      vis ? "view-reveal-symbolic" : "view-conceal-symbolic");
}

static Usuario *tela_login(void)
{
    GtkWidget *centro = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *logo = gui_label("A", "login-logo");
    GtkWidget *btn;
    GtkWidget *rodape;
    GtkWidget *linha_logo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    login_usuario = NULL;
    login_ok = FALSE;
    w_login = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w_login), "ERP Aurora — Acesso");
    gtk_window_set_default_size(GTK_WINDOW(w_login), 980, 640);
    gtk_window_set_position(GTK_WINDOW(w_login), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(w_login), "login-fundo");
    g_signal_connect(w_login, "destroy", G_CALLBACK(login_destruida), NULL);

    gtk_style_context_add_class(gtk_widget_get_style_context(card), "login-card");
    gtk_widget_set_size_request(card, 400, -1);
    gtk_widget_set_halign(logo, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(linha_logo), logo, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), linha_logo, FALSE, FALSE, 4);
    {
        GtkWidget *t = gui_label("ERP Aurora", "pg-titulo");
        GtkWidget *s = gui_label("Entre com seu usuário e senha", "pg-sub");

        gtk_label_set_xalign(GTK_LABEL(t), 0.5f);
        gtk_label_set_xalign(GTK_LABEL(s), 0.5f);
        gtk_box_pack_start(GTK_BOX(card), t, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(card), s, FALSE, FALSE, 8);
    }
    e_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_user), "Usuário");
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(e_user), GTK_ENTRY_ICON_PRIMARY, "avatar-default-symbolic");
    gtk_entry_set_max_length(GTK_ENTRY(e_user), 40);
    e_pass = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_pass), "Senha");
    gtk_entry_set_visibility(GTK_ENTRY(e_pass), FALSE);
    gtk_entry_set_max_length(GTK_ENTRY(e_pass), 128);
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(e_pass), GTK_ENTRY_ICON_PRIMARY, "dialog-password-symbolic");
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(e_pass), GTK_ENTRY_ICON_SECONDARY, "view-reveal-symbolic");
    gtk_entry_set_icon_activatable(GTK_ENTRY(e_pass), GTK_ENTRY_ICON_SECONDARY, TRUE);
    g_signal_connect(e_pass, "icon-press", G_CALLBACK(login_olho), NULL);
    g_signal_connect(e_user, "activate", G_CALLBACK(login_tentar), NULL);
    g_signal_connect(e_pass, "activate", G_CALLBACK(login_tentar), NULL);
    gtk_box_pack_start(GTK_BOX(card), e_user, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), e_pass, FALSE, FALSE, 0);

    l_erro = gui_label("", "erro");
    gtk_label_set_xalign(GTK_LABEL(l_erro), 0.5f);
    gtk_box_pack_start(GTK_BOX(card), l_erro, FALSE, FALSE, 0);
    btn = gui_botao("Entrar", NULL, "primario");
    g_signal_connect(btn, "clicked", G_CALLBACK(login_tentar), NULL);
    gtk_box_pack_start(GTK_BOX(card), btn, FALSE, FALSE, 4);

    rodape = gui_label("Núcleo em C11 • PIM II UNIP", "mudo");
    gtk_label_set_xalign(GTK_LABEL(rodape), 0.5f);
    gtk_box_pack_start(GTK_BOX(card), rodape, FALSE, FALSE, 6);

    gtk_widget_set_halign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(card, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(centro), card, TRUE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(w_login), centro);
    gtk_widget_show_all(w_login);
    gtk_widget_grab_focus(e_user);
    gtk_main();
    if (login_ok) {
        gtk_widget_destroy(w_login);
    }
    w_login = NULL;
    return login_usuario;
}

/* ================================================================== */
/* Troca de senha                                                      */
/* ================================================================== */
void gui_alterar_minha_senha(gboolean obrigatoria, gboolean *ok)
{
    char v[3][160];
    Campo c3[] = {
        {"Senha atual", CAMPO_SENHA, "", NULL},
        {"Nova senha (mín. 6 caracteres)", CAMPO_SENHA, "", NULL},
        {"Confirmar nova senha", CAMPO_SENHA, "", NULL}
    };
    Campo c2[] = {
        {"Nova senha (mín. 6 caracteres)", CAMPO_SENHA, "", NULL},
        {"Confirmar nova senha", CAMPO_SENHA, "", NULL}
    };
    const char *nova;
    const char *conf;

    *ok = FALSE;
    for (;;) {
        gboolean enviado;
        int rc;

        if (obrigatoria) {
            enviado = gui_form("Defina uma nova senha para continuar", "Salvar senha", c2, 2, v);
            nova = v[0];
            conf = v[1];
        } else {
            enviado = gui_form("Alterar minha senha", "Salvar senha", c3, 3, v);
            nova = v[1];
            conf = v[2];
            if (enviado && !auth_verificar_senha(G.usuario, v[0])) {
                gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Senha atual incorreta", "Tente novamente.");
                continue;
            }
        }
        if (!enviado) {
            return;
        }
        if (strcmp(nova, conf) != 0) {
            gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Senhas diferentes", "A confirmação não confere com a nova senha.");
            continue;
        }
        if (auth_verificar_senha(G.usuario, nova)) {
            gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_ERROR, "Senha repetida", "A nova senha deve ser diferente da atual.");
            continue;
        }
        rc = auth_trocar_senha(G.usuario->id, nova, FALSE);
        if (rc != ERP_OK) {
            gui_erro_rc(rc);
            continue;
        }
        memset(v, 0, sizeof v);
        auth_salvar();
        gui_registrar("Senha alterada pelo proprio usuario");
        *ok = TRUE;
        return;
    }
}

/* ================================================================== */
/* Janela principal                                                    */
/* ================================================================== */
typedef struct {
    Modulo      mod;
    const char *rotulo;
    const char *icone;
    const char *secao;
    GtkWidget *(*criar)(void);
    const char *nome;
} NavItem;

static const NavItem NAV[] = {
    {MOD_DASHBOARD, "Visão Geral", "view-grid-symbolic", "OPERAÇÕES PRINCIPAIS", pg_dashboard_criar, "dash"},
    {MOD_CLIENTES, "Clientes", "system-users-symbolic", NULL, pg_clientes_criar, "cli"},
    {MOD_ESTOQUE, "Estoque", "package-x-generic-symbolic", NULL, pg_estoque_criar, "est"},
    {MOD_VENDAS, "Vendas & Pedidos", "document-send-symbolic", NULL, pg_vendas_criar, "ven"},
    {MOD_FINANCEIRO, "Financeiro & Caixa", "x-office-spreadsheet-symbolic", NULL, pg_financeiro_criar, "fin"},
    {MOD_FISCAL, "NF-e & Boletos", "x-office-document-symbolic", NULL, pg_fiscal_criar, "fis"},
    {MOD_USUARIOS, "Usuários & Acesso", "avatar-default-symbolic", "ADMINISTRAÇÃO", pg_usuarios_criar, "usr"},
    {MOD_CONFIG, "Configurações & Backup", "preferences-system-symbolic", NULL, pg_config_criar, "cfg"}
};
#define N_NAV ((int)(sizeof NAV / sizeof NAV[0]))

static gint64 g_ultima_atividade;
static guint  g_idle_timer;

static void ir_para(Modulo m)
{
    int i;

    for (i = 0; i < N_NAV; i++) {
        if (NAV[i].mod == m && G.nav[m] != NULL) {
            gtk_stack_set_visible_child_name(GTK_STACK(G.stack), NAV[i].nome);
            break;
        }
    }
    for (i = 0; i < MOD_TOTAL; i++) {
        if (G.nav[i] != NULL) {
            GtkStyleContext *sc = gtk_widget_get_style_context(G.nav[i]);

            if ((Modulo)i == m) {
                gtk_style_context_add_class(sc, "nav-active");
            } else {
                gtk_style_context_remove_class(sc, "nav-active");
            }
        }
    }
    G.atual = m;
    gui_atualizar_modulo(m);
    {
        char t[200];

        snprintf(t, sizeof t, "ERP Aurora — %s — %s", g_empresa.nome_fantasia, auth_nome_modulo(m));
        gtk_window_set_title(GTK_WINDOW(G.janela), t);
    }
}

static void nav_clicado(GtkButton *b, gpointer d)
{
    (void)b;
    ir_para((Modulo)GPOINTER_TO_INT(d));
}

static void busca_mudou(GtkSearchEntry *e, gpointer d)
{
    gchar *f = g_utf8_casefold(gtk_entry_get_text(GTK_ENTRY(e)), -1);

    (void)d;
    util_copiar_string(G.busca, sizeof G.busca, f);
    g_free(f);
    gui_refiltrar();
}

static void sair_clicado(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    gui_registrar("LOGOUT");
    G.relogin = TRUE;
    gtk_widget_destroy(G.janela);
}

static void senha_clicado(GtkButton *b, gpointer d)
{
    gboolean ok = FALSE;

    (void)b; (void)d;
    gui_alterar_minha_senha(FALSE, &ok);
    if (ok) {
        gui_msg(GTK_WINDOW(G.janela), GTK_MESSAGE_INFO, "Senha alterada", "Sua senha foi atualizada com sucesso.");
    }
}

static void atualizar_clicado(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    gui_atualizar_tudo();
    gui_status("Telas atualizadas.");
}

static gboolean tecla(GtkWidget *w, GdkEventKey *ev, gpointer d)
{
    (void)w; (void)d;
    g_ultima_atividade = g_get_monotonic_time();
    if ((ev->state & GDK_CONTROL_MASK) && (ev->keyval == GDK_KEY_k || ev->keyval == GDK_KEY_K)) {
        gtk_widget_grab_focus(G.search);
        return TRUE;
    }
    return FALSE;
}

static gboolean atividade(GtkWidget *w, GdkEvent *ev, gpointer d)
{
    (void)w; (void)ev; (void)d;
    g_ultima_atividade = g_get_monotonic_time();
    return FALSE;
}

static gboolean checar_idle(gpointer d)
{
    (void)d;
    if (G.janela != NULL &&
        (g_get_monotonic_time() - g_ultima_atividade) / 1000000 > IDLE_SEGUNDOS) {
        gui_registrar("LOGOUT automatico por inatividade");
        G.relogin = TRUE;
        gtk_widget_destroy(G.janela);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static void janela_destruida(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    if (g_idle_timer != 0) {
        g_source_remove(g_idle_timer);
        g_idle_timer = 0;
    }
    gui_limpar_filtros();
    G.janela = NULL;
    G.status_msg = NULL;
    G.stack = NULL;
    G.search = NULL;
    memset(G.nav, 0, sizeof G.nav);
    memset(G.atualizar, 0, sizeof G.atualizar);
    gtk_main_quit();
}

static GtkWidget *construir_sidebar(void)
{
    GtkWidget *side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *inst = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *l1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *rod = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *esp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    int i;

    gtk_style_context_add_class(gtk_widget_get_style_context(side), "sidebar");
    gtk_widget_set_size_request(side, 250, -1);
    gtk_container_set_border_width(GTK_CONTAINER(side), 12);

    gtk_style_context_add_class(gtk_widget_get_style_context(inst), "instancia");
    gtk_box_pack_start(GTK_BOX(l1), gui_label("INSTÂNCIA LOCAL", "kpi-titulo"), TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(l1), gui_label("ARQUIVOS .DAT", "badge"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inst), l1, FALSE, FALSE, 0);
    {
        GtkWidget *nm = gui_label(g_empresa.razao_social, "card-titulo");

        gtk_label_set_ellipsize(GTK_LABEL(nm), PANGO_ELLIPSIZE_END);
        gtk_box_pack_start(GTK_BOX(inst), nm, FALSE, FALSE, 2);
    }
    gtk_box_pack_start(GTK_BOX(inst), gui_label("● Motor C11 / ISO ativo", "ok-texto"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(side), inst, FALSE, FALSE, 4);

    for (i = 0; i < N_NAV; i++) {
        const NavItem *n = &NAV[i];
        GtkWidget *b;
        GtkWidget *box;

        if (!auth_pode(G.perfil, n->mod, ACAO_VER)) {
            continue;
        }
        if (n->secao != NULL) {
            gtk_box_pack_start(GTK_BOX(side), gui_label(n->secao, "secao"), FALSE, FALSE, 0);
        }
        b = gtk_button_new();
        box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_box_pack_start(GTK_BOX(box), gtk_image_new_from_icon_name(n->icone, GTK_ICON_SIZE_MENU), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), gtk_label_new(n->rotulo), FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(b), box);
        gtk_widget_set_halign(box, GTK_ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "nav-btn");
        g_signal_connect(b, "clicked", G_CALLBACK(nav_clicado), GINT_TO_POINTER((int)n->mod));
        gtk_box_pack_start(GTK_BOX(side), b, FALSE, FALSE, 0);
        G.nav[n->mod] = b;
    }
    gtk_box_pack_start(GTK_BOX(side), esp, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(rod), gui_label("● ERP Aurora", "card-titulo"), TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(rod), gui_label("v1.0 (C11)", "badge"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(side), rod, FALSE, FALSE, 6);
    return side;
}

static GtkWidget *construir_topbar(void)
{
    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *mb = gtk_menu_button_new();
    GtkWidget *pop = gtk_popover_new(mb);
    GtkWidget *pv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *b1 = gui_botao("Alterar minha senha", "dialog-password-symbolic", NULL);
    GtkWidget *b2 = gui_botao("Sair", "system-log-out-symbolic", "perigo");
    GtkWidget *brf = gui_botao(NULL, "view-refresh-symbolic", "topo");
    char nome[160];

    gtk_style_context_add_class(gtk_widget_get_style_context(top), "topbar");
    gtk_box_pack_start(GTK_BOX(top), gui_label("A", "logo"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), gui_label("ERP Aurora", "app-title"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), gui_label("C11", "badge"), FALSE, FALSE, 0);
#ifdef USE_SQLSERVER
    gtk_box_pack_start(GTK_BOX(top), gui_label("● .dat + SQL Server", "badge badge-ok"), FALSE, FALSE, 4);
#else
    {
        GtkWidget *p = gui_label("● Arquivos .dat", "badge");

        gtk_style_context_add_class(gtk_widget_get_style_context(p), "badge-ok");
        gtk_box_pack_start(GTK_BOX(top), p, FALSE, FALSE, 4);
    }
#endif
    G.search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(G.search), "Buscar clientes, produtos, NF-e, pedidos...  (Ctrl+K)");
    gtk_widget_set_hexpand(G.search, TRUE);
    gtk_widget_set_size_request(G.search, 320, -1);
    g_signal_connect(G.search, "search-changed", G_CALLBACK(busca_mudou), NULL);
    gtk_box_pack_start(GTK_BOX(top), G.search, TRUE, TRUE, 10);

    g_signal_connect(brf, "clicked", G_CALLBACK(atualizar_clicado), NULL);
    gtk_widget_set_tooltip_text(brf, "Atualizar telas");
    gtk_box_pack_start(GTK_BOX(top), brf, FALSE, FALSE, 0);

    snprintf(nome, sizeof nome, "%s • %s", G.usuario->nome, auth_nome_perfil(G.perfil));
    {
        GtkWidget *bx = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

        gtk_box_pack_start(GTK_BOX(bx), gtk_image_new_from_icon_name("avatar-default-symbolic", GTK_ICON_SIZE_BUTTON), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(bx), gtk_label_new(nome), FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(mb), bx);
    }
    gtk_box_pack_start(GTK_BOX(pv), b1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pv), b2, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(pv), 10);
    gtk_container_add(GTK_CONTAINER(pop), pv);
    gtk_widget_show_all(pv);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(mb), pop);
    g_signal_connect(b1, "clicked", G_CALLBACK(senha_clicado), NULL);
    g_signal_connect(b2, "clicked", G_CALLBACK(sair_clicado), NULL);
    gtk_box_pack_end(GTK_BOX(top), mb, FALSE, FALSE, 0);
    return top;
}

static void construir_janela(void)
{
    GtkWidget *raiz = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *dir = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *rodape = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    GtkWidget *info;
    char dt[TAM_DATA];
    char txt[200];
    int i;
    Modulo primeiro = MOD_DASHBOARD;
    gboolean achou = FALSE;

    memset(G.nav, 0, sizeof G.nav);
    memset(G.atualizar, 0, sizeof G.atualizar);
    G.busca[0] = '\0';
    G.janela = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(G.janela), 1366, 820);
    gtk_window_set_position(GTK_WINDOW(G.janela), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(G.janela), "aurora");
    g_signal_connect(G.janela, "destroy", G_CALLBACK(janela_destruida), NULL);
    g_signal_connect(G.janela, "key-press-event", G_CALLBACK(tecla), NULL);
    gtk_widget_add_events(G.janela, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
    g_signal_connect(G.janela, "motion-notify-event", G_CALLBACK(atividade), NULL);
    g_signal_connect(G.janela, "button-press-event", G_CALLBACK(atividade), NULL);

    gtk_box_pack_start(GTK_BOX(raiz), construir_sidebar(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dir), construir_topbar(), FALSE, FALSE, 0);

    G.stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(G.stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_vexpand(G.stack, TRUE);
    for (i = 0; i < N_NAV; i++) {
        GtkWidget *pg;

        if (!auth_pode(G.perfil, NAV[i].mod, ACAO_VER)) {
            continue;
        }
        pg = NAV[i].criar();
        if (NAV[i].mod == MOD_CONFIG || NAV[i].mod == MOD_FISCAL) {
            GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

            gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
            gtk_container_add(GTK_CONTAINER(sw), pg);
            pg = sw;
        }
        gtk_stack_add_named(GTK_STACK(G.stack), pg, NAV[i].nome);
        if (!achou) {
            primeiro = NAV[i].mod;
            achou = TRUE;
        }
    }
    gtk_box_pack_start(GTK_BOX(dir), G.stack, TRUE, TRUE, 0);

    gtk_style_context_add_class(gtk_widget_get_style_context(rodape), "statusbar");
    gtk_box_pack_start(GTK_BOX(rodape), gui_label("● Motor C11 ativo", "ok-texto"), FALSE, FALSE, 0);
    G.status_msg = gui_label("Pronto.", "mudo");
    gtk_box_pack_start(GTK_BOX(rodape), G.status_msg, TRUE, TRUE, 0);
    data_formatar(data_hoje(), dt, sizeof dt);
    snprintf(txt, sizeof txt, "%s • %s • %s", G.usuario->login, auth_nome_perfil(G.perfil), dt);
    info = gui_label(txt, "mudo");
    gtk_box_pack_end(GTK_BOX(rodape), info, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dir), rodape, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(raiz), dir, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(G.janela), raiz);
    gtk_widget_show_all(G.janela);
    ir_para(primeiro);

    g_ultima_atividade = g_get_monotonic_time();
    g_idle_timer = g_timeout_add_seconds(30, checar_idle, NULL);
}

/* ================================================================== */
/* main                                                                */
/* ================================================================== */
int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    cadastros_inicializar_empresa_padrao();
    if (persistencia_carregar_dados() != ERP_OK) {
        sistema_inicializar_dados_padrao();
        persistencia_salvar_dados();
    }
    if (auth_inicializar() != ERP_OK) {
        fprintf(stderr, "Aviso: nao foi possivel gravar usuarios.dat\n");
    }
    gui_aplicar_css();

    for (;;) {
        Usuario *u = tela_login();

        if (u == NULL) {
            break;
        }
        G.usuario = u;
        G.perfil = u->perfil;
        G.janela = NULL;
        if (u->trocar_senha) {
            gboolean ok = FALSE;

            gui_alterar_minha_senha(TRUE, &ok);
            if (!ok) {
                gui_registrar("Troca obrigatoria de senha cancelada");
                continue;
            }
        }
        auth_salvar();
        G.relogin = FALSE;
        construir_janela();
        gtk_main();
        if (!G.relogin) {
            break;
        }
    }
    return 0;
}
