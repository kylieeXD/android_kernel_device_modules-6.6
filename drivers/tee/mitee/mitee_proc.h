#ifndef __MITEE_PROC_H
#define __MITEE_PROC_H

struct optee;

int mitee_proc_init(struct optee *optee);
void mitee_proc_deinit(struct optee *optee);

#endif
