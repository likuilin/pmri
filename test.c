#include <stdio.h>
#include <string.h>
#include <libpmemobj.h>

#define MAX_BUF_LEN 10 /* maximum length of our buffer */

struct my_root {
	size_t len; /* = strlen(buf) */
	char buf[MAX_BUF_LEN];
};

// https://pmem.io/2015/06/13/accessing-pmem.html
int main()
{
    if (1) {
        // write
        PMEMobjpool *pop = pmemobj_create("layout_file", "layout", PMEMOBJ_MIN_POOL, 0666);
        if (pop == NULL) {
            perror("pmemobj_create");
            return 1;
        }

        PMEMoid root = pmemobj_root(pop, sizeof (struct my_root));
        struct my_root *rootp = pmemobj_direct(root);

        char buf[MAX_BUF_LEN];
        scanf("%9s", buf);

        rootp->len = strlen(buf);
        pmemobj_persist(pop, &rootp->len, sizeof (rootp->len));
        pmemobj_memcpy_persist(pop, rootp->buf, buf, rootp->len);

        pmemobj_close(pop);
    } else {
        // read
        PMEMobjpool *pop = pmemobj_open("layout_file", "layout");
        if (pop == NULL) {
            perror("pmemobj_open");
            return 1;
        }

        PMEMoid root = pmemobj_root(pop, sizeof (struct my_root));
        struct my_root *rootp = pmemobj_direct(root);

        if (rootp->len == strlen(rootp->buf))
            printf("%s\n", rootp->buf);

        pmemobj_close(pop);
    }

	return 0;
}

