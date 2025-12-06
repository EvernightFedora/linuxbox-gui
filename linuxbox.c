#include "linuxbox.h"
#include <gio/gio.h>

// --- 创建逻辑的回调 (复用异步逻辑) ---
static void on_create_output(GObject *s, GAsyncResult *r, gpointer d) {
    GDataInputStream *stream = G_DATA_INPUT_STREAM(s);
    AppContext *app = (AppContext *)d;
    char *line = g_data_input_stream_read_line_finish(stream, r, NULL, NULL);
    if (line) {
        append_log(app->textview, "%s", line);
        g_free(line);
        g_data_input_stream_read_line_async(stream, G_PRIORITY_DEFAULT, NULL, on_create_output, app);
    }
}

static void on_create_done(GObject *s, GAsyncResult *r, gpointer d) {
    GSubprocess *proc = G_SUBPROCESS(s);
    AppContext *app = (AppContext *)d;
    if (g_subprocess_wait_finish(proc, r, NULL) && g_subprocess_get_successful(proc)) {
        append_log(app->textview, "🎉 创建成功！");
        create_desktop_entry(app->pending_name, app->pending_distro, app->pending_icon, app);
        refresh_container_list(app); // 创建成功后自动刷新列表
    } else {
        append_log(app->textview, "❌ 创建失败。");
    }
    gtk_widget_set_sensitive(app->btn_create, TRUE);
    g_free(app->pending_name);
    g_free(app->pending_distro);
    g_free(app->pending_icon);
}

static void on_btn_create_clicked(GtkWidget *btn, gpointer user_data) {
    AppContext *app = (AppContext *)user_data;
    const char *name_in = gtk_editable_get_text(GTK_EDITABLE(app->entry_name));
    guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(app->dropdown));

    if (idx >= app->distro_count || strlen(name_in) == 0) {
        append_log(app->textview, "⚠️ 请填写名称并选择类型。");
        return;
    }

    if (container_exists(name_in)) {
        append_log(app->textview, "⚠️ 容器 %s 已存在。", name_in);
        return;
    }

    const char *distro_id = app->distro_ids[idx];
    char *image = g_key_file_get_string(app->key_file, distro_id, "image", NULL);
    char *icon = g_key_file_get_string(app->key_file, distro_id, "icon", NULL);
    
    // 锁定按钮
    gtk_widget_set_sensitive(app->btn_create, FALSE);
    app->pending_name = g_strdup(name_in);
    app->pending_distro = g_strdup(distro_id);
    app->pending_icon = icon;

    const char *home = g_get_home_dir();
    char *custom_home = g_strdup_printf("%s/.local/share/distrobox/%s", home, name_in);
    
    append_log(app->textview, "🚀 开始创建 %s...", name_in);
    
    GError *err = NULL;
    GSubprocessLauncher *l = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE);
    GSubprocess *p = g_subprocess_launcher_spawn(l, &err, "distrobox", "create", "--yes", "--name", name_in, "--image", image, "--home", custom_home, NULL);
    
    if (p) {
        GInputStream *pipe = g_subprocess_get_stdout_pipe(p);
        GDataInputStream *stream = g_data_input_stream_new(pipe);
        g_data_input_stream_read_line_async(stream, G_PRIORITY_DEFAULT, NULL, on_create_output, app);
        g_subprocess_wait_async(p, NULL, on_create_done, app);
    } else {
        append_log(app->textview, "Error: %s", err->message);
        gtk_widget_set_sensitive(app->btn_create, TRUE);
    }
    g_object_unref(l); g_free(image); g_free(custom_home);
}

static void activate(GtkApplication *gtk_app, gpointer user_data) {
    AppContext *app = g_new0(AppContext, 1);
    
    // 加载配置
    app->key_file = g_key_file_new();
    if (!g_key_file_load_from_file(app->key_file, resolve_config_file(), G_KEY_FILE_NONE, NULL)) {
        g_printerr("⚠️ 无法加载配置文件\n");
    }

    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "Linux Box Manager");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 700, 600);

    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(app->window), main_vbox);

    // ================= TOP BAR (Toolbar style) =================
    GtkWidget *top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(top_bar, 10);
    gtk_widget_set_margin_bottom(top_bar, 10);
    gtk_widget_set_margin_start(top_bar, 10);
    gtk_widget_set_margin_end(top_bar, 10);
    
    // 1. Label
    gtk_box_append(GTK_BOX(top_bar), gtk_label_new("新建容器:"));

    // 2. Dropdown (Type)
    app->distro_ids = g_key_file_get_groups(app->key_file, (gsize*)&app->distro_count);
    GtkStringList *sl = gtk_string_list_new(NULL);
    for (int i=0; i<app->distro_count; i++) {
        char *n = g_key_file_get_string(app->key_file, app->distro_ids[i], "name", NULL);
        gtk_string_list_append(sl, n ? n : app->distro_ids[i]);
        g_free(n);
    }
    app->dropdown = gtk_drop_down_new(G_LIST_MODEL(sl), NULL);
    gtk_widget_set_size_request(app->dropdown, 180, -1); // 稍微宽一点
    gtk_box_append(GTK_BOX(top_bar), app->dropdown);

    // 3. Entry (Name)
    app->entry_name = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_name), "容器名称");
    gtk_widget_set_hexpand(app->entry_name, TRUE); // 占据剩余空间
    gtk_box_append(GTK_BOX(top_bar), app->entry_name);

    // 4. Create Button
    app->btn_create = gtk_button_new_with_label("创建");
    gtk_widget_add_css_class(app->btn_create, "suggested-action"); // 蓝色高亮
    g_signal_connect(app->btn_create, "clicked", G_CALLBACK(on_btn_create_clicked), app);
    gtk_box_append(GTK_BOX(top_bar), app->btn_create);

    gtk_box_append(GTK_BOX(main_vbox), top_bar);
    
    // 分割线
    gtk_box_append(GTK_BOX(main_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    // ================= LIST AREA =================
    // 标题栏
    GtkWidget *list_title_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(list_title_box, 15);
    gtk_widget_set_margin_top(list_title_box, 10);
    GtkWidget *lbl_title = gtk_label_new("现有子系统列表");
    gtk_widget_add_css_class(lbl_title, "title-4");
    gtk_box_append(GTK_BOX(list_title_box), lbl_title);
    
    // 刷新按钮 (小)
    GtkWidget *btn_refresh = gtk_button_new_from_icon_name("view-refresh-symbolic");
    gtk_widget_set_valign(btn_refresh, GTK_ALIGN_CENTER);
    g_signal_connect_swapped(btn_refresh, "clicked", G_CALLBACK(refresh_container_list), app);
    gtk_box_append(GTK_BOX(list_title_box), btn_refresh);
    
    gtk_box_append(GTK_BOX(main_vbox), list_title_box);

    // 列表容器
    GtkWidget *scrolled_list = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled_list, TRUE); // 占据主要高度
    gtk_widget_set_margin_top(scrolled_list, 10);
    gtk_widget_set_margin_bottom(scrolled_list, 10);
    gtk_widget_set_margin_start(scrolled_list, 10);
    gtk_widget_set_margin_end(scrolled_list, 10);
    
    // 加上边框让它看起来更像个区域
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_frame_set_child(GTK_FRAME(frame), scrolled_list);
    gtk_box_append(GTK_BOX(main_vbox), frame);

    app->listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(app->listbox), GTK_SELECTION_NONE); // 不需要选中行
    gtk_widget_add_css_class(app->listbox, "rich-list"); // 样式类
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_list), app->listbox);

    // ================= LOG AREA (Bottom) =================
    // 简单的分割器，下面放日志
    GtkWidget *expander = gtk_expander_new("显示运行日志");
    gtk_widget_set_vexpand(expander, FALSE); // 不强制扩展，高度自适应
    gtk_widget_set_margin_start(expander, 10);
    gtk_widget_set_margin_end(expander, 10);
    gtk_widget_set_margin_bottom(expander, 10);

    GtkWidget *scrolled_log = gtk_scrolled_window_new();
    gtk_widget_set_size_request(scrolled_log, -1, 250); // 固定高度
    gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(scrolled_log), TRUE);
    
    app->textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->textview), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(app->textview), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_log), app->textview);
    
    gtk_expander_set_child(GTK_EXPANDER(expander), scrolled_log);
    gtk_box_append(GTK_BOX(main_vbox), expander);

    // 初始化显示
    gtk_window_present(GTK_WINDOW(app->window));
    refresh_container_list(app); // 启动时刷新列表
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.linuxbox.manager", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
