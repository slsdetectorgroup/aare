CXX      := /usr/bin/c++
NVCC     := nvcc
ARCH     := -arch=sm_89 
CXXFLAGS := -std=c++17 -O3 --extended-lambda -ccbin $(CXX)
INCLUDES := -I../include -I../build/_deps/fmt-src/include
LDFLAGS  := -L../build -L../build/_deps/fmt-build
LIBS     := -laare_core -lfmt -lstdc++fs
DEFINES  := -DAARE_LOG_LEVEL=logERROR

TARGET   := test_cf_cuda
SRC      := ClusterFinderCUDA.test.cu
DEP      := $(SRC:.cu=.d)

all: $(TARGET)

$(TARGET): $(SRC) ../include/aare/clusterfinder_kernel.cuh
	$(NVCC) -Xptxas=-v $(ARCH) $(CXXFLAGS) $(DEFINES) $(INCLUDES) $(LDFLAGS) $< -o $@ $(LIBS)

clean:
	rm -f $(TARGET) $(DEP)

-include $(DEP)

.PHONY: all clean