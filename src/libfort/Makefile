include ../MakeVars

CFLAG       = $(CFLAGS) -Wno-char-subscripts # option for util.c

SRCS        = fort.c
OBJS        = $(SRCS:.c=.o)

AOUT        = libfort.a
DEST        = ../build

#
#---------------------------------------------------------------------
#
all:: clean $(AOUT) install

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAG) $(BUILD_INC_PATH) $(INC_PATH) -c $<

$(AOUT): $(OBJS)
	$(AR) -rc $(AOUT) $(OBJS)

install:
	\cp $(AOUT) $(DEST)/lib
	\cp fort.h $(DEST)/include

clean:
	\rm -f $(OBJS) $(AOUT) core.*
