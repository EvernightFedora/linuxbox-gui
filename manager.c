#include "linuxbox.h"
#include <string.h>

// --- 删除逻辑的回调 ---

static void on_delete_output(GObject *source, GAsyncResult *res, gpointer user_data) {
    GDataInputStream *stream = G_DATA_INPUT_STREAM(source);
    AppContext *app = (AppContext *)user_data;
    char *line = g_data_input_stream_read_line_finish(stream, res, NULL, NULL);
    if (line) {
        append_log(app->textview, "%s", line);
        g_free(line);
        g_data_input_stream_read_line_async(stream, G_PRIORITY_DEFAULT, NULL, on_delete_output, app);
    }
}

static void on_delete_done(GObject *source, GAsyncResult *res, gpointer user_data) {
    GSubprocess *proc = G_SUBPROCESS(source);
    AppContext *app = (AppContext *)user_data;
    
    if (g_subprocess_wait_finish(proc, res, NULL)) {
        if (g_subprocess_get_successful(proc)) {
            append_log(app->textview, "✅ 删除成功。");
            refresh_container_list(app); // 删除后刷新列表
        } else {
            append_log(app->textview, "❌ 删除失败。");
        }
    }
    // 恢复UI状态（如果有锁定）
}

// 每一行上的“删除”按钮点击事件
static void on_row_delete_clicked(GtkWidget *btn, gpointer user_data) {
    // 这里的 user_data 是容器名称字符串
    char *name = (char *)user_data;
    
    // 获取 AppContext (通过 btn 的 root window 或者全局传递，这里我们在 btn 上绑定了 data)
    // 为了简单，我们使用 g_object_get_data 获取传递的 app 指针
    AppContext *app = g_object_get_data(G_OBJECT(btn), "app_ptr");

    append_log(app->textview, "正在删除: %s ...", name);
    remove_desktop_entry(name, app);

    GError *err = NULL;
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE);
    GSubprocess *proc = g_subprocess_launcher_spawn(launcher, &err, "distrobox", "rm", "--force", name, NULL);
    
    if (proc) {
        GInputStream *pipe = g_subprocess_get_stdout_pipe(proc);
        GDataInputStream *stream = g_data_input_stream_new(pipe);
        g_data_input_stream_read_line_async(stream, G_PRIORITY_DEFAULT, NULL, on_delete_output, app);
        g_subprocess_wait_async(proc, NULL, on_delete_done, app);
    } else {
        append_log(app->textview, "启动失败: %s", err->message);
        g_error_free(err);
    }
    g_object_unref(launcher);
}

// --- 列表构建逻辑 ---

// 辅助函数：清除 ListBox 所有子项
static void clear_listbox(GtkWidget *listbox) {
    GtkWidget *child = gtk_widget_get_first_child(listbox);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(listbox), child);
        child = next;
    }
}

void refresh_container_list(AppContext *app) {
    clear_listbox(app->listbox);

    // 获取容器列表：格式 ID|Names|Status|Image
    char *cmd = "podman ps -a --format \"{{.Names}}|{{.Status}}|{{.Image}}\"";
    char *out = NULL;
    
    if (g_spawn_command_line_sync(cmd, &out, NULL, NULL, NULL)) {
        if (!out) return;
        
        char **lines = g_strsplit(out, "\n", -1);
        for (int i = 0; lines[i] != NULL; i++) {
            if (strlen(lines[i]) < 2) continue; // 跳过空行

            char **parts = g_strsplit(lines[i], "|", 3);
            if (g_strv_length(parts) >= 3) {
                char *name = parts[0];
                char *status = parts[1];
                char *image = parts[2];

                // --- 构建一行 UI ---
                // Row (HBox)
                // [ Icon ] [ Name (Bold) ] [ Image (Gray) ] [ Status ]  <space>  [ Delete Button ]
                
                GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
                gtk_widget_set_margin_top(row_box, 8);
                gtk_widget_set_margin_bottom(row_box, 8);
                gtk_widget_set_margin_start(row_box, 10);
                gtk_widget_set_margin_end(row_box, 10);

                // 1. 图标
                GtkWidget *icon = gtk_image_new_from_icon_name("package-x-generic-symbolic");
                gtk_box_append(GTK_BOX(row_box), icon);

                // 2. 信息区 (VBox: Name + Image)
                GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
                
                char *markup_name = g_strdup_printf("<b>%s</b>", name);
                GtkWidget *lbl_name = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(lbl_name), markup_name);
                gtk_widget_set_halign(lbl_name, GTK_ALIGN_START);
                gtk_box_append(GTK_BOX(info_box), lbl_name);
                g_free(markup_name);

                GtkWidget *lbl_img = gtk_label_new(image);
                gtk_widget_add_css_class(lbl_img, "dim-label"); // 变灰
                gtk_label_set_ellipsize(GTK_LABEL(lbl_img), PANGO_ELLIPSIZE_END); // 这里的图片名可能很长
                gtk_widget_set_halign(lbl_img, GTK_ALIGN_START);
                gtk_box_append(GTK_BOX(info_box), lbl_img);

                gtk_widget_set_hexpand(info_box, TRUE); // 让中间区域占据空间
                gtk_box_append(GTK_BOX(row_box), info_box);

                // 3. 状态标签
                GtkWidget *lbl_status = gtk_label_new(status);
                // 简单的状态着色逻辑
                if (g_str_has_prefix(status, "Up")) {
                    gtk_widget_add_css_class(lbl_status, "success"); // 自带主题绿色
                } else {
                    gtk_widget_add_css_class(lbl_status, "warning");
                }
                gtk_box_append(GTK_BOX(row_box), lbl_status);

                // 4. 删除按钮
                GtkWidget *btn_del = gtk_button_new_from_icon_name("user-trash-symbolic");
                gtk_widget_add_css_class(btn_del, "destructive-action"); // 红色
                
                // 绑定数据：把 Name 字符串绑定到按钮上，记得释放
                char *name_dup = g_strdup(name);
                g_signal_connect_data(btn_del, "clicked", G_CALLBACK(on_row_delete_clicked), 
                                      name_dup, (GClosureNotify)g_free, 0);
                // 传递 AppContext 指针
                g_object_set_data(G_OBJECT(btn_del), "app_ptr", app);
                
                gtk_box_append(GTK_BOX(row_box), btn_del);

                // 添加到 ListBox
                gtk_list_box_append(GTK_LIST_BOX(app->listbox), row_box);
            }
            g_strfreev(parts);
        }
        g_strfreev(lines);
        g_free(out);
    }
}
