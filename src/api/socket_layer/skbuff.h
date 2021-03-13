#ifndef SKBUFF_H_
#define SKBUFF_H_

#include "syshead.h"
#include "utils.h"
#include "list.h"
#include <pthread.h>

struct sk_buff {
    struct list_head list;
    int refcnt;
    int seqset;
    int txmitted;
    uint16_t protocol;
    uint32_t len;
    uint32_t dlen;
    uint32_t seq;
    uint32_t end_seq;
    uint8_t *end;
    uint8_t *head;
    uint8_t *data;
    uint8_t *payload;
};

struct sk_buff_head {
    struct list_head head;
    uint32_t qlen;
};

struct sk_buff *AllocSkb(unsigned int size);
void FreeSkb(struct sk_buff *skb);
uint8_t *SkbPush(struct sk_buff *skb, unsigned int len);
uint8_t *SkbHead(struct sk_buff *skb);
void *SkbReserve(struct sk_buff *skb, unsigned int len);
void SkbResetHeader(struct sk_buff *skb);

static inline uint32_t SkbQueueLen(const struct sk_buff_head *list) {
    return list->qlen;
}

static inline void SkbQueueInit(struct sk_buff_head *list) {
    ListInit(&list->head);
    list->qlen = 0;
}

static inline void SkbQueueAdd(struct sk_buff_head *list, struct sk_buff *new_e,
                               struct sk_buff *next) {
    ListAddTail(&new_e->list, &next->list);
    list->qlen += 1;
}

static inline void SkbQueueTail(struct sk_buff_head *list, struct sk_buff *new_e) {
    ListAddTail(&new_e->list, &list->head);
    list->qlen += 1;
}

static inline struct sk_buff *SkbDequeue(struct sk_buff_head *list) {
    struct sk_buff *skb = list_first_entry(&list->head, struct sk_buff, list);
    ListDel(&skb->list);
    list->qlen -= 1;

    return skb;
}

static inline int SkbQueueEmpty(const struct sk_buff_head *list) {
    return SkbQueueLen(list) < 1;
}

static inline struct sk_buff *SkbPeek(struct sk_buff_head *list) {
    if (SkbQueueEmpty(list)) return NULL;
    return list_first_entry(&list->head, struct sk_buff, list);
}

static inline struct sk_buff *SkbLastPeek(struct sk_buff_head *list) {
    if (SkbQueueEmpty(list)) return NULL;
    return list_last_entry(&list->head, struct sk_buff, list);
}

static inline void SkbQueueFree(struct sk_buff_head *list) {
    struct sk_buff *skb = NULL;
    
    while ((skb = SkbPeek(list)) != NULL) {
        SkbDequeue(list);
        skb->refcnt--;
        FreeSkb(skb);
    }
}

#endif
