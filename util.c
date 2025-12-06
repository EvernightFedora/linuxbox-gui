#include "linuxbox.h"
#include <stdarg.h>
#include <glib/gstdio.h>

void append_log(GtkWidget *textview, const char *fmt, ...) {
    if (!textview) return;
    
    va_list args;
    va_start(args, fmt);
    char *msg = g_strdup_vprintf(fmt, args);
    va_end(args);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    
    gtk_text_buffer_insert(buffer, &end, msg, -1);
    gtk_text_buffer_insert(buffer, &end, "\n", 1);
    
    // 自动滚动
    GtkTextMark *mark = gtk_text_buffer_create_mark(buffer, NULL, &end, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(textview), mark, 0.0, TRUE, 0.0, 1.0);
    
    g_free(msg);
}

void create_desktop_entry(const char *name, const char *distro, const char *icon, AppContext *app) {
    const char *home = g_get_home_dir();
    char *app_dir = g_strdup_printf("%s/.local/share/applications", home);
    
    if (g_mkdir_with_parents(app_dir, 0755) != 0) {
        append_log(app->textview, "❌ 错误: 无法创建 applications 目录");
        g_free(app_dir); return;
    }

    char *file_path = g_strdup_printf("%s/%s.desktop", app_dir, name);
    char *content = g_strdup_printf(
        "[Desktop Entry]\nName=%s (%s)\nExec=/usr/bin/distrobox enter %s\nIcon=%s\nTerminal=true\nType=Application\nCategories=LinuxBox;\n",
        name, distro, name, icon ? icon : "terminal"
    );

    if (g_file_set_contents(file_path, content, -1, NULL)) {
        append_log(app->textview, "✅ 快捷方式已创建: %s", name);
    }
    
    g_free(content); g_free(file_path); g_free(app_dir);
}

void remove_desktop_entry(const char *name, AppContext *app) {
    const char *home = g_get_home_dir();
    char *file_path = g_strdup_printf("%s/.local/share/applications/%s.desktop", home, name);
    if (g_file_test(file_path, G_FILE_TEST_EXISTS)) {
        g_remove(file_path);
        append_log(app->textview, "🗑️ 快捷方式已清理: %s", name);
    }
    g_free(file_path);
}

gboolean container_exists(const char *name) {
    char *cmd = g_strdup_printf("podman inspect --type container %s", name);
    int status = system(cmd); // system是同步的，对于inspect这种极快操作可以接受
    g_free(cmd);
    return (status == 0);
}

gchar *resolve_config_file(void) {
    const char *local = CONFIG_FILE_LOCAL;
    const char *system = CONFIG_FILE_SYSTEM;

    if (g_file_test(local, G_FILE_TEST_EXISTS))
        return g_strdup(local);
    if (g_file_test(system, G_FILE_TEST_EXISTS))
        return g_strdup(system);

    // 都不存在时返回本地路径，便于后续创建/写入
    return g_strdup(local);
}