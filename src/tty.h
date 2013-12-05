#ifndef TTY_H
#define TTY_H

int tty_new(char *command, int bufnum);
void tty_save_termstate(void);
void tty_restore_termstate(void);
int tty_configure_control_tty(void);
int tty_set_winsize(int fd, int rows, int cols);
struct winsize tty_get_winsize(int fd);

#endif
