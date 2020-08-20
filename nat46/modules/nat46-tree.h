#include "nat46-glue.h"

struct tree46_s {
    struct tree4_s *v4;
    struct tree6_s *v6;
};

bool tree46_insert(struct tree46_s *tree, struct in_addr addr4, uint8_t plen4, struct in6_addr addr6, uint8_t plen6, void *priv);
bool tree46_remove(struct tree46_s *tree, struct in_addr addr4, uint8_t plen4, struct in6_addr addr6, uint8_t plen6);
void *tree46_find_best_v4(struct tree46_s *tree, struct in_addr addr4);
void *tree46_find_best_v6(struct tree46_s *tree, struct in6_addr addr6);
void tree46_walk(struct tree46_s *tree, void (*fn)(void *, void *), void *priv);
