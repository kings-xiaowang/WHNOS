/* shell.h — WHNos 交互式 Shell */
#ifndef SHELL_H
#define SHELL_H

class Shell {
public:
    static void run();

    // 命令处理函数 (公开给命令表)
    static void cmd_help();
    static void cmd_clear();
    static void cmd_echo(const char* args);
    static void cmd_about();
    static void cmd_calc(const char* args);
    static void cmd_colors();
    static void cmd_meminfo();
    static void cmd_date();
    static void cmd_uptime();
    static void cmd_cpuinfo();
    static void cmd_reboot();
    static void cmd_shutdown();
    static void cmd_sleep(const char* args);
    static void cmd_clock();
    static void cmd_sudo(const char* args);
    static void cmd_netinfo();
    static void cmd_ping(const char* args);
    static void cmd_http(const char* args);
    static void cmd_ver();
    static void cmd_whoami();
    static void cmd_ascii();
    static void cmd_beep(const char* args);
    static void cmd_hexdump(const char* args);
    static void cmd_pci();
    static void cmd_gdt();
    static void cmd_pkg(const char* args);
};

#endif
