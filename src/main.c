#include <stdlib.h>
#include <stdio.h>

#include <iksemel.h>

#include "jabber_bind.h"

int main(void) {
    iks* config = 0;
	JabberBind* bind = 0;
    int ret;

    ret = iks_load("config.xml", &config);

    if(ret != IKS_OK) {
        fprintf(stderr, "Could not load config.xml.\n");
        return 1;
    }

    bind = jb_new(config);

    iks_delete(config);

	if(bind == NULL) {
        fprintf(stderr, "Failed to start service.\n");
		return 1;
	}

	jb_run(bind);

	jb_delete(bind);

    return 0;
}
