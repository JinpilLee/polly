This repository hosts the sources of Polly for FPGA based on Polly 5.0.0.  
How to build Polly for SPGen  
1.a download LLVM from:  
http://releases.llvm.org/5.0.0/llvm-5.0.0.src.tar.xz  
1.b download Clang from:  
http://releases.llvm.org/5.0.0/cfe-5.0.0.src.tar.xz  
2. make a workspace, for example:  
mkdir $HOME/LLVM  
3.a extract the LLVM file at $HOME/LLVM  
this will generate $HOME/LLVM/llvm-5.0.0-src  
3.b extract the Clang file at $HOME/LLVM/llvm-5.0.0-src/tools  
this will generate $HOME/LLVM/llvm-5.0.0-src/tools/cfe-5.0.0  
3.c rename cfe-5.0.0 to clang  
this will generate $HOME/LLVM/llvm-5.0.0-src/tools/clang  
4. clone this repository at $HOME/LLVM/llvm-5.0.0-src/tools  
this will generate $HOME/LLVM/llvm-5.0.0-src/tools/polly  
5. make a build directory for LLVM:  
mkdir $(HOME)/LLVM/build  
6. prepare build at $(HOME)/LLVM/build  
cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX={LLVM_INSTALL_PATH_YOU_WANT} ../llvm-5.0.0.src/  
7. make and make install  
e.g.) make -j32 install  
8. set PATH, LD_LIBRARY_PATH for LLVM, for example:  
  export PATH=$(LLVM_PATH)/bin:$PATH  
  export LD_LIBRARY_PATH=$(LLVM_PATH)/lib:$LD_LIBRARY_PATH  

How to generate SPDFile (compile option)  
clang -O3 -fno-inline -c -mllvm -polly -mllvm -polly-enable-spdgen test.c

TODO:
mirror from git repo
