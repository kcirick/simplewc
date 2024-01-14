#ifndef ACTION_H
#define ACTION_H

void key_function(struct keymap*);
void mouse_function(struct simple_client*, struct mousemap*, int);
void process_ipc_action(const char*);

#endif
