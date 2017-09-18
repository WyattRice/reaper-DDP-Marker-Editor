CFLAGS = -O -DSWELL_PROVIDED_BY_APP
CXXFLAGS = $(CFLAGS)

ARCH = $(shell uname -m)
UNAME_S = $(shell uname -s)



ifeq ($(UNAME_S), Darwin) 
  DLL_EXT=.dylib
  SWELL_OBJS = swell-modstub.o
  LINKEXTRA = -framework Cocoa

%.o : %.mm
	$(CXX) $(CXXFLAGS) -c -o $@ $^
else
  DLL_EXT=.so
  SWELL_OBJS = swell-modstub-generic.o
endif

vpath %.cpp ../WDL/swell
vpath %.mm ../WDL/swell

DLL=reaper_ddpedit$(DLL_EXT)

.phony: default

default: $(DLL)


res.rc_mac_dlg: res.rc
	php ../WDL/swell/mac_resgen.php res.rc

editmarkers.o: res.rc_mac_dlg

$(DLL): editmarkers.o $(SWELL_OBJS)
	$(CXX) -o $@ $(CXXFLAGS) $^ -shared $(LINKEXTRA)
