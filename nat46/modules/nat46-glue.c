/*
 * glue functions, candidates to go to -core
 *
 * Copyright (c) 2013-2014 Andrew Yourtchenko <ayourtch@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */


#include "nat46-glue.h"
#include "nat46-core.h"

static DEFINE_MUTEX(ref_lock);
static int is_valid_nat46(nat46_instance_t *nat46) {
  return (nat46 && (nat46->sig == NAT46_SIGNATURE));
}

nat46_instance_t *alloc_nat46_instance(void) {
  nat46_instance_t *nat46 = kzalloc(sizeof(nat46_instance_t), GFP_KERNEL);

  if (!nat46) {
    printk("[nat46] make_nat46_instance: can not alloc a nat46 instance\n");
    return NULL;
  } else {
    printk("[nat46] make_nat46_instance: allocated nat46 instance\n");
  }

  nat46->sig = NAT46_SIGNATURE;
  nat46->refcount = 1; /* The caller gets the reference */

  return nat46;
}


nat46_instance_t *get_nat46_instance(struct sk_buff *sk) {
  nat46_instance_t *nat46 = netdev_nat46_instance(sk->dev);
  mutex_lock(&ref_lock);
  if (is_valid_nat46(nat46)) {
    nat46->refcount++;
    mutex_unlock(&ref_lock);
    return nat46;
  } else {
    printk("[nat46] get_nat46_instance: Could not find a valid NAT46 instance!");
    mutex_unlock(&ref_lock);
    return NULL;
  }
}

struct pair_list_s {
    nat46_xlate_rulepair_t **pairs;
    size_t pairs_count;
    size_t pairs_allocated;
};

void add_to_list(void *arg1, void *arg2) {
    struct pair_list_s *pl = (struct pair_list_s *)arg1;
    nat46_xlate_rulepair_t *pair = (nat46_xlate_rulepair_t *)arg2;
    if (pl->pairs_allocated == 0)
    {
        pl->pairs = kmalloc(sizeof (nat46_xlate_rulepair_t *), GFP_KERNEL);
        pl->pairs_allocated = 1;
    } else if (pl->pairs_count >= pl->pairs_allocated)
    {
        pl->pairs = krealloc(pl->pairs, sizeof (nat46_xlate_rulepair_t *) * (pl->pairs_allocated * 2), GFP_KERNEL);
        pl->pairs_allocated *= 2;
    }
    pl->pairs[pl->pairs_count++] = pair;
}

static void release_rules(struct tree46_s *rules) {
   struct pair_list_s pairs = { 0 };
   size_t i;

  tree46_walk(rules, add_to_list, &pairs);
  for(i = 0; i < pairs.pairs_count; i++)
  {
    nat46_xlate_rulepair_t *rule = pairs.pairs[i];
    if (rule->local.v6_pref_len)
    {
      tree46_remove(rules, rule->local.v4_pref, rule->local.v4_pref_len, rule->local.v6_pref, rule->local.v6_pref_len);
    }
    if (rule->remote.v6_pref_len)
    {
      tree46_remove(rules, rule->remote.v4_pref, rule->remote.v4_pref_len, rule->remote.v6_pref, rule->remote.v6_pref_len);
    }
    kfree(rule);
  }
  kfree(pairs.pairs);
}

void release_nat46_instance(nat46_instance_t *nat46) {
  mutex_lock(&ref_lock);

  nat46->refcount--;

  if(0 == nat46->refcount) {
    printk("[nat46] release_nat46_instance: freeing nat46 instance\n");
    nat46->sig = FREED_NAT46_SIGNATURE;
    /* Remove all rules from this instance before freeing the instance */
    /* Capture all current rules to a list */
    release_rules(&nat46->local_rules);
    release_rules(&nat46->remote_rules);
    kfree(nat46);
  }
  mutex_unlock(&ref_lock);
}
