#ifndef LINUXBOX_H
#define LINUXBOX_H

#include <gtk/gtk.h>

#define CONFIG_FILE_LOCAL "distros.conf"
#define CONFIG_FILE_SYSTEM "/usr/share/linuxbox/distros.conf"

// 主程序结构体
typedef struct {
    GtkWidget *window;
    GtkWidget *dropdown;    // 类型选择
    GtkWidget *entry_name;  // 名称输入
    GtkWidget *btn_create;  // 创建按钮
    GtkWidget *listbox;     // 容器列表
    GtkWidget *textview;    // 日志输出
    GKeyFile *key_file;
    
    // 配置数据
    char **distro_ids;
    guint distro_count;

    // 临时状态
    char *pending_name;
    char *pending_distro;
    char *pending_icon;
} AppContext;

void append_log(GtkWidget *textview, const char *fmt, ...);
void create_desktop_entry(const char *name, const char *distro, const char *icon, AppContext *app);
void remove_desktop_entry(const char *name, AppContext *app);
gboolean container_exists(const char *name);

// 刷新列表
void refresh_container_list(AppContext *app);
// 删除容器逻辑
void delete_container(const char *name, AppContext *app);
gchar *resolve_config_file(void);

#endif
