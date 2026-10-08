TOPTARGETS := all clean

# SUBDIRS := $(wildcard */.)
# SUBDIRS := twilighte
SUBDIRS := ch376 ch395 debug ds1501 register skel stack twilighte skel_bank skel_romdis skel_clock

$(TOPTARGETS): $(SUBDIRS)

all: $(SUBDIRS)
$(SUBDIRS): force_look
	$(MAKE) -C $@ $(MAKECMDGOALS)

force_look:
	true

.PHONY: $(TOPTARGETS) $(SUBDIRS)
