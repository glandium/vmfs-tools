PATH_LOOKUP = $(wildcard $(foreach path,$(subst :, ,$(PATH)),$(path)/$(1)))

ALPHA := A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
alpha := a b c d e f g h i j k l m n o p q r s t u v w x y z
alphaALPHA := $(join $(addsuffix /,$(alpha)), $(ALPHA))

UC = $(strip $(eval __uc := $1) \
             $(foreach c,$(alphaALPHA), \
                $(eval __uc := \
                   $(subst $(subst /,,$(dir $(c))),$(notdir $(c)),$(__uc)))) \
             $(__uc))

#Usage: $(call PKG_CONFIG_CHK,name,cflags,ldflags)
# cflags and ldflags used in case pkg-config fails.
# Sets NAME_LDFLAGS and NAME_CFLAGS
PKG_CONFIG := $(call PATH_LOOKUP,pkg-config)
define _PKG_CONFIG_CHK
__name := $(call UC,$(1))
$$(__name)_LDFLAGS := $(if $(PKG_CONFIG),$$(strip $$(shell $$(PKG_CONFIG) --libs $(1) 2> /dev/null || echo __)),__)
$$(__name)_CFLAGS := $(if $(PKG_CONFIG),$$(strip $$(shell $$(PKG_CONFIG) --cflags $(1) 2> /dev/null || echo __)),__)
ifeq (__,$$($$(__name)_LDFLAGS))
$$(__name)_LDFLAGS := $(3)
endif
ifeq (__,$$($$(__name)_CFLAGS))
$$(__name)_CFLAGS := $(2)
endif
endef
PKG_CONFIG_CHK = $(eval $(call _PKG_CONFIG_CHK,$(1),$(2),$(3)))

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
