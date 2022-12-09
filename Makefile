CLCPP=bip39cl.cpp
OUTPUT=bip39cl
CXX=g++
CXXFLAGS=-O2 -std=c++11
OPENCL_VERSION=120
OPENCL_INCLUDE=/usr/local/cuda/include
OPENCL_LIBS=/usr/local/cuda/lib64
LIBS=-lOpenCL

all:	
	./embedcl bip39.cl bip39cl.cpp _bip39_cl
	${CXX} ${CXXFLAGS} cl_util.cpp util.cpp bip39cl.cpp bip39opencl.cpp main.cpp -o ${OUTPUT} -L${OPENCL_LIBS} -L${OPENCL_INCLUDE} ${LIBS} -DCL_TARGET_OPENCL_VERSION=${OPENCL_VERSION}

clean:
	rm -rf ${OUTPUT} ${CLCPP}