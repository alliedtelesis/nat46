#include "nat46-tree.h"

struct tree4_s {
    struct in_addr addr;
    uint8_t plen;
    struct tree4_s *left, *right;
    void *priv;
};

struct tree6_s {
    struct in6_addr addr;
    uint8_t plen;
    struct tree6_s *left, *right;
    void *priv;
};

static bool v4_addr_bit(struct in_addr addr, uint8_t bit)
{
    return !!(ntohl (addr.s_addr) & (1ULL << (31 - bit)));
}

static bool v6_addr_bit(struct in6_addr addr, uint8_t bit)
{
    return !!(addr.s6_addr[bit >> 3] & (1ULL << (7 - (bit % 8))));
}

static struct tree4_s *tree4_create(struct in_addr addr, uint8_t plen)
{
    struct tree4_s *tmp = kzalloc(sizeof(struct tree4_s), GFP_ATOMIC);
    tmp->addr.s_addr = htonl((ntohl (addr.s_addr)) & (0xffffffff << (32 - plen)));
    tmp->plen = plen;
    return tmp;
}

static struct tree6_s *tree6_create(struct in6_addr addr, uint8_t plen)
{
    int i = 0;
    struct tree6_s *tmp = kzalloc(sizeof (struct tree6_s), GFP_ATOMIC);
    for (; i < 16; i++)
    {
        if ((plen >> 3) > i)
        {
            tmp->addr.s6_addr[i] = addr.s6_addr[i];
        }
        else if ((plen >> 3) == i)
        {
            tmp->addr.s6_addr[i] = (addr.s6_addr[i] & (0xff << (8 - (plen % 8))));
        }
        else
        {
            tmp->addr.s6_addr[i] = 0x0;
        }
    }
    tmp->plen = plen;
    return tmp;
}

static bool tree46_insert_v4(struct tree4_s *tree, struct in_addr addr, uint8_t plen, void *priv)
{
    if (tree->plen == plen)
    {
        /* Insert here, or there is a conflict */
        if (tree->priv == NULL)
        {
            tree->priv = priv;
            return true;
        }
        return false;
    }
    if (v4_addr_bit (addr, tree->plen))
    {
        /* Go right */
        if (tree->right == NULL)
        {
            /* Insert here */
            tree->right = tree4_create(addr, tree->plen + 1);
        }
        return tree46_insert_v4(tree->right, addr, plen, priv);
    }
    else
    {
        /* Go left */
        if (tree->left == NULL)
        {
            /* Insert here */
            tree->left = tree4_create(addr, tree->plen + 1);
        }
        return tree46_insert_v4(tree->left, addr, plen, priv);
    }
}

static bool tree46_remove_v4(struct tree4_s **tree, struct in_addr addr, uint8_t plen)
{
    bool res = false;
    if (*tree == NULL)
    {
        return false;
    }
    if ((*tree)->plen == plen)
    {
        /* Disconnect this element */
        (*tree)->priv = NULL;
        res = true;
    }
    else
    {
        if (v4_addr_bit (addr, (*tree)->plen))
        {
            res = tree46_remove_v4(&((*tree)->right), addr, plen);
        }
        else
        {
            res = tree46_remove_v4(&((*tree)->left), addr, plen);
        }
    }
    if (res)
    {
        /* If we have no children, cleanup */
        if ((*tree)->priv == NULL && (*tree)->right == NULL && (*tree)->left == NULL)
        {
            struct tree4_s *tmp = *tree;
            *tree = NULL;
            kfree(tmp);
        }
    }
    return res;
}

static bool tree46_insert_v6(struct tree6_s *tree, struct in6_addr addr, uint8_t plen, void *priv)
{
    if (tree->plen == plen)
    {
        /* Insert here, or there is a conflict */
        if (tree->priv == NULL)
        {
            tree->priv = priv;
            return true;
        }
        return false;
    }
    if (v6_addr_bit(addr, tree->plen))
    {
        /* Go right */
        if (tree->right == NULL)
        {
            /* Insert here */
            tree->right = tree6_create(addr, tree->plen + 1);
        }
        return tree46_insert_v6(tree->right, addr, plen, priv);
    }
    else
    {
        /* Go left */
        if (tree->left == NULL)
        {
            /* Insert here */
            tree->left = tree6_create(addr, tree->plen + 1);
        }
        return tree46_insert_v6(tree->left, addr, plen, priv);
    }
}

static bool tree46_remove_v6(struct tree6_s **tree, struct in6_addr addr, uint8_t plen)
{
    bool res = false;
    if (*tree == NULL)
    {
        return false;
    }
    if ((*tree)->plen == plen)
    {
        /* Disconnect this element */
        (*tree)->priv = NULL;
        res = true;
    }
    else
    {
        if (v6_addr_bit (addr, (*tree)->plen))
        {
            res = tree46_remove_v6(&((*tree)->right), addr, plen);
        }
        else
        {
            res = tree46_remove_v6(&((*tree)->left), addr, plen);
        }
    }
    if (res)
    {
        /* If we have no children, cleanup */
        if ((*tree)->priv == NULL && (*tree)->right == NULL && (*tree)->left == NULL)
        {
            struct tree6_s *tmp = *tree;
            *tree = NULL;
            kfree (tmp);
        }
    }
    return res;
}

bool tree46_insert(struct tree46_s *tree, struct in_addr addr4, uint8_t plen4, struct in6_addr addr6, uint8_t plen6, void *priv)
{
    if (tree->v4 == NULL)
    {
        tree->v4 = tree4_create(addr4, 0);
    }
    if (!tree46_insert_v4(tree->v4, addr4, plen4, priv))
    {
        return false;
    }
    if (tree->v6 == NULL)
    {
        tree->v6 = tree6_create(addr6, 0);
    }
    if (!tree46_insert_v6(tree->v6, addr6, plen6, priv))
    {
        tree46_remove_v4(&tree->v4, addr4, plen4);
        return false;
    }
    return true;
}

bool tree46_remove(struct tree46_s *tree, struct in_addr addr4, uint8_t plen4, struct in6_addr addr6, uint8_t plen6)
{
    bool ret = false;
    if (tree46_remove_v4(&tree->v4, addr4, plen4))
    {
        ret = true;
    }
    if (tree46_remove_v6(&tree->v6, addr6, plen6))
    {
        ret = true;
    }
    return ret;
}

void *tree46_find_best_v4(struct tree46_s *tree, struct in_addr addr4)
{
    struct tree4_s *temp4 = tree->v4;
    void *last_priv = NULL;
    uint8_t lvl = 0;
    for (; temp4 && lvl <= 32; lvl++)
    {
        if (temp4->priv)
        {
            last_priv = temp4->priv;
        }
        if (v4_addr_bit (addr4, lvl))
        {
            temp4 = temp4->right;
        }
        else
        {
            temp4 = temp4->left;
        }
    }
    return last_priv;
}

void *tree46_find_best_v6(struct tree46_s *tree, struct in6_addr addr6)
{
    struct tree6_s *temp6 = tree->v6;
    void *last_priv = NULL;
    uint8_t lvl = 0;
    for (; temp6 && lvl <= 128; lvl++)
    {
        if (temp6->priv)
        {
            last_priv = temp6->priv;
        }
        if (v6_addr_bit(addr6, lvl))
        {
            temp6 = temp6->right;
        }
        else
        {
            temp6 = temp6->left;
        }
    }
    return last_priv;
}

static void tree46_walk_v4(struct tree4_s *t, void (*fn)(void *, void *), void *priv)
{
    if (t == NULL)
    {
        return;
    }
    if (t->priv)
    {
        fn(priv, t->priv);
    }
    tree46_walk_v4(t->left, fn, priv);
    tree46_walk_v4(t->right, fn, priv);
}

void tree46_walk(struct tree46_s *tree, void (*fn)(void *, void *), void *priv)
{
    if (tree != NULL)
    {
        tree46_walk_v4(tree->v4, fn, priv);
    }
}
