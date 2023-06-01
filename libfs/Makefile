# Target library
lib := libfs.a
CC := gcc
AR := ar rcs
objs := disk.o fs.o
CFLAGS := -Wall -Wextra -Werror -MMD -l
CFLAGS += -g

ifneq ($(V), 1)
Q = @
endif
all: $(lib)

## TODO: Phase 1
deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)

$(lib): $(objs)
	@echo "AR $@"
	$(AR) $(lib) $(objs)

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "Clean"
	$(Q)rm -f $(lib) $(objs) $(deps)
