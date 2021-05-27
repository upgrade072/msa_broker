include ../MakeVars

CFLAG       = $(CFLAGS) -Wno-char-subscripts

SRCS1       = elem.c
OBJS1       = $(SRCS1:.c=.o)
AOUT1       = libelem.a

AOUTS		= $(AOUT1)
DEST        = ../build

#
#---------------------------------------------------------------------
#
all:: clean $(AOUT1) install

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAG) $(BUILD_INC_PATH) $(INC_PATH) -c $<

$(AOUT1): $(OBJS1)
	$(AR) -rc $(AOUT1) $(OBJS1)

install:
	\cp $(AOUT1) $(DEST)/lib
	\cp elem.h $(DEST)/include

clean:
	\rm -f $(AOUTS) *.o core.* 
