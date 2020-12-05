#include "syshead.h"
#include "skbuff.h"
#include "list.h"

struct sk_buff *AllocSkb(unsigned int size) {
    struct sk_buff *skb = malloc(sizeof(struct sk_buff));

    memset(skb, 0, sizeof(struct sk_buff));
    skb->data = malloc(size);
    memset(skb->data, 0, size);

    skb->refcnt = 0;
    skb->head = skb->data;
    skb->end = skb->data + size;
    skb->txmitted = 0;

    ListInit(&skb->list);

    return skb;
}

void FreeSkb(struct sk_buff *skb) {
    if (skb->refcnt < 1) {
        free(skb->head);
        free(skb);
    }
}

void *SkbReserve(struct sk_buff *skb, unsigned int len) {
    skb->data += len;
    return skb->data;
}

uint8_t *SkbPush(struct sk_buff *skb, unsigned int len) {
    skb->data -= len;
    skb->len += len;
    return skb->data;
}

uint8_t *SkbHead(struct sk_buff *skb) {
    return skb->head;
}

void SkbResetHeader(struct sk_buff *skb) {
    skb->data = skb->end - skb->dlen;
    skb->len = skb->dlen;
}
