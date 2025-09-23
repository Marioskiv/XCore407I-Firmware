#pragma once
#include <stdint.h>
#include <stdlib.h>
struct pbuf {
  struct pbuf *next;
  void *payload;
  uint16_t len;
  uint16_t tot_len;
};
#define PBUF_RAW 0
#define PBUF_POOL 0
#define PBUF_TRANSPORT 0
static inline struct pbuf* pbuf_alloc(int layer, uint16_t length, int type){(void)layer;(void)type; struct pbuf* p=(struct pbuf*)malloc(sizeof(struct pbuf)); if(!p)return NULL; p->next=NULL; p->len=length; p->tot_len=length; p->payload=malloc(length); if(!p->payload){free(p);return NULL;} return p;}
static inline void pbuf_free(struct pbuf* p){ if(!p) return; free(p->payload); free(p);} 
static inline void pbuf_copy_partial(struct pbuf* p, void* dst, uint16_t length, uint16_t offset){ if(!p||offset+length>p->len)return; memcpy(dst, (uint8_t*)p->payload+offset, length);} 
static inline void pbuf_take(struct pbuf* p, const void* src, uint16_t length){ if(!p||length>p->len)return; memcpy(p->payload, src, length);} 
