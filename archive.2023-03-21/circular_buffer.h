#if !defined(_circular_buffer_h_)

#define _circular_buffer_h_

struct circular_buffer_struct{
  int head;
  int tail;
	char *data;
	unsigned int underflow;
	unsigned int overflow;
	unsigned int buffer_size;
};

void circular_buffer_init(struct circular_buffer_struct *circular_buffer, int length);
int circular_buffer_length(struct circular_buffer_struct *circular_buffer);
char circular_buffer_read(struct circular_buffer_struct *circular_buffer);
int circular_buffer_write(struct circular_buffer_struct *circular_buffer, char byte_to_write);
void circular_buffer_empty(struct circular_buffer_struct *circular_buffer);

#endif //!defined(_circular_buffer_h_)