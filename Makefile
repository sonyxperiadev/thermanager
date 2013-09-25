CFLAGS := -Wall -g -I/usr/include/libxml2
LDFLAGS := -l xml2

proj := thermanager
srcs := \
	src/configuration.c \
	src/control.c \
	src/mitigation.c \
	src/resource.c \
	src/threshold.c \
	src/dom.c \
	src/libxml2parser.c \
	src/watch.c \
	src/thermal_zone.c \
	src/cpufreq.c \
	src/util.c \
	src/main.c \

out := out
src_to_obj = $(patsubst %.c,$(out)/obj/%.o,$(1))
src_to_dep = $(patsubst %.c,$(out)/dep/%.d,$(1))

all_srcs := $(sort $(srcs))
all_objs := $(call src_to_obj,$(all_srcs))
all_deps := $(call src_to_dep,$(all_srcs))

all: $(proj)

$(out)/obj/%.o: %.c
	@echo "CC	$<"
	@$(CC) -MM -MF $(call src_to_dep,$<) -MP -MT "$@ $(call src_to_dep,$<)" $(CFLAGS) $<
	@$(CC) -o $@ -c $< $(CFLAGS)

$(proj): $(call src_to_obj,$(all_srcs))
	@echo "LD	$@"
	@$(CC) -o $@ $^ $(LDFLAGS) -lfuse -ldl

clean:
	@echo CLEAN
	@$(RM) -r $(proj) $(out)

ifneq ("$(MAKECMDGOALS)","clean")
cmd-goal-1 := $(shell mkdir -p $(sort $(dir $(all_objs) $(all_deps))))
-include $(all_deps)
endif
