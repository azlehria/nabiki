TARGET		:= nabiki

OBJDIR		:= build
OUTDIR		:= dist

CC			= gcc $(CPPFLAGS) $(CFLAGS) -c
CXX			= g++ $(CPPFLAGS) $(CXXFLAGS) -c

CPATH		:= /usr/local/include:.:$(CPATH)
CPPFLAGS	+= -DNDEBUG -DJSON_STRIP_COMMENTS -DSPH_KECCAK_64=1 -DSPH_KECCAK_UNROLL=4 -DSPH_KECCAK_NOCOPY -DCURL_NO_OLDIES -DNO_SSL -DNO_CACHING -DMAX_WORKER_THREADS=2
CFLAGS		+= -O3 -m64 -Wall -Wextra -Wno-unused-parameter -pthread -fPIC -fno-omit-frame-pointer -static-libstdc++ -static-libgcc
CXXFLAGS	+= $(CFLAGS) -std=c++17 -fno-rtti

LD			= g++ $(LDFLAGS)
LDFLAGS		+= $(CPPFLAGS) $(CXXFLAGS) -rdynamic -L/usr/local/cuda/lib64 -L/usr/local/cuda/lib64/stubs
LD_LIBS		+= -lcudart_static -lcrypto -lcurl -lOpenCL -ldl -lrt -lnvidia-ml

NVCC		= nvcc $(NVCCFLAGS)
NVCCFLAGS 	+= -std=c++14 -m64 -Xcompiler="$(CPPFLAGS) $(CFLAGS) -fno-rtti" $(nvcc_ARCH) -c
nvcc_ARCH 	+= -gencode=arch=compute_70,code=\"sm_70,compute_70\"
nvcc_ARCH 	+= -gencode=arch=compute_61,code=\"sm_61,compute_61\"
nvcc_ARCH 	+= -gencode=arch=compute_52,code=\"sm_52,compute_52\"
nvcc_ARCH 	+= -gencode=arch=compute_50,code=\"sm_50,compute_50\"
nvcc_ARCH 	+= -gencode=arch=compute_35,code=\"sm_35,compute_35\"
nvcc_ARCH 	+= -gencode=arch=compute_30,code=\"sm_30,compute_30\"

CC_SRCS		:= $(wildcard *.cpp) $(notdir $(wildcard BigInt/*.cpp))
C_SRCS		:= $(wildcard *.c) $(notdir $(wildcard CivetWeb/*.c))
CU_SRCS		:= $(wildcard *.cu)
OBJS		:= $(CU_SRCS:%.cu=$(OBJDIR)/%.cu.o) $(CC_SRCS:%.cpp=$(OBJDIR)/%.o) $(C_SRCS:%.c=$(OBJDIR)/%.o)

all: $(TARGET)

package: distfiles
	@tar cJf $(TARGET)-linux-$(shell grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' hybridminer.h).tar.xz -C dist .

distfiles: all
	cp $(TARGET).json $(OUTDIR)
	cp LICENSE $(OUTDIR)
	cp README.txt $(OUTDIR)
	cp $(TARGET) $(OUTDIR)

$(TARGET): $(OBJS) | $(OUTDIR)
	$(LD) -o $@ $^ $(LD_LIBS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OUTDIR):
	mkdir -p $(OUTDIR)

clean:
	rm -f $(TARGET)
	rm -f *.xz
	rm -rf $(OBJDIR)

distclean: clean
	rm -rf $(OUTDIR)

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $< -o $@

$(OBJDIR)/%.o: BigInt/%.cpp | $(OBJDIR)
	$(CXX) $< -o $@

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $< -o $@

$(OBJDIR)/%.o: CivetWeb/%.c | $(OBJDIR)
	$(CC) $< -o $@

$(OBJDIR)/%.cu.o: %.cu | $(OBJDIR)
	$(NVCC) $< -o $@

.PHONY: all clean distclean
