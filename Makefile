CC = gcc
CFLAGS = -Wall -Wextra -g

vpath %.c src/

OBJSDIR = build
OBJS = socket daycare http_parse stats http main
LIBS = pthread
LIBSFULL = $(addprefix -l , $(LIBS))
OBJSFULL = 	$(addprefix $(OBJSDIR)/,\
		$(addsuffix .o, $(OBJS)))


htdelire: $(OBJSFULL)
	$(CC) $(CFLAGS) $(LIBSFULL) -o $@ $^

$(OBJSDIR)/%.o: %.c
	mkdir -p $(OBJSDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJSDIR)/*
