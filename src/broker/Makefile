include ../MakeVars

TCMALLOC_FLAG = -std=c11 -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
CFLAG         = $(CFLAGS) -D_GNU_SOURCE -Wall -Wno-unused-function $(TCMALLOC_FLAG)

BUILD_INC_PATH  = -I../build/include -I../../../include
BUILD_LIBS_PATH = ../build/lib
BUILD_LIBS = $(BUILD_LIBS_PATH)/libevent_pthreads.a \
             $(BUILD_LIBS_PATH)/libevent.a \
             $(BUILD_LIBS_PATH)/libfort.a \
             $(BUILD_LIBS_PATH)/libzmq.a \
             $(BUILD_LIBS_PATH)/libelem.a \
			 $(BUILD_LIBS_PATH)/libtcmalloc_minimal.a \
             -lpthread -lrt -lm -ldl -lstdc++

SRCS        = main.c tr_recv.c tr_send.c tr_mgnt.c

OBJS        = $(SRCS:.c=.o)

AOUT        = broker

DEST        = ../bin

#
#---------------------------------------------------------------------
#
all:: clean $(AOUT) install

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAG) $(BUILD_INC_PATH) $(INC_PATH) -c $<

$(AOUT): $(OBJS)
	$(CC) $(CFLAG) -o $(AOUT) $(OBJS) $(BUILD_LIBS) $(LIB_PATH) $(LIBS) 

install:
	\cp zmessage.h ../build/include
	\cp $(AOUT) $(DEST)

clean:
	\rm -f $(OBJS) $(AOUT) core.* 
	rm -rf cscope.out tceetree.out tceetree.out.png
	rm -rf *.csv
