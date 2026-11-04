/* Compiler-generated freestanding memory calls; LVGL uses its own allocator. */
#include <stddef.h>
void *memcpy(void *d,const void *s,size_t n) {unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int v,size_t n) {unsigned char *a=d;while(n--)*a++=(unsigned char)v;return d;}
void *memmove(void *d,const void *s,size_t n) {
    unsigned char *a=d;const unsigned char *b=s;
    if((size_t)a<(size_t)b)while(n--)*a++=*b++;
    else {a+=n;b+=n;while(n--)*--a=*--b;}return d;
}
int memcmp(const void *a,const void *b,size_t n) {
    const unsigned char *x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;x++;y++;}return 0;
}
