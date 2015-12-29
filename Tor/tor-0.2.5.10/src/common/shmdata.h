/* Added by smkim */

#ifndef _SHMDATA_H_HEADER
#define _SHMDATA_H_HEADER

// TEXT SIZE
#define TEXT_SIZE 2048

// Key
#define MEM_KEY 1234

struct shared_use_st {
	int written;
	char text[TEXT_SIZE];
};

/*--------------------------------------------------------------*/

/* Circular buffer object */
typedef struct {
    int         size;   /* maximum number of elements           */
    int         start;  /* index of oldest element              */
    int         end;    /* index at which to write new element  */
    int         s_msb;
    int         e_msb;
    struct shared_use_st *elems;  /* vector of elements                   */
} CircularBuffer;
 
void cbInit(CircularBuffer *cb, int size) {
    cb->size  = size;
    cb->start = 0;
    cb->end   = 0;
    cb->s_msb = 0;
    cb->e_msb = 0;
    cb->elems = (struct shared_use_st *)calloc(cb->size, sizeof(struct shared_use_st));
}
 
int cbIsFull(CircularBuffer *cb) {
    return cb->end == cb->start && cb->e_msb != cb->s_msb; }
 
int cbIsEmpty(CircularBuffer *cb) {
    return cb->end == cb->start && cb->e_msb == cb->s_msb; }
 
void cbIncr(CircularBuffer *cb, int *p, int *msb) {
    *p = *p + 1;
    if (*p == cb->size) {
        *msb ^= 1;
        *p = 0;
    }
}
 
void cbWrite(CircularBuffer *cb, struct shared_use_st *elem) {
    cb->elems[cb->end] = *elem;
    if (cbIsFull(cb)) /* full, overwrite moves start pointer */
        cbIncr(cb, &cb->start, &cb->s_msb);
    cbIncr(cb, &cb->end, &cb->e_msb);
}
 
void cbRead(CircularBuffer *cb, struct shared_use_st *elem) {
    *elem = cb->elems[cb->start];
    cbIncr(cb, &cb->start, &cb->s_msb);
}

/*--------------------------------------------------------------*/

#endif
