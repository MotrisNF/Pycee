NAME = pycee

CC = gcc

CFLAGS    = -Wall -Wextra -Werror -Iinclude -MMD -MP
DBG_FLAGS = -g -fsanitize=address,undefined
REL_FLAGS = -O2

# One line per layer; a layer only uses the ones above it.
CORE     = arena errors reader source stack buffer utf8
FRONT    = tokens lexer parser ast_print
RESOLVER = native_defs symtab scope declarations objects expressions statements modules resolver
RUNTIME  = value heap interp eval exec call ops format fmtspec methods signals
TYPES    = number range tuple list string seq table dict object error file unicode_data
NATIVES  = natives sys math io random time os signal json
LIBS     = -lm

SRCS = src/main.c \
       $(CORE:%=src/core/%.c) \
       $(FRONT:%=src/front/%.c) \
       $(RESOLVER:%=src/resolver/%.c) \
       $(RUNTIME:%=src/runtime/%.c) \
       $(TYPES:%=src/types/%.c) \
       $(NATIVES:%=src/natives/%.c)

DBG_DIR  = build/debug
REL_DIR  = build/release
DBG_OBJS = $(SRCS:src/%.c=$(DBG_DIR)/%.o)
REL_OBJS = $(SRCS:src/%.c=$(REL_DIR)/%.o)

# make         -> ./pycee         (dev: -g + sanitizers)
# make release -> ./pycee-release (-O2)
all: $(NAME)

release: $(NAME)-release

$(NAME): $(DBG_OBJS)
	$(CC) $(CFLAGS) $(DBG_FLAGS) $^ $(LIBS) -o $@

$(NAME)-release: $(REL_OBJS)
	$(CC) $(CFLAGS) $(REL_FLAGS) $^ $(LIBS) -o $@

# Objects mirror src/ (build/debug/front/lexer.o...): each rule makes its directory.
$(DBG_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DBG_FLAGS) -c $< -o $@

$(REL_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(REL_FLAGS) -c $< -o $@

clean:
	rm -rf build

fclean: clean
	rm -f $(NAME) $(NAME)-release

re: fclean all

test: $(NAME)
	sh tests/run_tests.sh
	sh tests/check_members.sh
	sh tests/check_signals.sh

# Every test again, collecting on every allocation (with the sanitizers,
# a value left out of the roots is a use-after-free at once).
stress: $(NAME)
	PYCEE_GC_STRESS=1 sh tests/run_tests.sh

-include $(DBG_OBJS:.o=.d) $(REL_OBJS:.o=.d)

.PHONY: stress all release clean fclean re test
