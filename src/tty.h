#ifndef TTY_H
#define TTY_H

int tty_new(char *command);
void tty_save_termstate(void);
void tty_restore_termstate(void);
int tty_configure_control_tty(void);

#endif
