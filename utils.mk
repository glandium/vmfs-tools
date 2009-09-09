#Usage: $(call checking,message])
checking = $(shell sh -c "echo -n Checking for $(1)... " >&2)

#Usage: $(call result,value)
result = $(shell echo $(if $(1),yes,no) >&2)

#Usage: $(call PATH_LOOKUP,name)
# Sets NAME to the full path of name
define _PATH_LOOKUP
__path_lookup := $(call UC,$(1))
ifndef __$$(__path_lookup)
$$(call checking,$(1))
$$(__path_lookup) := $$(firstword $$(wildcard $$(foreach path,$$(subst :, ,$(PATH)),$$(path)/$(1))))
__$$(__path_lookup) := 1
$$(call result,$$($$(__path_lookup)))
endif
endef
PATH_LOOKUP = $(eval $(call _PATH_LOOKUP,$(1)))

ALPHA := A B C D E F G H I J K L M N O P Q R S T U V W X Y Z _
alpha := a b c d e f g h i j k l m n o p q r s t u v w x y z -
alphaALPHA := $(join $(addsuffix /,$(alpha)), $(ALPHA))

UC = $(strip $(eval __uc := $1) \
             $(foreach c,$(alphaALPHA), \
                $(eval __uc := \
                   $(subst $(subst /,,$(dir $(c))),$(notdir $(c)),$(__uc)))) \
             $(__uc))

#Usage: $(call PKG_CONFIG_CHK,name,cflags,ldflags)
# cflags and ldflags used in case pkg-config fails.
# Sets NAME_LDFLAGS and NAME_CFLAGS
define _PKG_CONFIG_CHK
$$(call PATH_LOOKUP,pkg-config)
$$(call checking,$(1))
__name := $(call UC,$(1))
$$(__name)_LDFLAGS := $$(if $$(PKG_CONFIG),$$(strip $$(shell $$(PKG_CONFIG) --libs $(1) 2> /dev/null || echo __)),__)
$$(__name)_CFLAGS := $$(if $$(PKG_CONFIG),$$(strip $$(shell $$(PKG_CONFIG) --cflags $(1) 2> /dev/null || echo __)),__)
ifeq (__,$$($$(__name)_LDFLAGS))
$$(__name)_LDFLAGS := $(3)
endif
ifeq (__,$$($$(__name)_CFLAGS))
$$(__name)_CFLAGS := $(2)
endif
$$(call result,$$($$(__name)_CFLAGS)$$($$(__name)_LDFLAGS))
endef
PKG_CONFIG_CHK = $(eval $(call _PKG_CONFIG_CHK,$(1),$(2),$(3)))

#Usage: $(call LINK_CHECK,func,ldflags)
# Try to link a simple program with a call to func() with given ldflags
# Sets FUNC_LDFLAGS and HAS_FUNC
define _LINK_CHECK
$$(call checking,$(if $(2),$(1) in $(2),$(1)))
__name := $(call UC,$(1))
__$$(__name) := $$(shell echo "char $(1)(); int main(void) { $(1)(); return(0); }" > __conftest.c; $(CC) -o __conftest __conftest.c $(2) 2> /dev/null && echo yes || echo no; rm -f __conftest*)
ifeq ($$(__$$(__name)),yes)
HAS_$$(__name) := 1
ifneq (,$(2))
$$(__name)_LDFLAGS := $(2)
endif
endif
$$(call result,$$(HAS_$$(__name)))
endef
LINK_CHECK = $(eval $(call _LINK_CHECK,$(1),$(2)))

GEN_VERSION = $(shell \
	(if [ -d .git ]; then \
		VER=$$(git describe --match "v[0-9].*" --abbrev=0 HEAD); \
		git update-index -q --refresh; \
		if git diff-index --name-status $${VER} -- | grep -q -v '^A'; then \
			VER=$$(git describe --match "v[0-9].*" --abbrev=4 HEAD | sed s/-/./g); \
		fi ; \
		if [ -z "$${VER}" ]; then \
			VER="v0.0.0.$$(git rev-list HEAD | wc -l).$$(git rev-parse --short=4 HEAD)"; \
		fi; \
		test -z "$$(git diff-index --name-only HEAD --)" || VER=$${VER}-dirty; \
        else \
		VER=$(patsubst %,%-patched,$(or $(VERSION:%-patched=%),v0.0.0)); \
	fi; \
	echo $$VER) 2> /dev/null)
